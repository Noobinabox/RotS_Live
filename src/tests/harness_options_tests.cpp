#include "affect_hours_registration.h"
#include "harness_options.h"

#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

namespace {
// A schedule name the options mark as unchanged, to show a refused parse left them alone.
constexpr const char* UNTOUCHED_SCHEDULE = "untouched";

// Owns a command line's words and gives them out as the argv a main() receives.
class CommandLine {
public:
    explicit CommandLine(std::vector<std::string> arguments)
        : words(std::move(arguments))
    {
        pointers.reserve(words.size());
        for (std::string& word : words) {
            pointers.push_back(word.data());
        }
    }

    // The argument count.
    [[nodiscard]] int count() const
    {
        // pointers holds one entry per word of a test's short command line, so it fits an int.
        return static_cast<int>(pointers.size());
    }

    // The argument vector; valid while the command line lives.
    [[nodiscard]] char** vector()
    {
        return pointers.data();
    }

private:
    // The words, owning their characters.
    std::vector<std::string> words;
    // Pointers into words, as argv.
    std::vector<char*> pointers;
};

// Parses arguments and expects a refusal that leaves the options unchanged.
void expect_refused(std::vector<std::string> arguments)
{
    CommandLine command_line(std::move(arguments));
    harness::HarnessOptions options;
    options.schedule_name = UNTOUCHED_SCHEDULE;
    std::string error;

    EXPECT_FALSE(harness::parse_harness_options(
        command_line.count(), command_line.vector(), options, error))
        << "Expected the command line to be refused.";
    EXPECT_EQ(options.schedule_name, UNTOUCHED_SCHEDULE) << "Expected the options unchanged.";
}
} // namespace

TEST(HarnessOptions, ReadsTheScheduleAndAgelandsOptions)
{
    const std::string schedule(harness::AFFECT_HOURS_SCHEDULE_NAME);
    CommandLine command_line({ "ageland_harness", std::string(harness::SCHEDULE_OPTION), schedule,
        "--random-seed", "7", "-d", "testlib", "4321" });
    harness::HarnessOptions options;
    std::string error;

    ASSERT_TRUE(harness::parse_harness_options(
        command_line.count(), command_line.vector(), options, error))
        << error;
    EXPECT_EQ(options.schedule_name, schedule) << "Expected the schedule name.";
    EXPECT_EQ(options.startup.port, 4321) << "Expected the port.";
    EXPECT_EQ(options.startup.dir, "testlib") << "Expected the data directory.";
    ASSERT_TRUE(options.startup.random_seed.has_value()) << "Expected the seed.";
    EXPECT_EQ(*options.startup.random_seed, 7u) << "Expected seed 7.";
}

TEST(HarnessOptions, AcceptsTheScheduleAfterAgelandsOptions)
{
    const std::string schedule(harness::AFFECT_HOURS_SCHEDULE_NAME);
    CommandLine command_line({ "ageland_harness", "-d", "testlib",
        std::string(harness::SCHEDULE_OPTION), schedule, "4321" });
    harness::HarnessOptions options;
    std::string error;

    ASSERT_TRUE(harness::parse_harness_options(
        command_line.count(), command_line.vector(), options, error))
        << error;
    EXPECT_EQ(options.schedule_name, schedule) << "Expected the schedule name.";
    EXPECT_EQ(options.startup.port, 4321) << "Expected the port.";
}

TEST(HarnessOptions, RefusesAMissingSchedule)
{
    expect_refused({ "ageland_harness", "-d", "testlib", "4321" });
}

TEST(HarnessOptions, RefusesARepeatedSchedule)
{
    const std::string option(harness::SCHEDULE_OPTION);
    const std::string schedule(harness::AFFECT_HOURS_SCHEDULE_NAME);
    expect_refused({ "ageland_harness", option, schedule, option, schedule, "4321" });
}

TEST(HarnessOptions, RefusesAScheduleOptionWithNoName)
{
    expect_refused({ "ageland_harness", "-d", "testlib", std::string(harness::SCHEDULE_OPTION) });
}

TEST(HarnessOptions, RefusesAScheduleNameAttachedWithEquals)
{
    const std::string attached = std::string(harness::SCHEDULE_OPTION) + "="
        + std::string(harness::AFFECT_HOURS_SCHEDULE_NAME);
    CommandLine command_line({ "ageland_harness", attached, "4321" });
    harness::HarnessOptions options;
    options.schedule_name = UNTOUCHED_SCHEDULE;
    std::string error;

    EXPECT_FALSE(harness::parse_harness_options(
        command_line.count(), command_line.vector(), options, error))
        << "Expected an attached schedule name to be refused.";
    EXPECT_EQ(options.schedule_name, UNTOUCHED_SCHEDULE) << "Expected the options unchanged.";
    EXPECT_NE(error.find("next argument"), std::string::npos)
        << "Expected the refusal to say the name is the next argument; got: " << error;
}

TEST(HarnessOptions, RefusesAnUnknownScheduleNamingTheKnownOne)
{
    CommandLine command_line(
        { "ageland_harness", std::string(harness::SCHEDULE_OPTION), "fast" });
    harness::HarnessOptions options;
    std::string error;

    EXPECT_FALSE(harness::parse_harness_options(
        command_line.count(), command_line.vector(), options, error))
        << "Expected an unknown schedule to be refused.";
    EXPECT_NE(error.find(std::string(harness::AFFECT_HOURS_SCHEDULE_NAME)), std::string::npos)
        << "Expected the refusal to name the known schedule; got: " << error;
}

TEST(HarnessOptions, RefusesAgelandOptionsThatDoNotParse)
{
    expect_refused({ "ageland_harness", std::string(harness::SCHEDULE_OPTION),
        std::string(harness::AFFECT_HOURS_SCHEDULE_NAME), "4321", "4322" });
}
