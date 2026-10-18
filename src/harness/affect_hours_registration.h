/* affect_hours_registration.h */

#ifndef AFFECT_HOURS_REGISTRATION_H
#define AFFECT_HOURS_REGISTRATION_H

#include "schedule_registry.h"

#include <string_view>

namespace harness {

// The name the affect-hours schedule is registered under.
inline constexpr std::string_view AFFECT_HOURS_SCHEDULE_NAME = "affect-hours";
// The name the affect-hours schedule without regeneration is registered under.
inline constexpr std::string_view AFFECT_HOURS_WITHOUT_REGEN_SCHEDULE_NAME
    = "affect-hours-without-regen";

// Registers the affect-hours schedule under AFFECT_HOURS_SCHEDULE_NAME. Each setup its factory
// builds holds a fresh AffectHoursSchedule built with Regeneration::On. Nothing advances it until
// an AdvanceHoursRequest grants it hours, and its ready state is "hours=0". Returns false, with
// the refusal logged, when the registry refuses it.
[[nodiscard]] bool register_affect_hours_schedule(ScheduleRegistry& registry);

// Registers the same schedule without regeneration under
// AFFECT_HOURS_WITHOUT_REGEN_SCHEDULE_NAME: its setups match register_affect_hours_schedule()'s,
// except that no pass regenerates hit points, moves or mana. Returns false, with the refusal
// logged, when the registry refuses it.
[[nodiscard]] bool register_affect_hours_without_regen_schedule(ScheduleRegistry& registry);

} // namespace harness

#endif /* AFFECT_HOURS_REGISTRATION_H */
