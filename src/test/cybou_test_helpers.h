// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_TEST_HELPERS_H
#define CYBOU_TEST_HELPERS_H

#include <cybou/network_genesis.h>
#include <map>
#include <mutex>
#include <cybou/identity_crypto.h>
#include <cybou/network_genesis.h>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cybou {

namespace test {

inline std::string Hex(std::span<const unsigned char> bytes)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(bytes.size() * 2);
    for (const unsigned char byte : bytes) {
        output.push_back(digits[byte >> 4]);
        output.push_back(digits[byte & 0x0f]);
    }
    return output;
}

inline std::vector<unsigned char> ParseHex(std::string_view input)
{
    auto nibble = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    std::vector<unsigned char> output;
    if ((input.size() & 1) != 0) return output;
    output.reserve(input.size() / 2);
    for (std::size_t i = 0; i < input.size(); i += 2) {
        const int high = nibble(input[i]);
        const int low = nibble(input[i + 1]);
        if (high < 0 || low < 0) return {};
        output.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return output;
}

} // namespace test

/** Canonical fee recipient for synthetic networks; never an official key. */
inline CybouState CreateTestGenesisState()
{
    auto state = CreateDevGenesisState();
    IdentityKeyId recovery_id{};
    recovery_id[0] = 0xCA;
    state.genesis_allocations.emplace(recovery_id,
        GenesisAllocation{.label = std::string{CENTRAL_AUTHORITY_NAME}, .claimed_by = std::nullopt});
    return state;
}

inline std::map<std::array<unsigned char, 32>, std::array<unsigned char, 32>>& TestNetworkSecrets()
{
    static std::map<std::array<unsigned char, 32>, std::array<unsigned char, 32>> secrets;
    return secrets;
}
inline std::mutex& TestNetworkSecretsMutex() { static std::mutex mutex; return mutex; }

/** A test Network Public Key; distinct seeds give distinct networks. */
inline IdentityHybridPublicKey TestNetworkPublicKey(unsigned char seed_byte = 0xA7)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = seed_byte;
    seed[1] = 0x4e;
    auto key = DeriveIdentityPublicKey(seed, IdentityKeyPurpose::NETWORK_ROOT).value();
    std::lock_guard lock{TestNetworkSecretsMutex()};
    TestNetworkSecrets()[key.ed25519] = seed;
    return key;
}

inline IdentityHybridPublicKey TestPoaFinalizerPublicKey(unsigned char seed_byte = 0xA7)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = seed_byte;
    return DeriveIdentityPublicKey(seed, IdentityKeyPurpose::POA_FINALIZER).value();
}

inline VerifiedNetworkGenesis SignTestNetworkGenesis(NetworkGenesis specification)
{
    std::array<unsigned char, 32> seed{};
    { std::lock_guard lock{TestNetworkSecretsMutex()}; seed = TestNetworkSecrets().at(specification.network_public_key.ed25519); }
    const auto digest = ComputeNetworkGenesisDigest(specification);
    specification.signature = SignIdentityMessage(seed, IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{digest.begin(), digest.size()}).value();
    return VerifiedNetworkGenesis::Create(std::move(specification)).value();
}
inline VerifiedNetworkGenesis CreateTestNetworkGenesis(const CybouState& state,
    const IdentityHybridPublicKey& poa_key, const IdentityHybridPublicKey& network_key,
    const CybouProtocolParameters& parameters = DevProtocolParameters())
{
    NetworkGenesis specification;
    specification.network_public_key = network_key;
    specification.genesis_state_root = CybouStateHash(state).value();
    specification.poa_finalizer_public_key = poa_key;
    specification.protocol_parameters = parameters;
    return SignTestNetworkGenesis(std::move(specification));
}
template <typename Mutate>
VerifiedNetworkGenesis WithTestGenesisParameters(const VerifiedNetworkGenesis& genesis, Mutate mutate)
{
    auto specification = genesis.GetGenesis();
    mutate(specification.protocol_parameters);
    return SignTestNetworkGenesis(std::move(specification));
}

} // namespace cybou

#endif
