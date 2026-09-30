// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#include <cybou/service_evidence.h>
#include <cybou/crypto/sha256.h>
#include <cybou/validation_service.h>
#include <algorithm>
#include <stdexcept>
namespace cybou {
namespace {
void N(std::vector<unsigned char>& out,uint64_t value,unsigned width) {for(unsigned i{0};i<width;++i)out.push_back(value>>(8*i));}
std::optional<std::vector<unsigned char>> Body(const ServiceEvidence& e) {
    const auto kind=static_cast<uint8_t>(e.kind);
    if(e.account_id.IsNull()||e.node_id.IsNull()||e.base_block_id.IsNull()||kind<1||kind>5)return std::nullopt;
    const bool receipt=e.kind==ServiceEvidenceKind::VALIDATION_RECEIPT;
    const bool storage=e.kind!=ServiceEvidenceKind::HEARTBEAT&&!receipt;
    const bool proof=e.kind==ServiceEvidenceKind::STORAGE_COMMIT||e.kind==ServiceEvidenceKind::STORAGE_RESPONSE;
    if(storage?(e.chunk_id==ChunkId{}||e.publication_id.IsNull()): (e.chunk_id!=ChunkId{}||(!receipt&&!e.publication_id.IsNull())))return std::nullopt;
    if(receipt ? (e.base_state_root.IsNull()||e.validated_operation.empty()||e.validated_operation.size()>128U*1024U||e.publication_id.IsNull())
               : (!e.base_state_root.IsNull()||!e.validated_operation.empty()))return std::nullopt;
    if(proof!=e.possession.has_value())return std::nullopt;
    if(e.authorization_siblings.size()>20 || (e.kind!=ServiceEvidenceKind::STORAGE_COMMIT && (e.authorization_index!=0||!e.authorization_siblings.empty()))) return std::nullopt;
    const auto encoded=e.possession?SerializeChunkPossessionProof(*e.possession):std::optional<std::vector<unsigned char>>{std::vector<unsigned char>{}};
    if(!encoded)return std::nullopt;
    std::vector<unsigned char> out{1};
    const auto h=[&](const auto& id) {out.insert(out.end(),id.begin(),id.end());};
    h(e.account_id.Value());h(e.node_id);N(out,e.base_height,8);h(e.base_block_id);out.push_back(kind);h(e.publication_id);h(e.chunk_id);
    N(out,encoded->size(),2);out.insert(out.end(),encoded->begin(),encoded->end());
    N(out,e.authorization_index,4);N(out,e.authorization_siblings.size(),1);for(const auto& sibling:e.authorization_siblings)h(sibling);
    h(e.base_state_root);N(out,e.validated_operation.size(),4);out.insert(out.end(),e.validated_operation.begin(),e.validated_operation.end());return out;
}
}
uint32_t ContributionSlots(uint64_t blocks) {return static_cast<uint32_t>(std::min<uint64_t>(blocks,16));}
std::optional<uint32_t> ContributionSlot(uint64_t height,uint64_t blocks) {
    if(blocks==0)return std::nullopt;const auto offset=height%blocks;const auto slots=ContributionSlots(blocks);
    // Division first bounds arithmetic even with hostile epoch_blocks near UINT64_MAX.
    for(uint32_t slot{0};slot<slots;++slot) {
        const auto at=(blocks/slots)*slot+((blocks%slots)*slot)/slots;
        if(offset==at)return slot;
    }
    return std::nullopt;
}
uint32_t StorageChallengeLeaf(const uint256& network,const uint256& base,const AccountId& owner,const ChunkId& chunk,uint64_t size) {
    if(size==0)return 0;
    std::array<unsigned char,32> hash{};const auto& id=owner.Value();
    if(!crypto::ComputeSha256({crypto::Sha256Bytes("CYBOU/STORAGE-CHALLENGE/V1"),{network.begin(),32},{base.begin(),32},{id.begin(),32},chunk},hash.data()))throw std::runtime_error{"challenge hash unavailable"};
    uint64_t value{0};for(unsigned i{0};i<8;++i)value|=uint64_t{hash[i]}<<(8*i);
    return static_cast<uint32_t>(value/1%((size+1023)/1024));
}
std::optional<IdentityKeyId> ServiceEvidenceDigest(const uint256& network,const ServiceEvidence& e) {
    const auto body=Body(e);if(!body||network.IsNull())return std::nullopt;IdentityKeyId out{};
    if(e.kind==ServiceEvidenceKind::VALIDATION_RECEIPT) return ValidationAttestationDigest(
        ValidationAttestation{{network,e.base_height,e.base_block_id,e.base_state_root},e.publication_id,e.node_id,{}});
    if(!crypto::ComputeSha256({crypto::Sha256Bytes("CYBOU/SERVICE-EVIDENCE/V1"),{network.begin(),32},*body},out.data()))return std::nullopt;return out;
}
std::optional<std::vector<unsigned char>> SerializeServiceEvidence(const ServiceEvidence& e) {
    auto out=Body(e);if(!out||e.signature.ml_dsa.size()!=2420)return std::nullopt;
    out->insert(out->end(),e.signature.ed25519.begin(),e.signature.ed25519.end());out->insert(out->end(),e.signature.ml_dsa.begin(),e.signature.ml_dsa.end());return out;
}
std::optional<ServiceEvidence> DeserializeServiceEvidence(std::span<const unsigned char> bytes) {
    constexpr size_t fixed=1+32+32+8+32+1+32+32+2;
    if(bytes.size()<fixed+5+36+64+2420||bytes.size()>fixed+1360+5+640+36+128U*1024U+64+2420||bytes[0]!=1)return std::nullopt;
    ServiceEvidence e;size_t offset{1};
    const auto h=[&](auto& id) {std::copy_n(bytes.begin()+offset,32,id.begin());offset+=32;};
    uint256 account;h(account);e.account_id=AccountId{account};h(e.node_id);
    for(unsigned i{0};i<8;++i)e.base_height|=uint64_t{bytes[offset++]}<<(8*i);
    h(e.base_block_id);e.kind=static_cast<ServiceEvidenceKind>(bytes[offset++]);h(e.publication_id);h(e.chunk_id);
    const size_t size=bytes[offset]|(size_t{bytes[offset+1]}<<8);offset+=2;
    if(size>1360||bytes.size()<fixed+size+5+36+64+2420)return std::nullopt;
    if(size){e.possession=DeserializeChunkPossessionProof(bytes.subspan(offset,size));if(!e.possession)return std::nullopt;offset+=size;}
    for(unsigned i{0};i<4;++i)e.authorization_index|=uint32_t{bytes[offset++]}<<(8*i);
    const auto siblings=bytes[offset++];if(siblings>20||bytes.size()-offset<size_t{siblings}*32+36+64+2420)return std::nullopt;
    e.authorization_siblings.resize(siblings);for(auto& sibling:e.authorization_siblings)h(sibling);
    h(e.base_state_root);uint32_t operation_size{0};for(unsigned i{0};i<4;++i)operation_size|=uint32_t{bytes[offset++]}<<(8*i);
    if(operation_size>128U*1024U||bytes.size()-offset!=operation_size+64+2420)return std::nullopt;
    e.validated_operation.assign(bytes.begin()+offset,bytes.begin()+offset+operation_size);offset+=operation_size;
    std::copy_n(bytes.begin()+offset,64,e.signature.ed25519.begin());offset+=64;
    e.signature.ml_dsa.assign(bytes.begin()+offset,bytes.end());return SerializeServiceEvidence(e)?std::optional{e}:std::nullopt;
}
} // namespace cybou
