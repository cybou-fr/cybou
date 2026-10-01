// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_protocol.h>

#include <algorithm>
#include <array>
#include <limits>

namespace cybou {
namespace {
constexpr std::array<unsigned char, 5> REQUEST_MAGIC{'C','Y','B','Q','1'};
constexpr std::array<unsigned char, 5> RESPONSE_MAGIC{'C','Y','B','S','1'};
constexpr size_t MAX_ACTIVATION_CODE{128};

void PutU32(std::vector<unsigned char>& out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (i * 8)));
}
bool GetU32(std::span<const unsigned char> bytes, size_t& offset, uint32_t& value)
{
    if (offset > bytes.size() || bytes.size() - offset < 4) return false;
    value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t{bytes[offset++]} << (i * 8);
    return true;
}
void PutU64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (i * 8)));
}
bool GetU64(std::span<const unsigned char> bytes, size_t& offset, uint64_t& value)
{
    if (offset > bytes.size() || bytes.size() - offset < 8) return false;
    value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= uint64_t{bytes[offset++]} << (i * 8);
    return true;
}
bool ReadBlob(std::span<const unsigned char> bytes, size_t& offset, std::vector<unsigned char>& out,
    size_t maximum)
{
    uint32_t size{0};
    if (!GetU32(bytes, offset, size) || size > maximum || offset > bytes.size() || bytes.size() - offset < size)
        return false;
    out.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
        bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += size;
    return true;
}
void PutBlob(std::vector<unsigned char>& out, std::span<const unsigned char> value)
{
    PutU32(out, static_cast<uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}
}

std::optional<std::vector<unsigned char>> EncodeBootstrapRequest(const BootstrapRequest& request)
{
    std::vector<unsigned char> out{REQUEST_MAGIC.begin(), REQUEST_MAGIC.end()};
    out.push_back(static_cast<unsigned char>(request.kind));
    switch (request.kind) {
    case BootstrapRequestKind::STATUS:
        if (!request.activation_code.empty() || request.binding || request.previous_binding || request.replacement ||
            request.transition_from_generation != 0) return std::nullopt;
        break;
    case BootstrapRequestKind::CLAIM: {
        if (request.activation_code.empty() || request.activation_code.size() > MAX_ACTIVATION_CODE ||
            !request.binding || request.previous_binding || request.replacement || request.transition_from_generation != 0) return std::nullopt;
        const auto binding = EncodeBootstrapNetworkBinding(*request.binding);
        if (!binding) return std::nullopt;
        PutBlob(out, std::span<const unsigned char>{reinterpret_cast<const unsigned char*>(request.activation_code.data()), request.activation_code.size()});
        PutBlob(out, *binding);
        break;
    }
    case BootstrapRequestKind::REPLACE: {
        if (!request.replacement || !request.previous_binding || request.binding || !request.activation_code.empty() ||
            request.transition_from_generation != 0) return std::nullopt;
        const auto replacement = EncodeBootstrapNetworkReplacement(*request.previous_binding, *request.replacement);
        if (!replacement) return std::nullopt;
        PutBlob(out, *replacement);
        break;
    }
    case BootstrapRequestKind::GET_TRANSITION:
        if (request.transition_from_generation == 0 || !request.activation_code.empty() || request.binding ||
            request.previous_binding || request.replacement) return std::nullopt;
        PutU64(out, request.transition_from_generation);
        break;
    default: return std::nullopt;
    }
    if (out.size() > p2p::MAX_BOOTSTRAP_FRAME_PAYLOAD) return std::nullopt;
    return out;
}

