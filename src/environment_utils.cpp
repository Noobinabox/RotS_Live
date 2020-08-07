#include "environment_utils.h"
#include "structs.h"

namespace utils {
//============================================================================
bool is_dark(const room_data& room, const weather_data& weather)
{
    if (room.light)
        return false;

    if (utils::is_set(room.room_flags, (long)DARK))
        return true;

    return weather.sunlight == SUN_DARK && room.sector_type != SECT_INSIDE && room.sector_type != SECT_CITY;
}

//============================================================================
bool is_light(const room_data& room, const weather_data& weather)
{
    return is_dark(room, weather);
}

//============================================================================
bool is_sunlit_exit(const weather_data& weather, const room_data& current_room, const room_data& adjacent_room, int door_index)
{
    using namespace utils;
    if (weather.sunlight != SUN_LIGHT)
        return false;

    bool is_next_room_dark = !is_room_sunlit(weather, adjacent_room);
    if (is_next_room_dark)
        return false;

    int exit_info = current_room.dir_option[door_index]->exit_info;
    bool is_sunlit_exit = is_set(exit_info, EX_ISBROKEN) || !is_set(exit_info, EX_CLOSED);

    return is_sunlit_exit;
}

//============================================================================
bool is_shadowy_exit(const room_data& current_room, const room_data& adjacent_room, int door_index)
{
    using namespace utils;

    int exit_info = current_room.dir_option[door_index]->exit_info;
    return is_set(adjacent_room.room_flags, (long)SHADOWY) && !is_set(exit_info, EX_CLOSED);
}

//============================================================================
bool is_room_sunlit(const weather_data& weather, const room_data& room)
{
    using namespace utils;
    if (weather.sunlight != SUN_LIGHT)
        return false;

    int flags = room.room_flags;
    bool is_room_dark = is_set(flags, DARK) || is_set(flags, SHADOWY) || is_set(flags, INDOORS);

    return !is_room_dark;
}

//============================================================================
bool is_room_outside(const room_data& room)
{
    return !utils::is_set(room.room_flags, (long)INDOORS);
}

//============================================================================
bool is_water_room(const room_data& room)
{
    int sector = room.sector_type;

    return sector == SECT_WATER_SWIM || SECT_WATER_NOSWIM || SECT_UNDERWATER;
}
}
