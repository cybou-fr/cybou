// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_TEST_SETUP_H
#define CYBOU_TEST_SETUP_H

#include <filesystem>
#include <random>
#include <string>

/** Minimal per-case scratch directory for CYBOU core tests. */
struct CybouTestSetup {
    std::filesystem::path m_data_dir;

    CybouTestSetup()
    {
        static std::random_device random;
        std::string suffix;
        suffix.reserve(32);
        for (int i = 0; i < 4; ++i) {
            suffix += std::to_string(random());
        }
        m_data_dir = std::filesystem::temp_directory_path() / "cybou-core-tests" / suffix;
        std::filesystem::create_directories(m_data_dir);
    }

    ~CybouTestSetup()
    {
        std::error_code error;
        std::filesystem::remove_all(m_data_dir, error);
    }
};

#endif
