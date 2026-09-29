// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_TEST_HELPERS_H
#define CYBOU_TEST_HELPERS_H

#include <cybou/identity_crypto.h>
#include <cybou/network_definition.h>

#include <array>

namespace cybou {

inline IdentityHybridPublicKey TestPoaFinalizerPublicKey(unsigned char seed_byte = 0xA7)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = seed_byte;
    return DeriveIdentityPublicKey(seed, IdentityKeyPurpose::POA_FINALIZER).value();
}

} // namespace cybou

#endif
