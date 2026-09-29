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

} // namespace cybou

#endif // CYBOU_PROTOCOL_LIMITS_H
