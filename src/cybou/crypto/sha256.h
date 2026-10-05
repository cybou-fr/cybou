// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Русский публичный API потокового SHA-256 на базе OpenSSL EVP.

#ifndef CYBOU_CRYPTO_SHA256_H
#define CYBOU_CRYPTO_SHA256_H

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <string_view>

namespace cybou::crypto {

/// \brief Потоковый SHA-256 с тем же байтовым результатом, что и одноразовый подсчет.
class Sha256 final
{
public:
    static constexpr std::size_t OUTPUT_SIZE{32};

    /// \brief Создает новый хеш-контекст SHA-256.
    /// \throw std::runtime_error При сбое инициализации OpenSSL EVP.
    Sha256();
    ~Sha256();

    Sha256(Sha256&&) noexcept;
    Sha256& operator=(Sha256&&) noexcept;
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;

    /// \brief Добавляет очередной непрерывный фрагмент данных.
    /// \param data Указатель на байты; может быть `nullptr` только при `size == 0`.
    /// \param size Длина фрагмента в байтах.
    /// \return `*this` для цепочек вызовов.
    /// \pre `Finalize()` для этого объекта еще не вызывался.
    /// \throw std::runtime_error При нарушении предусловий или сбое OpenSSL EVP.
    Sha256& Write(const unsigned char* data, std::size_t size);
    /// \brief Завершает хеширование и пишет 32-байтовый digest в буфер caller'а.
    /// \param output Буфер длиной не менее `OUTPUT_SIZE`.
    /// \pre `Finalize()` для этого объекта еще не вызывался.
    /// \post После успешного вызова объект завершен и больше не принимает `Write()`.
    /// \throw std::runtime_error При нарушении предусловий или сбое OpenSSL EVP.
    void Finalize(unsigned char* output);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/// \brief Считает SHA-256 по упорядоченному набору фрагментов.
/// \param parts Фрагменты, логически конкатенируемые в указанном порядке.
/// \param output Выходной буфер длиной не менее `Sha256::OUTPUT_SIZE`.
/// \return false только при сбое OpenSSL-провайдера; выходной буфер тогда очищается.
/// \post При `false` содержимое `output` очищено fail-closed.
bool ComputeSha256(std::initializer_list<std::span<const unsigned char>> parts, unsigned char* output) noexcept;

/// \brief Представляет байты строки как span без перекодирования.
/// \param text Исходная строка.
/// \return View на те же байты без копирования и без изменения lifetime.
std::span<const unsigned char> Sha256Bytes(std::string_view text) noexcept;

} // namespace cybou::crypto

#endif // CYBOU_CRYPTO_SHA256_H
