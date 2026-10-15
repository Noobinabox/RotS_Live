#include "affect_hours_registration.h"
#include "comm.h"
#include "harness_control_channel.h"
#include "harness_controlled_schedule.h"
#include "harness_options.h"
#include "schedule_definition.h"
#include "schedule_registry.h"
#include "schedule_setup.h"
#include "server_startup.h"
#include "standard_stream_channel.h"
#include "utils.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {
// The program name the usage text shows when the command line does not supply one.
constexpr const char* DEFAULT_PROGRAM_NAME = "ageland_harness";
} // namespace

int main(int argc, char** argv)
{
    harness::ScheduleRegistry schedules;
    const bool affect_hours_registered = harness::register_affect_hours_schedule(schedules);
    if (!affect_hours_registered) {
        return EXIT_FAILURE;
    }

    harness::HarnessOptions options;
    std::string parse_error;
    if (!harness::parse_harness_options(argc, argv, options, parse_error)) {
        if (!parse_error.empty()) {
            log(parse_error.c_str());
        }
        const bool has_program_name = argc > 0 && argv != nullptr && argv[0] != nullptr;
        const char* const program_name = has_program_name ? argv[0] : DEFAULT_PROGRAM_NAME;
        std::cerr << "Usage: " << program_name << " " << harness::SCHEDULE_OPTION << " "
                  << harness::AFFECT_HOURS_SCHEDULE_NAME
                  << " [-m] [-q] [-r] [-s] [-x] [-d pathname] [-p port #] [--random-seed n]"
                     " [ port # ]\n";
        return EXIT_FAILURE;
    }

    prepare_server_process(options.startup);
    const std::string mode = "Harness: world time advances only on request over standard input "
                             "(schedule "
        + options.schedule_name + ").";
    log(mode.c_str());

    // Untied, so the reading thread's reads never flush standard output while the game thread
    // writes to it.
    std::cin.tie(nullptr);
    // The process's standard streams outlive the channel and its reading thread.
    std::unique_ptr<harness::HarnessControlChannel> channel
        = std::make_unique<harness::StandardStreamChannel>(std::cin, std::cout);

    const harness::ScheduleDefinition* const definition = schedules.find(options.schedule_name);
    if (definition == nullptr) {
        const std::string message = std::string("SYSERR: ") + __func__ + ": the schedule '"
            + options.schedule_name + "' is not registered.";
        log(message.c_str());
        return EXIT_FAILURE;
    }
    harness::ScheduleSetup setup = definition->create_setup();
    if (setup.schedule == nullptr) {
        const std::string message = std::string("SYSERR: ") + __func__ + ": the schedule '"
            + definition->name + "' could not be built.";
        log(message.c_str());
        return EXIT_FAILURE;
    }

    std::shared_ptr<GameTickSchedule> tick_schedule
        = std::make_shared<harness::HarnessControlledSchedule>(
            definition->name, std::move(setup), std::move(channel));
    run_the_game(options.startup.port, std::move(tick_schedule));
    return EXIT_SUCCESS;
}
