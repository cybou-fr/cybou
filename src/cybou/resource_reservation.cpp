// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#include <cybou/resource_reservation.h>
#include <cybou/crypto/sha256.h>
#include <algorithm>
#include <set>
#include <stdexcept>
namespace cybou {
namespace {
void W64(std::vector<unsigned char>& out,uint64_t n) { for (unsigned i=0;i<8;++i) out.push_back(n>>(8*i)); }
uint64_t R64(std::span<const unsigned char> in) { uint64_t n=0; for(unsigned i=0;i<8;++i) n|=uint64_t{in[i]}<<(8*i); return n; }
uint256 Hash(std::string_view domain,std::span<const unsigned char> bytes) {
    uint256 id; if(!crypto::ComputeSha256({crypto::Sha256Bytes(domain),bytes},id.begin())) throw std::runtime_error("resource digest failure"); return id;
}
template<typename P,typename F> std::optional<IdentityKeyId> Commitment(const P& payload,F serialize,std::string_view domain) {
    const auto bytes=serialize(payload); if(!bytes) return std::nullopt;
    const auto hash=Hash(domain,*bytes); IdentityKeyId id{}; std::copy_n(hash.begin(),32,id.begin()); return id;
}
}
std::vector<unsigned char> SerializeResourceTicket(const ResourceTicket& ticket) {
    std::vector<unsigned char> out;
    for(const auto* id:{&ticket.storage_grant,&ticket.bandwidth_grant,&ticket.request_nonce,&ticket.publication_id})out.insert(out.end(),id->begin(),id->end());
    return out;
}
std::optional<ResourceTicket> DeserializeResourceTicket(std::span<const unsigned char> in) {
    if(in.size()!=RESOURCE_TICKET_SIZE)return std::nullopt;
    ResourceTicket ticket;size_t offset=0;
    for(auto* id:{&ticket.storage_grant,&ticket.bandwidth_grant,&ticket.request_nonce,&ticket.publication_id}) {std::copy_n(in.begin()+offset,32,id->begin());offset+=32;}
    if(ticket.storage_grant.IsNull()||ticket.bandwidth_grant.IsNull()||ticket.request_nonce.IsNull()||ticket.publication_id.IsNull())return std::nullopt;
    return ticket;
}
std::optional<std::vector<unsigned char>> SerializeResourceReservation(const ResourceReservationPayload& p) {
    if(p.grants.empty()||p.grants.size()>MAX_RESOURCE_GRANTS_PER_OPERATION) return std::nullopt;
    std::vector<unsigned char> out{1,static_cast<unsigned char>(p.grants.size())}; std::set<uint256> commitments;
    for(const auto& g:p.grants) {
        if((g.domain!=ResourceDomain::STORAGE&&g.domain!=ResourceDomain::BANDWIDTH)||!g.bytes||g.bytes>(256ULL<<30)||g.use_commitment.IsNull()||!commitments.insert(g.use_commitment).second) return std::nullopt;
        out.push_back(static_cast<uint8_t>(g.domain)); W64(out,g.bytes); out.insert(out.end(),g.use_commitment.begin(),g.use_commitment.end());
    } return out;
}
std::optional<ResourceReservationPayload> DeserializeResourceReservation(std::span<const unsigned char> in) {
    if(in.size()<2||in[0]!=1||in[1]==0||in[1]>MAX_RESOURCE_GRANTS_PER_OPERATION||in.size()!=2+41*size_t{in[1]}) return std::nullopt;
    ResourceReservationPayload p;
    for(size_t off=2;off<in.size();off+=41) { ResourceGrant g; g.domain=static_cast<ResourceDomain>(in[off]); g.bytes=R64(in.subspan(off+1,8)); std::copy_n(in.begin()+off+9,32,g.use_commitment.begin()); p.grants.push_back(g); }
    return SerializeResourceReservation(p)?std::optional{p}:std::nullopt;
}
std::optional<std::vector<unsigned char>> SerializeResourceRelease(const ResourceReleasePayload& p) {
    if(p.grants.empty()||p.grants.size()>MAX_RESOURCE_GRANTS_PER_OPERATION) return std::nullopt;
    std::vector<unsigned char> out{1,static_cast<unsigned char>(p.grants.size())}; std::set<uint256> ids;
    for(const auto& id:p.grants) { if(id.IsNull()||!ids.insert(id).second) return std::nullopt; out.insert(out.end(),id.begin(),id.end()); } return out;
}
std::optional<ResourceReleasePayload> DeserializeResourceRelease(std::span<const unsigned char> in) {
    if(in.size()<2||in[0]!=1||in[1]==0||in[1]>MAX_RESOURCE_GRANTS_PER_OPERATION||in.size()!=2+32*size_t{in[1]}) return std::nullopt;
    ResourceReleasePayload p; for(size_t off=2;off<in.size();off+=32) { uint256 id; std::copy_n(in.begin()+off,32,id.begin()); p.grants.push_back(id); }
    return SerializeResourceRelease(p)?std::optional{p}:std::nullopt;
}
std::optional<IdentityKeyId> ResourceReservationCommitment(const ResourceReservationPayload& p) { return Commitment(p,SerializeResourceReservation,"CYBOU/RESOURCE-RESERVATION/V1"); }
std::optional<IdentityKeyId> ResourceReleaseCommitment(const ResourceReleasePayload& p) { return Commitment(p,SerializeResourceRelease,"CYBOU/RESOURCE-RELEASE/V1"); }
uint256 ResourceGrantId(const uint256& operation,uint32_t index) {
    std::vector<unsigned char> bytes(operation.begin(),operation.end()); for(unsigned i=0;i<4;++i) bytes.push_back(index>>(8*i)); return Hash("CYBOU/RESOURCE-GRANT/V1",bytes);
}
uint256 ResourceUseCommitment(const uint256& network,const AccountId& account,std::span<const unsigned char,32> provider,ResourceUse use,const uint256& publication,const ChunkId& chunk,uint64_t bytes,const uint256& nonce) {
    std::vector<unsigned char> data(network.begin(),network.end()); data.insert(data.end(),account.Value().begin(),account.Value().end()); data.insert(data.end(),provider.begin(),provider.end()); data.push_back(static_cast<uint8_t>(use)); data.insert(data.end(),publication.begin(),publication.end()); data.insert(data.end(),chunk.begin(),chunk.end()); W64(data,bytes); data.insert(data.end(),nonce.begin(),nonce.end()); return Hash("CYBOU/RESOURCE-USE/V1",data);
}
}
