/* harness_options.cpp */

#include "harness_options.h"

#include "affect_hours_schedule.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace harness {

//============================================================================
bool parse_harness_options(
    int argc, char** argv, HarnessOptions& out_options, std::string& out_error)
{
    if (argc < 1 || argv == nullptr) {
        out_error = "no command line";
        return false;
    }

    // ageland's options, with the program name first, for parse_startup_options.
    std::vector<char*> startup_arguments;
    startup_arguments.reserve(static_cast<std::size_t>(argc));
    startup_arguments.push_back(argv[0]);
    std::optional<std::string> schedule_name;
    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        const std::string_view argument = argv[argument_index];
        if (argument != SCHEDULE_OPTION) {
            startup_arguments.push_back(argv[argument_index]);
            continue;
        }

        if (schedule_name.has_value()) {
            out_error = std::string(SCHEDULE_OPTION) + " is given more than once";
            return false;
        }
        if (argument_index + 1 >= argc) {
            out_error = std::string(SCHEDULE_OPTION) + " needs a schedule name";
            return false;
        }
        ++argument_index;
        schedule_name = std::string(argv[argument_index]);
    }

    if (!schedule_name.has_value()) {
        out_error = std::string(SCHEDULE_OPTION) + " is required";
        return false;
    }
    if (*schedule_name != AffectHoursSchedule::NAME) {
        out_error = "unknown schedule '" + *schedule_name + "'; the known schedule is "
            + std::string(AffectHoursSchedule::NAME);
        return false;
    }

    StartupOptions startup {};
    std::string startup_error;
    // startup_arguments holds at most argc entries, so its size fits an int.
    const int startup_argument_count = static_cast<int>(startup_arguments.size());
    if (!parse_startup_options(
            startup_argument_count, startup_arguments.data(), &startup, &startup_error)) {
        out_error = startup_error;
        return false;
    }

    out_options.schedule_name = std::move(*schedule_name);
    out_options.startup = std::move(startup);
    return true;
}

} // namespace harness