std::optional<BootstrapRequest> DecodeBootstrapRequest(const std::span<const unsigned char> bytes,
    const std::optional<BootstrapNetworkBinding>& current_binding)
{
    if (bytes.size() < REQUEST_MAGIC.size() + 1 || bytes.size() > p2p::MAX_BOOTSTRAP_FRAME_PAYLOAD ||
        !std::equal(REQUEST_MAGIC.begin(), REQUEST_MAGIC.end(), bytes.begin())) return std::nullopt;
    BootstrapRequest request;
    request.kind = static_cast<BootstrapRequestKind>(bytes[REQUEST_MAGIC.size()]);
    size_t offset = REQUEST_MAGIC.size() + 1;
    if (request.kind == BootstrapRequestKind::STATUS)
        return offset == bytes.size() ? std::optional<BootstrapRequest>{request} : std::nullopt;
    if (request.kind == BootstrapRequestKind::GET_TRANSITION) {
        if (!GetU64(bytes, offset, request.transition_from_generation) || request.transition_from_generation == 0 ||
            offset != bytes.size()) return std::nullopt;
        return request;
    }
    std::vector<unsigned char> blob;
    if (request.kind == BootstrapRequestKind::CLAIM) {
        if (!ReadBlob(bytes, offset, blob, MAX_ACTIVATION_CODE) || blob.empty()) return std::nullopt;
        request.activation_code.assign(blob.begin(), blob.end());
        if (!ReadBlob(bytes, offset, blob, p2p::MAX_BOOTSTRAP_FRAME_PAYLOAD) || offset != bytes.size()) return std::nullopt;
        request.binding = DecodeBootstrapNetworkBinding(blob);
        return request.binding ? std::optional<BootstrapRequest>{std::move(request)} : std::nullopt;
    }
    if (request.kind == BootstrapRequestKind::REPLACE && current_binding) {
        if (!ReadBlob(bytes, offset, blob, p2p::MAX_BOOTSTRAP_FRAME_PAYLOAD) || offset != bytes.size()) return std::nullopt;
        request.replacement = DecodeBootstrapNetworkReplacement(blob, *current_binding);
        request.previous_binding = current_binding;
        return request.replacement ? std::optional<BootstrapRequest>{std::move(request)} : std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::vector<unsigned char>> EncodeBootstrapResponse(const BootstrapResponse& response)
{
    std::vector<unsigned char> out{RESPONSE_MAGIC.begin(), RESPONSE_MAGIC.end()};
    out.push_back(static_cast<unsigned char>(response.status));
    if (response.status == BootstrapResponseStatus::TRANSITION) {
        if (response.binding || response.transition.empty()) return std::nullopt;
        PutBlob(out, response.transition);
    } else if (response.binding) {
        if (!response.transition.empty()) return std::nullopt;
        const auto binding = EncodeBootstrapNetworkBinding(*response.binding);
        if (!binding) return std::nullopt;
        PutBlob(out, *binding);
    } else {
        if (!response.transition.empty()) return std::nullopt;
        PutU32(out, 0);
    }
    if (out.size() > p2p::MAX_BOOTSTRAP_FRAME_PAYLOAD) return std::nullopt;
    return out;
}

std::optional<BootstrapResponse> DecodeBootstrapResponse(const std::span<const unsigned char> bytes)
{
    if (bytes.size() < 10 || bytes.size() > p2p::MAX_BOOTSTRAP_FRAME_PAYLOAD ||
        !std::equal(RESPONSE_MAGIC.begin(), RESPONSE_MAGIC.end(), bytes.begin()) || bytes[5] > 10) return std::nullopt;
    size_t offset{6};
    std::vector<unsigned char> blob;
    if (!ReadBlob(bytes, offset, blob, p2p::MAX_BOOTSTRAP_FRAME_PAYLOAD) || offset != bytes.size()) return std::nullopt;
    BootstrapResponse response{static_cast<BootstrapResponseStatus>(bytes[5]), std::nullopt, {}};
    if (!blob.empty()) {
        if (response.status == BootstrapResponseStatus::TRANSITION) response.transition = std::move(blob);
        else {
            response.binding = DecodeBootstrapNetworkBinding(blob);
            if (!response.binding) return std::nullopt;
        }
    }
    if (response.status == BootstrapResponseStatus::TRANSITION && response.transition.empty()) return std::nullopt;
    if (response.status == BootstrapResponseStatus::EMPTY && response.binding) return std::nullopt;
    if ((response.status == BootstrapResponseStatus::BOUND || response.status == BootstrapResponseStatus::CLAIMED ||
         response.status == BootstrapResponseStatus::REPLACED || response.status == BootstrapResponseStatus::ALREADY_BOUND) &&
        !response.binding) return std::nullopt;
    return response;
}

std::optional<p2p::Frame> BootstrapProtocolHandler::Handle(const p2p::Frame& frame)
{
    using p2p::MessageType;
    if (frame.type != MessageType::BOOTSTRAP_REQUEST) return std::nullopt;
    const auto current = m_store.CurrentBinding();
    auto request = DecodeBootstrapRequest(frame.payload, current);
    BootstrapResponse response;
    if (!request) {
        response.status = m_store.State() == BootstrapStoreState::EMPTY ? BootstrapResponseStatus::EMPTY :
            BootstrapResponseStatus::INVALID_REQUEST;
        response.binding = current;
    } else if (request->kind == BootstrapRequestKind::STATUS) {
        response.status = current ? BootstrapResponseStatus::BOUND : BootstrapResponseStatus::EMPTY;
        response.binding = current;
    } else if (request->kind == BootstrapRequestKind::CLAIM) {
        switch (m_store.ClaimInitialNetwork(request->activation_code, *request->binding)) {
        case BootstrapClaimStatus::CLAIMED: response.status = BootstrapResponseStatus::CLAIMED; break;
        case BootstrapClaimStatus::ALREADY_BOUND: response.status = BootstrapResponseStatus::ALREADY_BOUND; break;
        case BootstrapClaimStatus::INVALID_ACTIVATION_CODE: response.status = BootstrapResponseStatus::INVALID_ACTIVATION_CODE; break;
        case BootstrapClaimStatus::INVALID_BINDING: response.status = BootstrapResponseStatus::INVALID_BINDING; break;
        default: response.status = BootstrapResponseStatus::STORAGE_ERROR; break;
        }
        response.binding = m_store.CurrentBinding();
    } else if (request->kind == BootstrapRequestKind::GET_TRANSITION) {
        const auto transition = m_store.ArchivedTransition(request->transition_from_generation);
        if (transition) {
            const auto previous = m_store.ArchivedBinding(request->transition_from_generation);
            if (previous) {
                const auto encoded = EncodeBootstrapNetworkReplacement(*previous, *transition);
                if (encoded) {
                    response.status = BootstrapResponseStatus::TRANSITION;
                    response.transition = *encoded;
                }
            }
        } else {
            response.status = BootstrapResponseStatus::INVALID_REQUEST;
        }
    } else {
        const auto status = request->replacement ? m_store.ReplaceNetwork(*request->replacement) :
            BootstrapReplacementStatus::INVALID_REPLACEMENT;
        switch (status) {
        case BootstrapReplacementStatus::REPLACED: response.status = BootstrapResponseStatus::REPLACED; break;
        case BootstrapReplacementStatus::NOT_BOUND: response.status = BootstrapResponseStatus::EMPTY; break;
        case BootstrapReplacementStatus::INVALID_REPLACEMENT: response.status = BootstrapResponseStatus::INVALID_REPLACEMENT; break;
        default: response.status = BootstrapResponseStatus::STORAGE_ERROR; break;
        }
        response.binding = m_store.CurrentBinding();
    }
    auto payload = EncodeBootstrapResponse(response);
    if (!payload) return std::nullopt;
    return p2p::Frame{MessageType::BOOTSTRAP_RESPONSE, std::move(*payload)};
}

} // namespace cybou
