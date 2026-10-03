// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/provision.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/network_genesis.h>
#include <cybou/secret_file.h>
#include <cybou/hex.h>

#include <openssl/rand.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace cybou {

namespace {

std::string JoinWords(const RecoveryWords& words)
{
    std::string res;
    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0) res += " ";
        res += words[i];
    }
    return res;
}

std::string FormatWordsNumbered(const RecoveryWords& words)
{
    std::ostringstream ss;
    for (size_t i = 0; i < words.size(); ++i) {
        ss << std::setw(2) << std::setfill(' ') << (i + 1) << ". " << words[i] << '\n';
    }
    return ss.str();
}

std::string FormatByteArrayCpp(std::span<const unsigned char> bytes, size_t indent = 4)
{
    std::ostringstream ss;
    const std::string ind(indent, ' ');
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i % 12 == 0) {
            if (i > 0) ss << '\n';
            ss << ind;
        }
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
        if (i + 1 < bytes.size()) ss << ", ";
    }
    return ss.str();
}

} // namespace

std::optional<DevnetProvisionResult> GenerateDevnetProvisioning()
{
    DevnetProvisionResult res;

    // 1. Network Root Key (strictly offline)
    auto net_entropy = GenerateRecoveryEntropy();
    if (!net_entropy) return std::nullopt;
    res.network_entropy = *net_entropy;
    res.network_words = EncodeRecoveryWords(res.network_entropy);
    auto net_pub = DeriveIdentityPublicKey(res.network_entropy, IdentityKeyPurpose::NETWORK_ROOT);
    if (!net_pub) return std::nullopt;
    res.network_public_key = *net_pub;
    res.network_id_bytes = CanonicalSerializeNetworkPublicKey(res.network_public_key);

    // 2. cybou.cybou Identity
    auto cybou_entropy = GenerateRecoveryEntropy();
    if (!cybou_entropy) return std::nullopt;
    res.cybou_entropy = *cybou_entropy;
    res.cybou_words = EncodeRecoveryWords(res.cybou_entropy);

    // Stable non-zero random AccountID (DEC-165)
    std::array<unsigned char, 32> acc_bytes{};
    do {
        if (RAND_bytes(acc_bytes.data(), static_cast<int>(acc_bytes.size())) != 1) {
            return std::nullopt;
        }
    } while (std::all_of(acc_bytes.begin(), acc_bytes.end(), [](unsigned char b){ return b == 0; }));
    auto parsed_acc = AccountId::FromBytes(acc_bytes);
    if (!parsed_acc) return std::nullopt;
    res.cybou_account_id = *parsed_acc;

    auto cybou_rec = DeriveIdentityPublicKey(res.cybou_entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    if (!cybou_rec) return std::nullopt;
    res.cybou_recovery_key = *cybou_rec;
    auto cybou_rec_id = ComputeRecoveryKeyId(res.cybou_recovery_key);
    if (!cybou_rec_id) return std::nullopt;
    res.cybou_recovery_key_id = *cybou_rec_id;

    auto cybou_auth = DeriveIdentityPublicKey(res.cybou_entropy, IdentityKeyPurpose::AUTHORIZATION);
    if (!cybou_auth) return std::nullopt;
    res.cybou_auth_key = *cybou_auth;

    auto kem_seed = DeriveIdentityXWingSeed(res.cybou_entropy);
    if (!kem_seed) return std::nullopt;
    auto kem_pub = DeriveXWingPublicKey(*kem_seed);
    crypto::CleanseMemory(kem_seed->data(), kem_seed->size());
    if (!kem_pub) return std::nullopt;
    res.cybou_kem_pub = *kem_pub;

    auto poa_pub = DeriveIdentityPublicKey(res.cybou_entropy, IdentityKeyPurpose::POA_FINALIZER);
    if (!poa_pub) return std::nullopt;
    res.cybou_poa_pub = *poa_pub;
    auto poa_id = ComputePoaFinalizerKeyId(res.cybou_poa_pub);
    if (!poa_id) return std::nullopt;
    res.cybou_poa_key_id = *poa_id;

    // 3. Consensus Genesis State: cybou.cybou is the only genesis allocation.
    res.genesis_state = CreateDevGenesisState();
    res.genesis_state.genesis_allocations[res.cybou_recovery_key_id] = GenesisAllocation{
        .balance = 100'000'000, .authority = 1'000'001, .label = std::string{CENTRAL_AUTHORITY_NAME}};

    if (ValidateCybouState(res.genesis_state) != StateValidationError::NONE) {
        return std::nullopt;
    }
    auto state_root = CybouStateHash(res.genesis_state);
    if (!state_root) return std::nullopt;
    res.genesis_state_root = *state_root;

    // 4. Signed NetworkGenesis: the digest is only what the Network Key signs.
    res.signed_genesis.version = CYBOU_NETWORK_GENESIS_VERSION;
    res.signed_genesis.network_public_key = res.network_public_key;
    res.signed_genesis.genesis_state_root = res.genesis_state_root;
    res.signed_genesis.poa_finalizer_public_key = res.cybou_poa_pub;
    res.signed_genesis.protocol_parameters = DevProtocolParameters();
    const auto genesis_digest = ComputeNetworkGenesisDigest(res.signed_genesis);
    auto sig = SignIdentityMessage(
        res.network_entropy,
        IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{genesis_digest.begin(), genesis_digest.size()});
    if (!sig) return std::nullopt;
    res.signed_genesis.signature = *sig;

    if (VerifySignedNetworkGenesis(res.signed_genesis) != NetworkGenesisError::NONE) {
        return std::nullopt;
    }

    // 5. Serializations
    res.serialized_signed_genesis = SerializeSignedNetworkGenesis(res.signed_genesis);
    auto state_bytes = SerializeCybouState(res.genesis_state);
    if (!state_bytes) return std::nullopt;
    res.serialized_genesis_state = std::move(*state_bytes);

    // Round-trip check
    auto roundtrip_genesis = DeserializeSignedNetworkGenesis(res.serialized_signed_genesis);
    if (!roundtrip_genesis || *roundtrip_genesis != res.signed_genesis) {
        return std::nullopt;
    }
    auto roundtrip_state = DeserializeCybouState(res.serialized_genesis_state);
    if (!roundtrip_state) {
        return std::nullopt;
    }
    auto roundtrip_state_root = CybouStateHash(*roundtrip_state);
    if (!roundtrip_state_root || *roundtrip_state_root != res.genesis_state_root) {
        return std::nullopt;
    }

    return res;
}

bool ProvisionDevnet(
    const std::filesystem::path& private_dir,
    const std::filesystem::path& constants_header_path)
{
    std::error_code ec;
    const auto private_root = std::filesystem::weakly_canonical(std::filesystem::current_path() / "private");
    const auto target = std::filesystem::weakly_canonical(private_dir);
    const auto relative = target.lexically_relative(private_root);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
        std::cerr << "Private material must stay beneath the repository private directory\n";
        return false;
    }
    if (std::filesystem::exists(constants_header_path)) {
        std::cerr << "Refusing to replace existing network constants or secret material\n";
        return false;
    }
    if (std::filesystem::exists(private_dir, ec)) {
        if (!std::filesystem::is_empty(private_dir, ec)) {
            std::cerr << "Error: target private directory is not empty: " << private_dir.string()
                      << " (existing networks are immutable)\n";
            return false;
        }
    }
    std::filesystem::create_directories(private_dir, ec);
    if (ec) {
        std::cerr << "Error creating directory: " << private_dir.string() << ": " << ec.message() << '\n';
        return false;
    }

    auto prov = GenerateDevnetProvisioning();
    if (!prov) {
        std::cerr << "Error generating DEVNET provisioning material\n";
        return false;
    }

    // 1. Write private material files
    const auto write_secret = [&](const std::string& filename, const std::string& content) -> bool {
        const auto p = private_dir / filename;
        std::vector<unsigned char> bytes(content.begin(), content.end());
        const bool ok = CreateSecretFile(p, bytes);
        crypto::CleanseMemory(bytes.data(), bytes.size());
        if (!ok) {
            std::cerr << "Failed to write secret file: " << p.string() << '\n';
        }
        return ok;
    };

    // Network root secret
    {
        std::ostringstream ss;
        ss << "# CYBOU DEVNET Network Root Key (OFFLINE ROOT OF TRUST)\n"
           << "# DO NOT COMMIT THIS FILE TO GIT OR DISTRIBUTE ONLINE\n\n"
           << "MNEMONIC_PHRASE:\n" << JoinWords(prov->network_words) << "\n\n"
           << "MNEMONIC_WORDS:\n" << FormatWordsNumbered(prov->network_words) << '\n'
           << "NETWORK_SEED_HEX: " << cybou::HexEncode(prov->network_entropy) << '\n'
           << "NETWORK_ID_HEX: " << cybou::HexEncode(prov->network_id_bytes) << '\n'
           << "NETWORK_ED25519_HEX: " << cybou::HexEncode(prov->network_public_key.ed25519) << '\n'
           << "NETWORK_ML_DSA_HEX: " << cybou::HexEncode(prov->network_public_key.ml_dsa) << '\n';
        if (!write_secret("network_root_secret.txt", ss.str())) return false;
    }

    // cybou.cybou Identity secret
    {
        std::ostringstream ss;
        ss << "# CYBOU DEVNET cybou.cybou Identity & PoA Finalizer Key\n"
           << "# Central Authority Desktop Material\n\n"
           << "MNEMONIC_PHRASE:\n" << JoinWords(prov->cybou_words) << "\n\n"
           << "MNEMONIC_WORDS:\n" << FormatWordsNumbered(prov->cybou_words) << '\n'
           << "CYBOU_SEED_HEX: " << cybou::HexEncode(prov->cybou_entropy) << '\n'
           << "ACCOUNT_ID_HEX: " << prov->cybou_account_id.Value().GetHex() << '\n'
           << "RECOVERY_KEY_ID_HEX: " << cybou::HexEncode(prov->cybou_recovery_key_id) << '\n'
           << "POA_FINALIZER_KEY_ID_HEX: " << cybou::HexEncode(prov->cybou_poa_key_id) << '\n'
           << "POA_ED25519_HEX: " << cybou::HexEncode(prov->cybou_poa_pub.ed25519) << '\n'
           << "POA_ML_DSA_HEX: " << cybou::HexEncode(prov->cybou_poa_pub.ml_dsa) << '\n';
        if (!write_secret("cybou_identity_secret.txt", ss.str())) return false;
    }

    // Human-readable summary
    {
        std::ostringstream ss;
        ss << "=================================================================\n"
           << "               CYBOU DEVNET PROVISIONING SUMMARY\n"
           << "=================================================================\n\n"
           << "Network Public Key (NetworkID):\n  " << cybou::HexEncode(prov->network_id_bytes) << "\n\n"
           << "Genesis State Root:\n  " << prov->genesis_state_root.GetHex() << "\n\n"
           << "PoA Finalizer Key ID:\n  " << cybou::HexEncode(prov->cybou_poa_key_id) << "\n\n"
           << "cybou.cybou Account ID:\n  " << prov->cybou_account_id.Value().GetHex() << "\n\n"
           << "cybou.cybou Recovery Key ID:\n  " << cybou::HexEncode(prov->cybou_recovery_key_id) << "\n"
           << "  Genesis allocation: 100,000,000 CYBOU, 1,000,001 AUTH, name cybou\n\n"
           << "Signed Genesis Size: " << prov->serialized_signed_genesis.size() << " bytes\n"
           << "Genesis State Size:  " << prov->serialized_genesis_state.size() << " bytes\n";
        const auto summary_path = private_dir / "summary.txt";
        std::ofstream sum_file(summary_path);
        if (sum_file) {
            sum_file << ss.str();
        }
    }

    // 2. Generate C++ public constants header
    {
        const auto parent = constants_header_path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, ec);
        }

        std::ostringstream h;
        h << "// Copyright (c) 2026 Stanislav SAVELIEV\n"
          << "// Distributed under the MIT software license, see the accompanying\n"
          << "// file COPYING or https://opensource.org/license/mit/.\n\n"
          << "// AUTO-GENERATED BY CYBOU PROVISIONING TOOL. DO NOT EDIT MANUALLY.\n\n"
          << "#ifndef CYBOU_DEVNET_CONSTANTS_H\n"
          << "#define CYBOU_DEVNET_CONSTANTS_H\n\n"
          << "#include <array>\n"
          << "#include <cstdint>\n"
          << "#include <span>\n\n"
          << "namespace cybou::devnet_constants {\n\n"
          << "// Canonical NetworkID bytes (exact Network Public Key)\n"
          << "inline constexpr std::array<unsigned char, " << prov->network_id_bytes.size() << "> NETWORK_ID_BYTES = {\n"
          << FormatByteArrayCpp(prov->network_id_bytes, 4) << "\n};\n\n"
          << "// Canonical Serialized Signed NetworkGenesis bytes\n"
          << "inline constexpr std::array<unsigned char, " << prov->serialized_signed_genesis.size() << "> SIGNED_GENESIS_BYTES = {\n"
          << FormatByteArrayCpp(prov->serialized_signed_genesis, 4) << "\n};\n\n"
          << "// Canonical Serialized CybouState bytes\n"
          << "inline constexpr std::array<unsigned char, " << prov->serialized_genesis_state.size() << "> GENESIS_STATE_BYTES = {\n"
          << FormatByteArrayCpp(prov->serialized_genesis_state, 4) << "\n};\n\n"
          << "// Genesis State Root (BLAKE3-256)\n"
          << "inline constexpr std::array<unsigned char, 32> GENESIS_STATE_ROOT_BYTES = {\n"
          << FormatByteArrayCpp(std::span<const unsigned char>{prov->genesis_state_root.data(), 32}, 4) << "\n};\n\n"
          << "// cybou.cybou Public Identity Constants\n"
          << "inline constexpr std::array<unsigned char, 32> CYBOU_ACCOUNT_ID = {\n"
          << FormatByteArrayCpp(std::span<const unsigned char>{prov->cybou_account_id.Value().data(), 32}, 4) << "\n};\n\n"
          << "inline constexpr std::array<unsigned char, 32> CYBOU_RECOVERY_KEY_ID = {\n"
          << FormatByteArrayCpp(prov->cybou_recovery_key_id, 4) << "\n};\n\n"
          << "inline constexpr std::array<unsigned char, 32> CYBOU_POA_KEY_ID = {\n"
          << FormatByteArrayCpp(prov->cybou_poa_key_id, 4) << "\n};\n\n"
          << "} // namespace cybou::devnet_constants\n\n"
          << "#endif // CYBOU_DEVNET_CONSTANTS_H\n";

        std::ofstream out(constants_header_path);
        if (!out) {
            std::cerr << "Failed to write constants header: " << constants_header_path.string() << '\n';
            return false;
        }
        out << h.str();
    }

    // Cleanse secret memory in stack
    ::cybou::crypto::CleanseMemory(prov->network_entropy.data(), prov->network_entropy.size());
    ::cybou::crypto::CleanseMemory(prov->cybou_entropy.data(), prov->cybou_entropy.size());

    std::cout << "DEVNET provisioned successfully!\n"
              << "Private secrets saved to:  " << private_dir.string() << "\n"
              << "Public constants saved to: " << constants_header_path.string() << "\n"
              << "NetworkID: " << cybou::HexEncode(prov->network_id_bytes) << "\n";

    return true;
}

} // namespace cybou
