// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/crypto/sha256.h>
#include <cybou/p2p/peer_admission.h>

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <fstream>
#include <span>

BOOST_AUTO_TEST_SUITE(cybou_peer_admission_tests)

namespace {
std::filesystem::path UniquePath()
{
    static std::atomic<unsigned> sequence{0};
    return std::filesystem::temp_directory_path() / ("cybou-peer-admission-" +
        std::to_string(sequence.fetch_add(1)));
}

std::array<unsigned char, 32> Hash(const std::string& bytes)
{
    std::array<unsigned char, 32> digest{};
    BOOST_REQUIRE(cybou::crypto::ComputeSha256({std::span<const unsigned char>{
        reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()}}, digest.data()));
    return digest;
}

void Write(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    BOOST_REQUIRE(output.good());
}
}

BOOST_AUTO_TEST_CASE(dataset_integrity_and_prefix_matching_are_fail_closed)
{
    const auto path = UniquePath();
    std::filesystem::remove_all(path);
    const std::string bytes{"192.0.2.0,192.0.2.255,FR\n2001:db8::,2001:db8::ffff,FR\n"
        "198.51.100.0,198.51.100.255,US\n"};
    Write(path, bytes);
    const auto dataset = cybou::p2p::FrenchIpDataset::LoadDbIpCountryCsv(path, Hash(bytes));
    BOOST_REQUIRE(dataset);
    BOOST_CHECK(dataset->IsFrench("192.0.2.1"));
    BOOST_CHECK(dataset->IsFrench("::ffff:192.0.2.1"));
    BOOST_CHECK(dataset->IsFrench("2001:db8::abcd"));
    BOOST_CHECK(!dataset->IsFrench("192.0.3.1"));
    BOOST_CHECK(!dataset->IsFrench("not-an-ip"));

    auto wrong_digest = Hash(bytes);
    wrong_digest[0] ^= 1;
    BOOST_CHECK(!cybou::p2p::FrenchIpDataset::Load(path, wrong_digest));

    const std::string malformed{"192.0.2.0,192.0.2.255,FRA\n"};
    Write(path, malformed);
    BOOST_CHECK(!cybou::p2p::FrenchIpDataset::LoadDbIpCountryCsv(path, Hash(malformed)));
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_CASE(public_policy_fails_closed_and_lab_only_allows_private_routes)
{
    const auto path = UniquePath();
    std::filesystem::remove_all(path);
    const std::string bytes{"198.51.100.0,198.51.100.255,FR\n"};
    Write(path, bytes);
    const auto dataset = cybou::p2p::FrenchIpDataset::LoadDbIpCountryCsv(path, Hash(bytes));
    BOOST_REQUIRE(dataset);

    const auto public_policy = cybou::p2p::PeerAdmissionPolicy::Public(dataset);
    BOOST_CHECK(public_policy.Ready());
    BOOST_CHECK(public_policy.Allows("198.51.100.42"));
    BOOST_CHECK(!public_policy.Allows("127.0.0.1"));
    BOOST_CHECK(!cybou::p2p::PeerAdmissionPolicy::Public(nullptr).Ready());
    BOOST_CHECK(!cybou::p2p::PeerAdmissionPolicy::Public(nullptr).Allows("198.51.100.42"));

    const auto lab_policy = cybou::p2p::PeerAdmissionPolicy::Lab();
    BOOST_CHECK(lab_policy.Ready());
    BOOST_CHECK(lab_policy.Allows("127.0.0.1"));
    BOOST_CHECK(lab_policy.Allows("10.0.0.1"));
    BOOST_CHECK(lab_policy.Allows("fc00::1"));
    BOOST_CHECK(!lab_policy.Allows("198.51.100.42"));
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_SUITE_END()
