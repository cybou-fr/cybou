// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_RESOURCE_RESERVATION_H
#define CYBOU_RESOURCE_RESERVATION_H
#include <cybou/identity_registry.h>
#include <cybou/chunk_id.h>
namespace cybou {
enum class ResourceDomain : uint8_t { STORAGE = 1, BANDWIDTH = 2 };
enum class ResourceUse : uint8_t { STORE = 1, PUT = 2, GET = 3 };
inline constexpr size_t MAX_RESOURCE_GRANTS_PER_OPERATION{32};
inline constexpr size_t MAX_RESOURCE_GRANTS_PER_ACCOUNT{8192};
struct ResourceGrant {
    ResourceDomain domain{ResourceDomain::STORAGE};
    uint64_t bytes{0};
    uint256 use_commitment;
    friend bool operator==(const ResourceGrant&, const ResourceGrant&) = default;
};
struct ResourceReservationPayload {
    std::vector<ResourceGrant> grants;
    friend bool operator==(const ResourceReservationPayload&, const ResourceReservationPayload&) = default;
};
struct ResourceReleasePayload {
    std::vector<uint256> grants;
    friend bool operator==(const ResourceReleasePayload&, const ResourceReleasePayload&) = default;
};
struct AuthorizedResourceReservation {
    IdentityOperationAuthorization authorization;
    ResourceReservationPayload reservation;
    friend bool operator==(const AuthorizedResourceReservation&, const AuthorizedResourceReservation&) = default;
};
struct AuthorizedResourceRelease {
    IdentityOperationAuthorization authorization;
    ResourceReleasePayload release;
    friend bool operator==(const AuthorizedResourceRelease&, const AuthorizedResourceRelease&) = default;
};
struct ResourceTicket {
    uint256 storage_grant;
    uint256 bandwidth_grant;
    uint256 request_nonce;
    uint256 publication_id;
    friend bool operator==(const ResourceTicket&,const ResourceTicket&) = default;
};
inline constexpr size_t RESOURCE_TICKET_SIZE{128};
std::vector<unsigned char> SerializeResourceTicket(const ResourceTicket&);
std::optional<ResourceTicket> DeserializeResourceTicket(std::span<const unsigned char>);
struct ReservedResource {
    AccountId account;
    ResourceGrant grant;
    uint64_t epoch{0};
    friend bool operator==(const ReservedResource&, const ReservedResource&) = default;
};
std::optional<std::vector<unsigned char>> SerializeResourceReservation(const ResourceReservationPayload&);
std::optional<ResourceReservationPayload> DeserializeResourceReservation(std::span<const unsigned char>);
std::optional<std::vector<unsigned char>> SerializeResourceRelease(const ResourceReleasePayload&);
std::optional<ResourceReleasePayload> DeserializeResourceRelease(std::span<const unsigned char>);
std::optional<IdentityKeyId> ResourceReservationCommitment(const ResourceReservationPayload&);
std::optional<IdentityKeyId> ResourceReleaseCommitment(const ResourceReleasePayload&);
uint256 ResourceGrantId(const uint256& operation, uint32_t index);
/** Opaque to consensus: provider placement and application semantics are not state fields. */
uint256 ResourceUseCommitment(const uint256& network, const AccountId& account,
    std::span<const unsigned char,32> provider, ResourceUse use, const uint256& publication,
    const ChunkId& chunk, uint64_t bytes, const uint256& request_nonce = {});
} // namespace cybou
#endif
