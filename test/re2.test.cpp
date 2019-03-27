#include "pch.h"
#include "re2/re2.h"

using namespace std::literals;

namespace re2::test
{
    TEST(Match, FullMatch) {
        int i;
        std::string s;
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", &s, &i));
        EXPECT_EQ(s, "ruby");
        EXPECT_EQ(i, 1234);
        EXPECT_FALSE(RE2::FullMatch("ruby", "(.+)", &i));
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", &s));
        i = 0;
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", "(\\w+):(\\d+)", (void*)NULL, &i));
        EXPECT_EQ(i, 1234);
        EXPECT_FALSE(RE2::FullMatch("ruby:123456789123", "(\\w+):(\\d+)", &s, &i)); // integer overflow
        EXPECT_TRUE(RE2::FullMatch("TraceDb 2018-12-22 22h29m44s", "TraceDb \\d{4}-\\d{2}-\\d{2} \\d{2}h\\d{2}m\\d{2}s"));
    }

    TEST(Match, PartialMatch) {
        auto s = "NewYork_c5r4_qp42_dash11.m4s";
        auto qp = 0;
        std::string index;
        EXPECT_TRUE(RE2::PartialMatch(s, "_qp(\\d{2})_dash(\\w+)\\.", &qp, &index));
        EXPECT_EQ(qp, 42);
        EXPECT_EQ(index, "11");
        qp = 0;
        EXPECT_TRUE(RE2::PartialMatch(s, "_qp(\\d{2})_", &qp));
        EXPECT_EQ(qp, 42);
    }

    TEST(Re2, PreCompiled) {
        int i;
        std::string s;
        RE2 re("(\\w+):(\\d+)");
        EXPECT_TRUE(re.ok()); // compiled; if not, see re.error();
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", re, &s, &i));
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", re, &s));
        EXPECT_TRUE(RE2::FullMatch("ruby:1234", re, (void*)NULL, &i));
        EXPECT_FALSE(RE2::FullMatch("ruby:123456789123", re, &s, &i));
    }

    TEST(Re2, Option) {
        RE2 re("(ab", RE2::Quiet); // don't write to stderr for parser failure
        EXPECT_FALSE(re.ok()); // can check re.error() for details
    }

    TEST(Re2, Replace) {
        {
            auto str = "tile9-576p-1500kbps_dash$Number$.m4s"s;
            EXPECT_TRUE(RE2::Replace(&str, "\\$Number\\$", folly::to<std::string>(666)));
            EXPECT_EQ(str, "tile9-576p-1500kbps_dash666.m4s");
        }
        {
            auto str = "tile9-576p-1500kbps_dash$Number$$Number$.m4s"s;
            EXPECT_TRUE(RE2::Replace(&str, "\\$Number\\$", folly::to<std::string>(666)));
            EXPECT_EQ(str, "tile9-576p-1500kbps_dash666$Number$.m4s");
            EXPECT_TRUE(RE2::Replace(&str, "\\$Number\\$", folly::to<std::string>(666)));
            EXPECT_EQ(str, "tile9-576p-1500kbps_dash666666.m4s");
            EXPECT_FALSE(RE2::Replace(&str, "\\$Number\\$", folly::to<std::string>(666)));
        }
        {
            auto str = "banana"s;
            EXPECT_TRUE(RE2::GlobalReplace(&str, "ana", folly::to<std::string>(1)));
            EXPECT_EQ(str, "b1na");
        }
        {
            auto str = "banana"s;
            EXPECT_TRUE(RE2::GlobalReplace(&str, "an", folly::to<std::string>(1)));
            EXPECT_EQ(str, "b11a");
        }
    }

    TEST(Re2, Extract) {
        auto str = "tile9-576p-1500kbps_dash$Number$$Number$.m4s"s;
        auto extract = "123"s;
        EXPECT_TRUE(RE2::Extract(str, "\\$Number\\$", folly::to<std::string>(666), &extract));
        EXPECT_EQ(str, "tile9-576p-1500kbps_dash$Number$$Number$.m4s");
        EXPECT_EQ(extract, "123");
        EXPECT_EQ(extract, "666");
        EXPECT_TRUE(RE2::Extract(str, "\\$Number\\$", folly::to<std::string>(666), &extract));
        EXPECT_TRUE(RE2::Extract(str, "\\$Number\\$", folly::to<std::string>(666), &extract));
        EXPECT_EQ(extract, "666");
    }

    TEST(Re2, QuoteMeta) {
        EXPECT_EQ(RE2::QuoteMeta(".-?$"), "\\.\\-\\?\\$");
        EXPECT_EQ(RE2::QuoteMeta("123abc"), "123abc");
    }

    TEST(Re2, Consume) {
        {
            std::string str{ "abcd12b456" };
            re2::StringPiece input{ str };
            std::string consume;
            auto index = 0;
            while (RE2::Consume(&input, "(\\w+?)b", &consume) && ++index) {
                if (index == 1) {
                    EXPECT_EQ(input.data(), "cd12b456");
                    EXPECT_EQ(consume, "a");
                } else if (index == 2) {
                    EXPECT_EQ(input.data(), "456");
                    EXPECT_EQ(consume, "cd12");
                } else {
                    FAIL();
                }
            }
        }
        {
            RE2 r{ "\\s*(\\w+)" }; // matches a word, possibly proceeded by whitespace
            std::string word;
            std::string s{ "   aaa b!@#$@#$cccc" };
            re2::StringPiece input(s);
            ASSERT_TRUE(RE2::Consume(&input, r, &word));
            ASSERT_EQ(word, "aaa") << " input: " << input;
            ASSERT_TRUE(RE2::Consume(&input, r, &word));
            ASSERT_EQ(word, "b") << " input: " << input;
            ASSERT_FALSE(RE2::Consume(&input, r, &word)) << " input: " << input;
        }
    }

    TEST(Re2, FindAndConsume) {
        std::string str{ "abcd12b456" };
        re2::StringPiece input{ str };
        std::string consume;
        auto index = 0;
        while (RE2::FindAndConsume(&input, "(.b)", &consume) && ++index) {
            if (index == 1) {
                EXPECT_STREQ(input.data(), "cd12b456");
                EXPECT_EQ(consume, "ab");
            } else if (index == 2) {
                EXPECT_STREQ(input.data(), "456");
                EXPECT_EQ(consume, "2b");
            } else {
                FAIL();
            }
        }
    }
}
