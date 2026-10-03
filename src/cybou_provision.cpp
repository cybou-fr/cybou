// Offline-only network creation and read-only verification. Never linked into cybou.
#include <cybou/provision.h>
#include <cybou/official_networks.h>
#include <cybou/official_devnet_constants.h>
#include <cybou/secret_file.h>
#include <cybou/secret32.h>
#include <cybou/hex.h>
#include <cybou/crypto/cleanse.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <source_location>

namespace {
std::string Field(const std::string& text, const std::string& name)
{
    std::istringstream lines{text};
    std::string line;
    std::optional<std::string> value;
    while (std::getline(lines, line)) {
        if (!line.starts_with(name + ": ")) continue;
        if (value) throw std::runtime_error("duplicate secret field");
        value = line.substr(name.size() + 2);
        if (!value->empty() && value->back() == '\r') value->pop_back();
    }
    if (!value) throw std::runtime_error("missing secret field");
    return *value;
}
struct PrivateText {
    std::string text;
    explicit PrivateText(const std::filesystem::path& path) {
        auto bytes = cybou::ReadSecretFile(path, 16384);
        if (!bytes) throw std::runtime_error("cannot read private file with secure permissions");
        text.assign(bytes->begin(), bytes->end());
        cybou::crypto::CleanseMemory(bytes->data(), bytes->size());
    }
    ~PrivateText() { cybou::crypto::CleanseMemory(text.data(), text.size()); }
};
cybou::Secret32 Seed(const std::string& text, const std::string& field)
{
    auto value = Field(text, field);
    auto hash = cybou::ParseHash256UserHex(value);
    cybou::crypto::CleanseMemory(value.data(), value.size());
    if (!hash) throw std::runtime_error("invalid seed encoding");
    std::array<unsigned char, 32> bytes;
    std::copy(hash->begin(), hash->end(), bytes.begin());
    cybou::Secret32 seed{bytes};
    cybou::crypto::CleanseMemory(bytes.data(), bytes.size());
    cybou::crypto::CleanseMemory(hash->data(), hash->size());
    return seed;
}
void VerifyMnemonic(const std::string& text, const cybou::Secret32& seed)
{
    const auto marker = text.find("MNEMONIC_PHRASE:\n");
    if (marker == std::string::npos) throw std::runtime_error("missing mnemonic");
    std::istringstream words{text.substr(marker + 17, text.find('\n', marker + 17) - marker - 17)};
    cybou::RecoveryWords parsed;
    for (auto& word : parsed) if (!(words >> word)) throw std::runtime_error("invalid mnemonic");
    std::string extra;
    if (words >> extra) throw std::runtime_error("invalid mnemonic");
    auto entropy = cybou::DecodeRecoveryWords(parsed);
    const bool match = entropy && *entropy == seed.Get();
    for (auto& word : parsed) cybou::crypto::CleanseMemory(word.data(), word.size());
    if (entropy) cybou::crypto::CleanseMemory(entropy->data(), entropy->size());
    if (!match) throw std::runtime_error("mnemonic does not match saved seed");
}
void Require(bool ok, std::source_location site = std::source_location::current()) { if (!ok) throw std::runtime_error("private material does not match compiled DEVNET (check " + std::to_string(site.line()) + ")"); }
void Verify(const std::filesystem::path& dir)
{
    using namespace cybou;
    const auto& network = RequireOfficialNetwork("devnet");
    PrivateText root{dir / "network_root_secret.txt"}, identity{dir / "cybou_identity_secret.txt"};
    const auto root_seed = Seed(root.text, "NETWORK_SEED_HEX");
    const auto identity_seed = Seed(identity.text, "CYBOU_SEED_HEX");
    VerifyMnemonic(root.text, root_seed);
    VerifyMnemonic(identity.text, identity_seed);
    const auto root_key = DeriveIdentityPublicKey(root_seed.Get(), IdentityKeyPurpose::NETWORK_ROOT);
    const auto poa_key = DeriveIdentityPublicKey(identity_seed.Get(), IdentityKeyPurpose::POA_FINALIZER);
    const auto recovery = DeriveIdentityPublicKey(identity_seed.Get(), IdentityKeyPurpose::RECOVERY_ROOT);
    Require(root_key && *root_key == network.genesis.GetNetworkPublicKey());
    Require(poa_key && *poa_key == network.genesis.GetPoaPublicKey());
    Require(recovery.has_value());
    const auto recovery_id = ComputeRecoveryKeyId(*recovery);
    const auto poa_id = ComputePoaFinalizerKeyId(*poa_key);
    Require(recovery_id && *recovery_id == devnet_constants::CYBOU_RECOVERY_KEY_ID);
    Require(poa_id && *poa_id == devnet_constants::CYBOU_POA_KEY_ID);
    Require(Field(identity.text, "ACCOUNT_ID_HEX") == HexEncode(devnet_constants::CYBOU_ACCOUNT_ID));
    Require(Field(root.text, "NETWORK_ID_HEX") == HexEncode(devnet_constants::NETWORK_ID_BYTES));
    Require(Field(root.text, "NETWORK_ED25519_HEX") == HexEncode(root_key->ed25519));
    Require(Field(root.text, "NETWORK_ML_DSA_HEX") == HexEncode(root_key->ml_dsa));
    Require(Field(identity.text, "RECOVERY_KEY_ID_HEX") == HexEncode(*recovery_id));
    Require(Field(identity.text, "POA_FINALIZER_KEY_ID_HEX") == HexEncode(*poa_id));
    Require(Field(identity.text, "POA_ED25519_HEX") == HexEncode(poa_key->ed25519));
    Require(Field(identity.text, "POA_ML_DSA_HEX") == HexEncode(poa_key->ml_dsa));
    const auto allocation = network.genesis_state.genesis_allocations.find(*recovery_id);
    Require(allocation != network.genesis_state.genesis_allocations.end());
    Require(allocation->second.label == CENTRAL_AUTHORITY_NAME && allocation->second.balance == 100'000'000 &&
        allocation->second.authority == 1'000'001);
    Require(CybouStateHash(network.genesis_state) == std::optional<Hash256>{network.genesis.GetGenesisStateRoot()});
    std::cout << "DEVNET verified: compiled signed genesis, state, Network Root, PoA, Recovery, AccountID and allocation match.\n";
}
}
int main(int argc, char** argv)
{
    try {
        if (argc == 3 && std::string_view{argv[1]} == "verify-devnet") { Verify(argv[2]); return 0; }
        if (argc == 4 && std::string_view{argv[1]} == "create-devnet") {
            return cybou::ProvisionDevnet(argv[2], argv[3]) ? 0 : 1;
        }
        std::cerr << "Offline use only:\n  cybou-provision verify-devnet PRIVATE_DIR\n"
                     "  cybou-provision create-devnet NEW_PRIVATE_DIR NEW_CONSTANTS_HEADER\n";
        return 2;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
