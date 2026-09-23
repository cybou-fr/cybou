// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#define BOOST_TEST_MODULE CYBOU Core Test Suite

#include <boost/test/included/unit_test.hpp>

#include <test/util/setup_common.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

/** Redirect debug log to unit_test.log files. */
const std::function<void(const std::string&)> G_TEST_LOG_FUN = [](const std::string& message) {
    static const bool should_log{std::any_of(
        &boost::unit_test::framework::master_test_suite().argv[1],
        &boost::unit_test::framework::master_test_suite().argv[boost::unit_test::framework::master_test_suite().argc],
        [](const char* argument) { return std::string{"DEBUG_LOG_OUT"} == argument; })};
    if (should_log) std::cout << message;
};

const std::function<std::vector<const char*>()> G_TEST_COMMAND_LINE_ARGUMENTS = []() {
    std::vector<const char*> arguments;
    for (int i = 1; i < boost::unit_test::framework::master_test_suite().argc; ++i) {
        arguments.push_back(boost::unit_test::framework::master_test_suite().argv[i]);
    }
    return arguments;
};

const std::function<std::string()> G_TEST_GET_FULL_NAME = []() {
    return boost::unit_test::framework::current_test_case().full_name();
};
