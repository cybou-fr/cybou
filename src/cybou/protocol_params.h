// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_PARAMS_H
#define CYBOU_PROTOCOL_PARAMS_H

/// \file
/// \brief Immutable protocol parameters, зафиксированные в signed NetworkGenesis.

#include <cstddef>
#include <cstdint>

namespace cybou {

/// \brief DEV onboarding credit в spendable CYBOU на одно успешное создание аккаунта.
inline constexpr uint64_t DEV_ONBOARDING_BONUS{6000};
/// \brief Максимум AccountCreate в одном finalized block, шт.
inline constexpr uint32_t DEFAULT_MAX_ACCOUNT_CREATES_PER_BLOCK{100};
/// \brief Требуемая сложность PoW для AccountCreate, в leading zero bits хэша работы.
inline constexpr uint32_t DEFAULT_ACCOUNT_CREATION_WORK_BITS{25};
/// \brief Минимальное число эпох между подготовкой и допустимым включением AccountCreate.
inline constexpr uint64_t DEFAULT_ACCOUNT_CREATION_EPOCH_LAG{1};
/// \brief Размер эпохи в finalized blocks.
inline constexpr uint64_t DEFAULT_EPOCH_BLOCKS{1024};
/// \brief Плоская комиссия Payment в целых CYBOU.
inline constexpr uint64_t DEFAULT_PAYMENT_FEE{1};
/// \brief Комиссия RootPublication за каждый начатый KiB serialized payload, CYBOU/KiB.
inline constexpr uint64_t DEFAULT_ROOT_PUBLICATION_FEE_PER_STARTED_KIB{4};
/// \brief Комиссия RootPublication за каждый авторизуемый чанк, CYBOU/chunk.
inline constexpr uint64_t DEFAULT_ROOT_PUBLICATION_FEE_PER_CHUNK{4};
/// \brief Требуемая сложность PoW для name claim, в leading zero bits.
inline constexpr uint32_t DEFAULT_NAME_CLAIM_WORK_BITS{16};
/// \brief Минимальная глубина между commit и reveal имени, в finalized blocks.
inline constexpr uint64_t DEFAULT_NAME_COMMIT_MIN_DEPTH{1};
/// \brief Максимальное время жизни pending name commit, в finalized blocks.
inline constexpr uint64_t DEFAULT_NAME_COMMIT_MAX_LIFETIME{1000};
/// \brief Верхняя граница числа одновременно живых pending name commits.
inline constexpr uint32_t DEFAULT_MAX_PENDING_NAME_COMMITS{10000};

/// \brief Immutable consensus parameters, одинаковые для каждого Full Node этой сети.
struct CybouProtocolParameters {
    /// \brief Сложность AccountCreate work, в битах.
    uint32_t account_creation_work_bits{DEFAULT_ACCOUNT_CREATION_WORK_BITS};
    /// \brief Задержка AccountCreate в эпохах между вычислением work и включением.
    uint64_t account_creation_epoch_lag{DEFAULT_ACCOUNT_CREATION_EPOCH_LAG};
    /// \brief Максимум AccountCreate на один finalized block.
    uint32_t max_account_creates_per_block{DEFAULT_MAX_ACCOUNT_CREATES_PER_BLOCK};
    /// \brief Начальный spendable credit после успешного AccountCreate, в CYBOU.
    uint64_t onboarding_bonus{DEV_ONBOARDING_BONUS};
    /// \brief Длина эпохи в finalized blocks.
    uint64_t epoch_blocks{DEFAULT_EPOCH_BLOCKS};
    /// \brief Фиксированная комиссия Payment, в CYBOU.
    uint64_t payment_fee{DEFAULT_PAYMENT_FEE};
    /// \brief Компонент комиссии RootPublication за каждый начатый KiB payload, в CYBOU.
    uint64_t root_publication_fee_per_started_kib{DEFAULT_ROOT_PUBLICATION_FEE_PER_STARTED_KIB};
    /// \brief Компонент комиссии RootPublication за каждый чанк Merkle-дерева, в CYBOU.
    uint64_t root_publication_fee_per_chunk{DEFAULT_ROOT_PUBLICATION_FEE_PER_CHUNK};
    /// \brief Сложность name claim work, в битах.
    uint32_t name_claim_work_bits{DEFAULT_NAME_CLAIM_WORK_BITS};
    /// \brief Минимальное число finalized blocks между name commit и reveal.
    uint64_t name_commit_min_depth{DEFAULT_NAME_COMMIT_MIN_DEPTH};
    /// \brief Максимальное число finalized blocks, после которого pending commit истекает.
    uint64_t name_commit_max_lifetime{DEFAULT_NAME_COMMIT_MAX_LIFETIME};
    /// \brief Максимум одновременно удерживаемых pending name commits.
    uint32_t max_pending_name_commits{DEFAULT_MAX_PENDING_NAME_COMMITS};
    /// \brief Разрешает XWing KEM в протоколе Identity этой сети.
    bool identity_kem_xwing_enabled{false};

    friend bool operator==(const CybouProtocolParameters&, const CybouProtocolParameters&) = default;
};

/// \brief Возвращает фиксированный набор DEV protocol parameters.
/// \return Полный immutable набор, подходящий для офлайн сборки DEVNET genesis.
constexpr CybouProtocolParameters DevProtocolParameters()
{
    CybouProtocolParameters params{};
    params.identity_kem_xwing_enabled = true;
    return params;
}

/// \brief Canonical derivation эпохи из finalized block height и immutable parameters.
/// \param block_height Высота finalized block, для которой требуется epoch index.
/// \param params Immutable protocol parameters текущей сети.
/// \return Индекс эпохи; при epoch_blocks == 0 возвращает 0 как защиту от деления на ноль.
constexpr uint64_t EpochForHeight(const uint64_t block_height, const CybouProtocolParameters& params)
{
    return params.epoch_blocks == 0 ? 0 : block_height / params.epoch_blocks;
}

} // namespace cybou

#endif // CYBOU_PROTOCOL_PARAMS_H
