// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Relay proof-of-work каждой пользовательской операции (DEC-273).
/// \details PoW — условие допуска в candidate pool и mesh relay на каждом Full Node,
///          включая PoA. Он путешествует вместе с exact bytes операции до финализации и
///          не входит в блок: finalized history его не хранит и не переисполняет.

#ifndef CYBOU_OPERATION_WORK_H
#define CYBOU_OPERATION_WORK_H

#include <cybou/hash256.h>
#include <cybou/protocol_operation.h>
#include <cybou/state.h>

#include <cstdint>
#include <optional>
#include <stop_token>

namespace cybou {

/// \brief SHA-256("CYBOU/OP-WORK" || NetworkBinding || OperationID || nonce LE64).
cybou::Hash256 ComputeOperationWorkHash(const cybou::Hash256& network_binding,
    const cybou::Hash256& operation_id, uint64_t nonce);

/// \brief true, если хэш работы имеет не меньше `required_bits` ведущих нулевых бит.
bool CheckOperationWork(const cybou::Hash256& network_binding, const cybou::Hash256& operation_id,
    uint64_t nonce, uint32_t required_bits);

/// \brief Подбирает nonce; `std::nullopt` при остановке или `required_bits` > 64.
std::optional<uint64_t> SolveOperationWork(const cybou::Hash256& network_binding,
    const cybou::Hash256& operation_id, uint32_t required_bits, std::stop_token stop = {});

/// \brief Сложность relay-PoW операции по уровню AUTH авторизующей Identity в finalized state.
/// \return 0 для операций со своей защитой: AccountCreate (consensus PoW) и PoaAuthAdjustment
///         (подпись genesis PoA key). Name-операции дороже на `NAME_OPERATION_EXTRA_WORK_BITS`.
uint32_t RequiredOperationWorkBits(const ProtocolOperation& operation, const CybouState& finalized);

} // namespace cybou

#endif // CYBOU_OPERATION_WORK_H
