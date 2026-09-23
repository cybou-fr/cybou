// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_OPERATION_H
#define CYBOU_PROTOCOL_OPERATION_H

#include <cybou/account_creation.h>

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace cybou {

inline constexpr uint8_t PROTOCOL_OPERATION_VERSION{1};

enum class ProtocolOperationType : uint8_t {
    ACCOUNT_CREATE = 1,
};

using ProtocolOperationPayloadV1 = std::variant<AccountCreateOpV1>;

struct ProtocolOperationV1 {
    uint8_t version{PROTOCOL_OPERATION_VERSION};
    ProtocolOperationPayloadV1 payload;

    ProtocolOperationV1() = default;
    ProtocolOperationV1(AccountCreateOpV1 operation) : payload{std::move(operation)} {}

    friend bool operator==(const ProtocolOperationV1&, const ProtocolOperationV1&) = default;
};

ProtocolOperationType OperationType(const ProtocolOperationV1& operation);
std::vector<unsigned char> SerializeProtocolOperation(const ProtocolOperationV1& operation);
std::optional<ProtocolOperationV1> DeserializeProtocolOperation(std::span<const unsigned char> bytes);

} // namespace cybou

#endif // CYBOU_PROTOCOL_OPERATION_H
