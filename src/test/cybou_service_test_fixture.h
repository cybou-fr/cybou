// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_SERVICE_TEST_FIXTURE_H
#define CYBOU_SERVICE_TEST_FIXTURE_H

#include <cybou/identity_service.h>
#include <cybou/crypto/sha256.h>
#include <cybou/network_genesis.h>
#include <cybou/p2p/peer_admission.h>
#include <cybou/p2p/session.h>
#include <test/cybou_test_helpers.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

inline std::shared_ptr<const cybou::p2p::PeerAdmissionPolicy> TestPeerAdmissionPolicy()
{
    // Synthetic Geo input for component tests, through the real verified policy.
    // No alternate network profile or runtime admission bypass is involved.
    static const auto policy = [] {
        const std::string csv{"10.0.0.0,10.255.255.255,FR\n127.0.0.0,127.255.255.255,FR\n"
            "172.16.0.0,172.31.255.255,FR\n192.168.0.0,192.168.255.255,FR\n::1,::1,FR\n"};
        std::array<unsigned char, 32> hash{};
        if (!cybou::crypto::ComputeSha256({cybou::crypto::Sha256Bytes(csv)}, hash.data()))
            throw std::runtime_error("test Geo hash failed");
        const auto path = std::filesystem::temp_directory_path() /
            ("cybou-unit-geo-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".csv");
        { std::ofstream output{path, std::ios::binary}; output << csv; }
        const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
        const std::chrono::year_month_day date{today};
        const auto dataset = cybou::p2p::FrenchIpDataset::LoadDbIpCountryCsv(path, hash, date.year()/date.month(), today);
        std::filesystem::remove(path);
        if (!dataset) throw std::runtime_error("test Geo dataset failed validation");
        return std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(cybou::p2p::PeerAdmissionPolicy::Public(dataset));
    }();
    return policy;
}

struct CybouServiceTestFixture {
    std::array<unsigned char, 32> validator_seed{};
    cybou::CybouState genesis;
    cybou::VerifiedNetworkGenesis definition{cybou::CreateTestNetworkGenesis(cybou::CreateTestGenesisState(),
        cybou::TestPoaFinalizerPublicKey(), cybou::TestNetworkPublicKey())};
    std::unique_ptr<cybou::CybouNodeRuntime> runtime;
    std::filesystem::path directory;
    std::vector<std::filesystem::path> vaults;

    explicit CybouServiceTestFixture(unsigned char seed_byte = 0x72)
    {
        static std::atomic<unsigned> sequence{0};
        directory = std::filesystem::temp_directory_path() /
            ("cybou-service-integration-" + std::to_string(sequence.fetch_add(1)));
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        validator_seed[0] = seed_byte;
        genesis = cybou::CreateTestGenesisState();
        definition = cybou::CreateTestNetworkGenesis(genesis, cybou::TestPoaFinalizerPublicKey(seed_byte), cybou::TestNetworkPublicKey(seed_byte));
        definition = cybou::WithTestGenesisParameters(definition, [](auto& params) { params.account_creation_work_bits = 0; });
        definition = cybou::WithTestGenesisParameters(definition, [](auto& params) { params.name_claim_work_bits = 0; });
        cybou::NodeRuntimeConfig config{
            .network_genesis = definition,
            .data_dir = directory / "runtime",
            .poa_finalizer_recovery_entropy = validator_seed,
            .memory_only = true,
            .wipe_data = true,
            .peer_admission_policy = TestPeerAdmissionPolicy(),
        };
        runtime = std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
        if (!runtime->InitializeGenesis(genesis)) throw std::runtime_error("genesis initialization failed");
    }

    ~CybouServiceTestFixture()
    {
        for (const auto& path : vaults) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }

    std::unique_ptr<cybou::CybouIdentityService> CreateIdentity(const std::string& filename)
    {
        const auto path = directory / filename;
        std::filesystem::remove(path);
        vaults.push_back(path);
        auto service = std::make_unique<cybou::CybouIdentityService>(*runtime, path);
        if (!service->PrepareNewIdentity() ||
            !service->CreateIdentitySync("correct horse battery staple").success) {
            throw std::runtime_error("account creation failed");
        }
        return service;
    }

    bool HandshakeAsPeer(cybou::p2p::PeerSession& session, const cybou::p2p::Hello& hello) const
    {
        return session.Handshake(hello);
    }

};

#endif
