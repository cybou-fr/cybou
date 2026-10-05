// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#include <cybou/binary_codec.h>
#include <boost/test/unit_test.hpp>
BOOST_AUTO_TEST_SUITE(cybou_binary_codec_tests)
BOOST_AUTO_TEST_CASE(rejects_unbounded_lengths_truncation_and_non_boolean_flags) {
    const std::array<unsigned char, 4> excessive{0xff, 0xff, 0xff, 0xff};
    cybou::BinaryReader lengths{excessive};
    BOOST_CHECK_THROW(lengths.Bytes(1024), std::length_error);
    const std::array<unsigned char, 1> invalid_flag{2};
    cybou::BinaryReader flags{invalid_flag};
    BOOST_CHECK_THROW(flags.Flag(), std::invalid_argument);
    cybou::BinaryReader truncated{invalid_flag};
    BOOST_CHECK_THROW(truncated.U64(), std::invalid_argument);
    cybou::BinaryWriter bounded{1}; bounded.U8(1);
    BOOST_CHECK_THROW(bounded.U8(2), std::length_error);
    cybou::BinaryReader trailing{invalid_flag};
    BOOST_CHECK_THROW(trailing.Finish(), std::invalid_argument);
}
BOOST_AUTO_TEST_CASE(utf8_rejects_overlong_surrogate_truncated_and_out_of_range_sequences) {
    for (const auto text : {std::string{"\xc0\x80"}, std::string{"\xed\xa0\x80"},
        std::string{"\xf4\x90\x80\x80"}, std::string{"\xe2\x82"}}) {
        BOOST_CHECK(!cybou::IsValidUtf8(text));
        cybou::BinaryWriter writer;
        BOOST_CHECK_THROW(writer.Text(text, 32), std::invalid_argument);
    }
    BOOST_CHECK(cybou::IsValidUtf8("Mail: Привет"));
}
BOOST_AUTO_TEST_SUITE_END()
