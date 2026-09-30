// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_LIMITS_H
#define CYBOU_PROTOCOL_LIMITS_H

#include <cstddef>
#include <cstdint>

namespace cybou {

/**
 * One invariant for every layer: consensus validation of RootPublication,
 * fees, bundle staging, the chunk-authorization proof index, encrypted trees
 * and StorageService placement. A publication the network can finalize is
 * always one the application and storage layers can serve.
 * 2^20 chunks is hundreds of GiB of encrypted content per publication.
 */
inline constexpr std::uint32_t MAX_PUBLICATION_CHUNKS{1U << 20};

/** Largest serialized ProtocolOperation accepted anywhere (pool, CYP2, CLI). */
inline constexpr std::uint32_t MAX_OPERATION_PAYLOAD_BYTES{128U * 1024U};
/** Largest serialized finalized block transferred over CYP2. */
inline constexpr std::uint32_t MAX_FINALIZED_BLOCK_BYTES{32U * 1024U * 1024U};

} // namespace cybou

#endif // CYBOU_PROTOCOL_LIMITS_H
