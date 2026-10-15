/* schedule_setup.h */

#ifndef SCHEDULE_SETUP_H
#define SCHEDULE_SETUP_H

#include "game_tick_schedule.h"
#include "harness_request_registry.h"

#include <memory>
#include <string>
#include <type_traits>

namespace harness {

// What a schedule's factory builds for one harness run: the schedule, the requests that drive
// it, and the state its ready reply reports. It can be moved but not copied.
struct ScheduleSetup {
    // The schedule each pass is delegated to; null only when the factory could not build it.
    // Held behind a pointer so that moving the setup never moves the schedule the requests
    // borrow.
    std::unique_ptr<GameTickSchedule> schedule;
    // The requests the schedule accepts. Declared after schedule, so they are destroyed before
    // the schedule they may borrow.
    HarnessRequestRegistry requests;
    // The text after the schedule's name in the ready reply, such as "hours=0"; may be empty.
    std::string ready_state;
};

static_assert(!std::is_copy_constructible_v<ScheduleSetup>, "A ScheduleSetup is never copied.");
static_assert(
    std::is_nothrow_move_constructible_v<ScheduleSetup>, "A ScheduleSetup moves without throwing.");

} // namespace harness

#endif /* SCHEDULE_SETUP_H */
