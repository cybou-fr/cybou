// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_OPERATION_V2_H
#define CYBOU_PROTOCOL_OPERATION_V2_H

#include <cybou/account_creation_v2.h>
#include <cybou/payment_v2.h>

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace cybou {

inline constexpr uint8_t PROTOCOL_OPERATION_VERSION_V2{2};
inline constexpr size_t AUTHORIZED_PAYMENT_V2_SIZE{2574};

enum class ProtocolOperationKindV2 : uint8_t { ACCOUNT_CREATE = 1, PAYMENT = 2 };

using ProtocolOperationV2 = std::variant<AccountCreateOpV2, AuthorizedPaymentV2>;

std::optional<std::vector<unsigned char>> SerializeProtocolOperationV2(const ProtocolOperationV2& operation);
std::optional<ProtocolOperationV2> DeserializeProtocolOperationV2(std::span<const unsigned char> bytes);
std::optional<uint256> ComputeOperationIdV2(const ProtocolOperationV2& operation);

} // namespace cybou
#endif
