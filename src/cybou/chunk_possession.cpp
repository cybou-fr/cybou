// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#include <cybou/chunk_possession.h>
#include <cybou/encrypted_chunk.h>
#include <algorithm>
#include <bit>
namespace cybou {
namespace {
using CV = std::array<uint32_t, 8>;
using Words = std::array<uint32_t, 16>;
constexpr CV IV{0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
constexpr std::array<unsigned,16> PERM{2,6,3,10,7,0,4,13,1,11,12,5,9,14,15,8};
void Mix(Words& v, unsigned a, unsigned b, unsigned c, unsigned d, uint32_t x, uint32_t y) {
    v[a] += v[b] + x; v[d] = std::rotr(v[d] ^ v[a],16); v[c] += v[d]; v[b] = std::rotr(v[b] ^ v[c],12);
    v[a] += v[b] + y; v[d] = std::rotr(v[d] ^ v[a],8); v[c] += v[d]; v[b] = std::rotr(v[b] ^ v[c],7);
}
CV Compress(const CV& cv, Words m, uint64_t counter, uint32_t length, uint32_t flags) {
    Words v{}; std::copy(cv.begin(),cv.end(),v.begin()); std::copy_n(IV.begin(),4,v.begin()+8);
    v[12]=counter; v[13]=counter>>32; v[14]=length; v[15]=flags;
    for(unsigned r{0};r<7;++r) {
        Mix(v,0,4,8,12,m[0],m[1]); Mix(v,1,5,9,13,m[2],m[3]); Mix(v,2,6,10,14,m[4],m[5]); Mix(v,3,7,11,15,m[6],m[7]);
        Mix(v,0,5,10,15,m[8],m[9]); Mix(v,1,6,11,12,m[10],m[11]); Mix(v,2,7,8,13,m[12],m[13]); Mix(v,3,4,9,14,m[14],m[15]);
        const auto old=m; for(unsigned i{0};i<16;++i) m[i]=old[PERM[i]];
    }
    CV out{}; for(unsigned i{0};i<8;++i) out[i]=v[i]^v[i+8]; return out;
}
CV Leaf(std::span<const unsigned char> bytes, uint64_t index, bool root=false) {
    CV cv=IV;
    for(size_t offset{0};offset<bytes.size();offset+=64) {
        const auto count=std::min<size_t>(64,bytes.size()-offset); Words m{};
        for(size_t i{0};i<count;++i) m[i/4] |= uint32_t{bytes[offset+i]} << (8*(i%4));
        const bool last=offset+count==bytes.size();
        cv=Compress(cv,m,index,count,(offset==0?1U:0U)|(last?2U:0U)|(root&&last?8U:0U));
    }
    return cv;
}
CV Parent(const CV& left,const CV& right,bool root=false) {
    Words m{}; std::copy(left.begin(),left.end(),m.begin()); std::copy(right.begin(),right.end(),m.begin()+8);
    return Compress(IV,m,0,64,4U|(root?8U:0U));
}
size_t Split(size_t count) { return std::bit_floor(count-1); }
CV Tree(std::span<const unsigned char> bytes,size_t first,size_t count,size_t target,std::vector<CV>* proof) {
    if(count==1) return Leaf(bytes.subspan(first*1024,std::min<size_t>(1024,bytes.size()-first*1024)),first);
    const auto left_count=Split(count); const bool in_left=target<first+left_count;
    const auto left=Tree(bytes,first,left_count,target,in_left?proof:nullptr);
    const auto right=Tree(bytes,first+left_count,count-left_count,target,in_left?nullptr:proof);
    if(proof) proof->push_back(in_left?right:left); return Parent(left,right);
}
CV Restore(const ChunkPossessionProof& proof,size_t first,size_t count,size_t& used,bool root) {
    if(count==1) return Leaf(proof.leaf_bytes,first,root);
    const auto left_count=Split(count); const bool in_left=proof.leaf_index<first+left_count;
    const auto child=Restore(proof,in_left?first:first+left_count,in_left?left_count:count-left_count,used,false);
    if(used>=proof.siblings.size()) return {};
    const auto other=proof.siblings[used++]; return in_left?Parent(child,other,root):Parent(other,child,root);
}
bool Shape(const ChunkPossessionProof& p) {
    if(p.stored_bytes<ENCRYPTED_CHUNK_MIN_STORED_BYTES || p.stored_bytes>ENCRYPTED_CHUNK_MAX_STORED_BYTES || p.siblings.size()>10) return false;
    const size_t count=(p.stored_bytes+1023)/1024;
    if(p.leaf_index>=count || p.leaf_bytes.size()!=std::min<uint64_t>(1024,p.stored_bytes-uint64_t{p.leaf_index}*1024)) return false;
    size_t first{0},remaining=count,depth{0};
    while(remaining>1) {const auto left=Split(remaining); if(p.leaf_index<first+left) remaining=left; else {first+=left;remaining-=left;} ++depth;}
    return depth==p.siblings.size();
}
void N(std::vector<unsigned char>& out,uint64_t value,unsigned width) { for(unsigned i{0};i<width;++i) out.push_back(value>>(8*i)); }
uint64_t Read(std::span<const unsigned char> bytes,size_t& offset,unsigned width) {
    uint64_t out{0};for(unsigned i{0};i<width;++i)out|=uint64_t{bytes[offset++]}<<(8*i);return out;
}
} // namespace
std::optional<ChunkPossessionProof> BuildChunkPossessionProof(std::span<const unsigned char> bytes,uint32_t leaf) {
    if(bytes.size()<ENCRYPTED_CHUNK_MIN_STORED_BYTES || bytes.size()>ENCRYPTED_CHUNK_MAX_STORED_BYTES || leaf>=(bytes.size()+1023)/1024) return std::nullopt;
    ChunkPossessionProof p{bytes.size(),leaf,{}, {}};
    const auto part=bytes.subspan(size_t{leaf}*1024,std::min<size_t>(1024,bytes.size()-size_t{leaf}*1024));p.leaf_bytes.assign(part.begin(),part.end());
    Tree(bytes,0,(bytes.size()+1023)/1024,leaf,&p.siblings); return p;
}
bool VerifyChunkPossessionProof(const ChunkId& id,const ChunkPossessionProof& proof) {
    if(!Shape(proof))return false;size_t used{0};const auto cv=Restore(proof,0,(proof.stored_bytes+1023)/1024,used,true);
    ChunkId digest{};for(unsigned i{0};i<32;++i)digest[i]=cv[i/4]>>(8*(i%4));return used==proof.siblings.size()&&digest==id;
}
std::optional<std::vector<unsigned char>> SerializeChunkPossessionProof(const ChunkPossessionProof& proof) {
    if(!Shape(proof))return std::nullopt;std::vector<unsigned char> out{1};N(out,proof.stored_bytes,8);N(out,proof.leaf_index,4);
    N(out,proof.leaf_bytes.size(),2);out.insert(out.end(),proof.leaf_bytes.begin(),proof.leaf_bytes.end());N(out,proof.siblings.size(),1);
    for(const auto& cv:proof.siblings)for(auto word:cv)N(out,word,4);return out;
}
std::optional<ChunkPossessionProof> DeserializeChunkPossessionProof(std::span<const unsigned char> bytes) {
    if(bytes.size()<16 || bytes.size()>16+1024+320 || bytes[0]!=1)return std::nullopt;size_t offset{1};ChunkPossessionProof p;
    p.stored_bytes=Read(bytes,offset,8);p.leaf_index=Read(bytes,offset,4);const auto size=Read(bytes,offset,2);
    if(size>1024 || size>bytes.size()-offset-1)return std::nullopt;
    p.leaf_bytes.assign(bytes.begin()+offset,bytes.begin()+offset+size);offset+=size;const auto count=bytes[offset++];
    if(count>10 || bytes.size()-offset!=size_t{count}*32)return std::nullopt;
    p.siblings.resize(count);for(auto& cv:p.siblings)for(auto& word:cv)word=Read(bytes,offset,4);
    return Shape(p)?std::optional{p}:std::nullopt;
}
} // namespace cybou
