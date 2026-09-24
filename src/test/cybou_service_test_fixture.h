// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_SERVICE_TEST_FIXTURE_H
#define CYBOU_SERVICE_TEST_FIXTURE_H

#include <cybou/identity_service.h>
#include <cybou/network_definition.h>
#include <cybou/validator.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

struct CybouServiceTestFixture {
    std::array<unsigned char, 32> validator_seed{};
    cybou::CybouState genesis;
    cybou::CybouNetworkDefinition definition;
    std::unique_ptr<cybou::CybouNodeRuntime> runtime;
    std::filesystem::path directory;
    std::vector<std::filesystem::path> vaults;

    explicit CybouServiceTestFixture(unsigned char seed_byte = 0x72)
    {
        static std::atomic<unsigned> sequence{0};
        directory = std::filesystem::temp_directory_path() /
            ("cybou-service-integration-" + std::to_string(sequence.fetch_add(1)));
        std::filesystem::create_directories(directory);
        validator_seed[0] = seed_byte;
        const auto pair = cybou::GenerateValidatorKeyPair(validator_seed);
        if (!pair) throw std::runtime_error("validator key generation failed");
        genesis = cybou::CreateDevGenesisState(pair->public_key);
        definition = cybou::CreateDevNetworkDefinition(genesis);
        definition.protocol_parameters.account_creation_work_bits = 0;
        cybou::NodeRuntimeConfig config{
            .network_definition = definition,
            .data_dir = directory / "runtime",
            .validator_private_key = validator_seed,
            .memory_only = true,
            .wipe_data = true,
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
};

#endif
