#include "affect_hours_schedule.h"
#include "comm.h"
#include "harness_control_channel.h"
#include "harness_controlled_schedule.h"
#include "harness_options.h"
#include "server_startup.h"
#include "standard_stream_channel.h"
#include "utils.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

int main(int argc, char** argv)
{
    harness::HarnessOptions options;
    std::string parse_error;
    if (!harness::parse_harness_options(argc, argv, options, parse_error)) {
        if (!parse_error.empty()) {
            log(parse_error.c_str());
        }
        std::cerr << "Usage: " << argv[0] << " " << harness::SCHEDULE_OPTION << " "
                  << harness::AffectHoursSchedule::NAME
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
    std::shared_ptr<GameTickSchedule> tick_schedule
        = std::make_shared<harness::HarnessControlledSchedule>(std::move(channel));
    run_the_game(options.startup.port, std::move(tick_schedule));
    return EXIT_SUCCESS;
}
