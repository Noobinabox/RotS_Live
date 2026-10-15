/* harness_options.h */

#ifndef HARNESS_OPTIONS_H
#define HARNESS_OPTIONS_H

#include "comm.h"

#include <string>
#include <string_view>

namespace harness {

// The option that names the schedule a harness run uses.
inline constexpr std::string_view SCHEDULE_OPTION = "--schedule";

// The harness binary's command line.
struct HarnessOptions {
    // The schedule the run uses, by name; AFFECT_HOURS_SCHEDULE_NAME is the only one.
    std::string schedule_name;
    // The options the harness binary takes as ageland does.
    StartupOptions startup {};
};

// Reads the harness binary's command line: SCHEDULE_OPTION followed by a schedule name,
// anywhere after the program name, and ageland's own options. Each of argv[0] to
// argv[argc - 1] must be a non-null string. Returns true and fills out_options on success.
// Otherwise leaves out_options unchanged, sets out_error and returns false: when argc < 1 or
// argv is null (out_error is then "no command line"), when the schedule option is missing,
// repeated, has no name or is written with its name after '=', when the name is unknown, or
// when ageland's options do not parse (out_error then holds ageland's message, which may be
// empty).
bool parse_harness_options(
    int argc, char** argv, HarnessOptions& out_options, std::string& out_error);

} // namespace harness

#endif /* HARNESS_OPTIONS_H */
