#include "harness_request.h"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace {
// The value out_hours holds before a refused parse, to show the parse left it alone.
constexpr int UNTOUCHED_HOURS = 42;

// The refusal text for a line whose hour count is not usable.
std::string bad_hours_error(std::string_view line)
{
    return std::string(line) + ": hours must be a whole number from 1 to "
        + std::to_string(harness::MAX_REQUESTED_HOURS);
}

// The request line asking for hours_text game hours.
std::string advance_line(std::string_view hours_text)
{
    return std::string(harness::ADVANCE_HOURS_REQUEST) + " " + std::string(hours_text);
}
} // namespace

TEST(HarnessRequest, AcceptsAdvanceHoursWithAWholeNumberOfHours)
{
    int hours = 0;
    std::string error;

    EXPECT_TRUE(harness::parse_harness_request(advance_line("3"), hours, error)) << error;
    EXPECT_EQ(hours, 3) << "Expected the hour count to be read.";
}

TEST(HarnessRequest, AcceptsTheSmallestAndLargestHourCounts)
{
    int hours = 0;
    std::string error;

    EXPECT_TRUE(harness::parse_harness_request(advance_line("1"), hours, error)) << error;
    EXPECT_EQ(hours, 1) << "Expected 1 hour.";
    const std::string largest = std::to_string(harness::MAX_REQUESTED_HOURS);
    EXPECT_TRUE(harness::parse_harness_request(advance_line(largest), hours, error)) << error;
    EXPECT_EQ(hours, harness::MAX_REQUESTED_HOURS) << "Expected the largest hour count.";
}

TEST(HarnessRequest, IgnoresOneTrailingCarriageReturn)
{
    int hours = 0;
    std::string error;

    EXPECT_TRUE(harness::parse_harness_request(advance_line("2") + "\r", hours, error)) << error;
    EXPECT_EQ(hours, 2) << "Expected a CRLF line to read like an LF line.";
}

TEST(HarnessRequest, RefusesAnHourCountOutOfRange)
{
    const std::vector<std::string> lines
        = { advance_line("0"), advance_line(std::to_string(harness::MAX_REQUESTED_HOURS + 1)) };
    for (const std::string& line : lines) {
        int hours = UNTOUCHED_HOURS;
        std::string error;
        EXPECT_FALSE(harness::parse_harness_request(line, hours, error)) << "Accepted: " << line;
        EXPECT_EQ(error, bad_hours_error(line)) << "Unexpected refusal for: " << line;
        EXPECT_EQ(hours, UNTOUCHED_HOURS) << "Expected out_hours untouched for: " << line;
    }
}

TEST(HarnessRequest, RefusesASignPaddingLeadingZerosOrTrailingText)
{
    const std::vector<std::string> hour_texts = { "-1", "+1", " 1", "1 ", "01", "1x", "x", "" };
    for (const std::string& hour_text : hour_texts) {
        const std::string line = advance_line(hour_text);
        int hours = UNTOUCHED_HOURS;
        std::string error;
        EXPECT_FALSE(harness::parse_harness_request(line, hours, error)) << "Accepted: " << line;
        EXPECT_EQ(error, bad_hours_error(line)) << "Unexpected refusal for: " << line;
        EXPECT_EQ(hours, UNTOUCHED_HOURS) << "Expected out_hours untouched for: " << line;
    }

    int hours = UNTOUCHED_HOURS;
    std::string error;
    const std::string bare_name(harness::ADVANCE_HOURS_REQUEST);
    EXPECT_FALSE(harness::parse_harness_request(bare_name, hours, error))
        << "Expected a request with no hour count to be refused.";
    EXPECT_EQ(error, bad_hours_error(bare_name)) << "Unexpected refusal for the bare name.";
}

TEST(HarnessRequest, RefusesAnUnknownRequestByItsWholeName)
{
    const std::vector<std::string> lines
        = { "status", std::string(harness::ADVANCE_HOURS_REQUEST) + "3" };
    for (const std::string& line : lines) {
        int hours = UNTOUCHED_HOURS;
        std::string error;
        EXPECT_FALSE(harness::parse_harness_request(line, hours, error)) << "Accepted: " << line;
        EXPECT_EQ(error, line + ": unknown request") << "Unexpected refusal for: " << line;
    }
}

TEST(HarnessRequest, RefusesAnEmptyLine)
{
    int hours = UNTOUCHED_HOURS;
    std::string error;

    EXPECT_FALSE(harness::parse_harness_request("", hours, error)) << "Accepted an empty line.";
    EXPECT_EQ(error, "empty request") << "Unexpected refusal for an empty line.";
    EXPECT_FALSE(harness::parse_harness_request("\r", hours, error)) << "Accepted a bare CR.";
    EXPECT_EQ(error, "empty request") << "Unexpected refusal for a bare CR.";
}

TEST(HarnessRequest, RefusesALineLongerThanTheLimitWithoutQuotingIt)
{
    const std::string overlong(harness::MAX_REQUEST_LINE_LENGTH + 1, 'a');
    int hours = UNTOUCHED_HOURS;
    std::string error;

    EXPECT_FALSE(harness::parse_harness_request(overlong, hours, error))
        << "Accepted an overlong line.";
    EXPECT_EQ(error,
        "request longer than " + std::to_string(harness::MAX_REQUEST_LINE_LENGTH) + " characters")
        << "Unexpected refusal for an overlong line.";
}
