// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_BOOTSTRAP_PROTOCOL_H
#define CYBOU_BOOTSTRAP_PROTOCOL_H

#include <cybou/bootstrap_store.h>
#include <cybou/p2p/session.h>

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cybou {

enum class BootstrapRequestKind : uint8_t { STATUS = 1, CLAIM = 2, REPLACE = 3 };
enum class BootstrapResponseStatus : uint8_t {
    EMPTY = 0, BOUND = 1, CLAIMED = 2, REPLACED = 3, ALREADY_BOUND = 4,
    INVALID_ACTIVATION_CODE = 5, INVALID_BINDING = 6, INVALID_REPLACEMENT = 7,
    STORAGE_ERROR = 8, INVALID_REQUEST = 9,
};

struct BootstrapRequest {
    BootstrapRequestKind kind{BootstrapRequestKind::STATUS};
    std::string activation_code;
    std::optional<BootstrapNetworkBinding> binding;
    std::optional<BootstrapNetworkBinding> previous_binding;
    std::optional<BootstrapNetworkReplacement> replacement;
};

struct BootstrapResponse {
    BootstrapResponseStatus status{BootstrapResponseStatus::INVALID_REQUEST};
    std::optional<BootstrapNetworkBinding> binding;
};

std::optional<std::vector<unsigned char>> EncodeBootstrapRequest(const BootstrapRequest& request);
std::optional<BootstrapRequest> DecodeBootstrapRequest(std::span<const unsigned char> bytes,
    const std::optional<BootstrapNetworkBinding>& current_binding = std::nullopt);
std::optional<std::vector<unsigned char>> EncodeBootstrapResponse(const BootstrapResponse& response);
std::optional<BootstrapResponse> DecodeBootstrapResponse(std::span<const unsigned char> bytes);

class BootstrapProtocolHandler {
public:
    explicit BootstrapProtocolHandler(BootstrapStore& store) : m_store{store} {}
    std::optional<p2p::Frame> Handle(const p2p::Frame& frame);
private:
    BootstrapStore& m_store;
};

} // namespace cybou
#endif
