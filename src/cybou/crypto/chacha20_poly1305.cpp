// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/crypto/chacha20_poly1305.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <limits>
#include <memory>

namespace cybou::crypto {
namespace {

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
unsigned char EMPTY_OCTET{0};

const unsigned char* DataOrSentinel(const std::span<const unsigned char> data)
{
    return data.empty() ? &EMPTY_OCTET : data.data();
}

unsigned char* OutputOrSentinel(const std::span<unsigned char> data)
{
    return data.empty() ? const_cast<unsigned char*>(&EMPTY_OCTET) : data.data();
}

void Cleanse(const std::span<unsigned char> data)
{
    if (!data.empty()) OPENSSL_cleanse(data.data(), data.size());
}

} // namespace

bool ChaCha20Poly1305Encrypt(
    const std::span<const unsigned char, CHACHA20_POLY1305_KEY_SIZE> key,
    const std::span<const unsigned char, CHACHA20_POLY1305_NONCE_SIZE> nonce,
    const std::span<const unsigned char> associated_data,
    const std::span<const unsigned char> plaintext,
    const std::span<unsigned char> ciphertext_and_tag)
{
    constexpr std::size_t TAG_SIZE{CHACHA20_POLY1305_TAG_SIZE};
    constexpr auto INT_MAX_SIZE = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (associated_data.size() > INT_MAX_SIZE || plaintext.size() > INT_MAX_SIZE ||
        ciphertext_and_tag.size() < TAG_SIZE || ciphertext_and_tag.size() - TAG_SIZE != plaintext.size()) {
        Cleanse(ciphertext_and_tag);
        return false;
    }

    CipherContext context{EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free};
    if (!context || EVP_EncryptInit_ex2(context.get(), EVP_chacha20_poly1305(), key.data(), nonce.data(), nullptr) != 1) {
        Cleanse(ciphertext_and_tag);
        return false;
    }

    int written{0};
    if (!associated_data.empty() && EVP_EncryptUpdate(
            context.get(), nullptr, &written, DataOrSentinel(associated_data), static_cast<int>(associated_data.size())) != 1) {
        Cleanse(ciphertext_and_tag);
        return false;
    }

    int ciphertext_size{0};
    if (!plaintext.empty() && EVP_EncryptUpdate(
            context.get(), OutputOrSentinel(ciphertext_and_tag.first(plaintext.size())), &ciphertext_size,
            DataOrSentinel(plaintext), static_cast<int>(plaintext.size())) != 1) {
        Cleanse(ciphertext_and_tag);
        return false;
    }

    int final_size{0};
    auto* output = OutputOrSentinel(ciphertext_and_tag);
    if (EVP_EncryptFinal_ex(context.get(), output + ciphertext_size, &final_size) != 1 ||
        static_cast<std::size_t>(ciphertext_size + final_size) != plaintext.size() ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_AEAD_GET_TAG, static_cast<int>(TAG_SIZE),
            output + plaintext.size()) != 1) {
        Cleanse(ciphertext_and_tag);
        return false;
    }
    return true;
}

bool ChaCha20Poly1305Decrypt(
    const std::span<const unsigned char, CHACHA20_POLY1305_KEY_SIZE> key,
    const std::span<const unsigned char, CHACHA20_POLY1305_NONCE_SIZE> nonce,
    const std::span<const unsigned char> associated_data,
    const std::span<const unsigned char> ciphertext_and_tag,
    const std::span<unsigned char> plaintext)
{
    constexpr std::size_t TAG_SIZE{CHACHA20_POLY1305_TAG_SIZE};
    constexpr auto INT_MAX_SIZE = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (ciphertext_and_tag.size() < TAG_SIZE || ciphertext_and_tag.size() - TAG_SIZE > INT_MAX_SIZE ||
        associated_data.size() > INT_MAX_SIZE || plaintext.size() != ciphertext_and_tag.size() - TAG_SIZE) {
        Cleanse(plaintext);
        return false;
    }

    CipherContext context{EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free};
    if (!context || EVP_DecryptInit_ex2(context.get(), EVP_chacha20_poly1305(), key.data(), nonce.data(), nullptr) != 1) {
        Cleanse(plaintext);
        return false;
    }

    int written{0};
    if (!associated_data.empty() && EVP_DecryptUpdate(
            context.get(), nullptr, &written, DataOrSentinel(associated_data), static_cast<int>(associated_data.size())) != 1) {
        Cleanse(plaintext);
        return false;
    }

    const auto ciphertext = ciphertext_and_tag.first(ciphertext_and_tag.size() - TAG_SIZE);
    int plaintext_size{0};
    if (!ciphertext.empty() && EVP_DecryptUpdate(
            context.get(), OutputOrSentinel(plaintext), &plaintext_size,
            DataOrSentinel(ciphertext), static_cast<int>(ciphertext.size())) != 1) {
        Cleanse(plaintext);
        return false;
    }

    auto* tag = const_cast<unsigned char*>(ciphertext_and_tag.data() + ciphertext.size());
    if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_AEAD_SET_TAG, static_cast<int>(TAG_SIZE), tag) != 1) {
        Cleanse(plaintext);
        return false;
    }

    int final_size{0};
    unsigned char final_output[EVP_MAX_BLOCK_LENGTH]{};
    const int final_ok = EVP_DecryptFinal_ex(context.get(), final_output, &final_size);
    OPENSSL_cleanse(final_output, sizeof(final_output));
    if (final_ok != 1 || final_size != 0 || static_cast<std::size_t>(plaintext_size) != plaintext.size()) {
        Cleanse(plaintext);
        return false;
    }
    return true;
}

} // namespace cybou::crypto
