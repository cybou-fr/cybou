// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/recovery_phrase.h>

#include <openssl/rand.h>
#include <openssl/sha.h>

#include <algorithm>
#include <array>

namespace cybou {
namespace {
// Source: bitcoin/bips bip-0039/english.txt, SHA-256
// 2f5eed53a4727b4bf8880d8f3f199efc90e58503646d9ff8eff3a2ed3b24dbda.
// The BIP is licensed MIT. Word order is part of the recovery format.
constexpr std::array<std::string_view, 2048> WORDS{
#include <cybou/bip39_english.inc>
};

unsigned char GetBit(const unsigned char* bytes, size_t position)
{
    return (bytes[position / 8] >> (7 - position % 8)) & 1;
}

void SetBit(unsigned char* bytes, size_t position, unsigned char value)
{
    bytes[position / 8] |= value << (7 - position % 8);
}
} // namespace

std::optional<RecoveryEntropy> GenerateRecoveryEntropy()
{
    RecoveryEntropy entropy{};
    if (RAND_bytes(entropy.data(), entropy.size()) != 1) return std::nullopt;
    return entropy;
}

RecoveryWords EncodeRecoveryWords(const RecoveryEntropy& entropy)
{
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};
    SHA256(entropy.data(), entropy.size(), hash.data());
    RecoveryWords result{};
    for (size_t word{0}; word < result.size(); ++word) {
        unsigned index{0};
        for (size_t bit{0}; bit < 11; ++bit) {
            const size_t position{word * 11 + bit};
            index = (index << 1) | (position < 256 ? GetBit(entropy.data(), position)
                                                   : GetBit(hash.data(), position - 256));
        }
        result[word] = WORDS[index];
    }
    return result;
}

std::optional<RecoveryEntropy> DecodeRecoveryWords(const RecoveryWords& words)
{
    std::array<unsigned char, 33> bits{};
    for (size_t word{0}; word < words.size(); ++word) {
        const auto found = std::lower_bound(WORDS.begin(), WORDS.end(), words[word]);
        if (found == WORDS.end() || *found != words[word]) return std::nullopt;
        const unsigned index = static_cast<unsigned>(found - WORDS.begin());
        for (size_t bit{0}; bit < 11; ++bit) {
            SetBit(bits.data(), word * 11 + bit, (index >> (10 - bit)) & 1);
        }
    }
    RecoveryEntropy entropy{};
    std::copy_n(bits.begin(), entropy.size(), entropy.begin());
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};
    SHA256(entropy.data(), entropy.size(), hash.data());
    if (bits[32] != hash[0]) return std::nullopt;
    return entropy;
}

} // namespace cybou
