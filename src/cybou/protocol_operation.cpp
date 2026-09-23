// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/protocol_operation.h>

namespace cybou {

ProtocolOperationType OperationType(const ProtocolOperationV1& operation)
{
    return std::visit([](const auto&) { return ProtocolOperationType::ACCOUNT_CREATE; }, operation.payload);
}

std::vector<unsigned char> SerializeProtocolOperation(const ProtocolOperationV1& operation)
{
    std::vector<unsigned char> out{operation.version, static_cast<uint8_t>(OperationType(operation))};
    const auto payload{std::visit([](const auto& value) { return SerializeAccountCreateOp(value); }, operation.payload)};
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

std::optional<ProtocolOperationV1> DeserializeProtocolOperation(const std::span<const unsigned char> bytes)
{
    if (bytes.size() < 2 || bytes[0] != PROTOCOL_OPERATION_VERSION) return std::nullopt;
    if (bytes[1] != static_cast<uint8_t>(ProtocolOperationType::ACCOUNT_CREATE)) return std::nullopt;
    const auto account_create{DeserializeAccountCreateOp(bytes.subspan(2))};
    if (!account_create) return std::nullopt;
    return ProtocolOperationV1{*account_create};
}

} // namespace cybou
