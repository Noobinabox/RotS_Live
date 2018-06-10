#pragma once

#include "base_utils.h"

struct room_data;
struct weather_data;

namespace utils {
bool is_room_outside(const room_data& room);

bool is_dark(const room_data& room, const weather_data& weather);
bool is_light(const room_data& room, const weather_data& weather);

bool is_sunlit_exit(const weather_data& weather, const room_data& current_room, const room_data& adjacent_room, int door_index);
bool is_shadowy_exit(const room_data& current_room, const room_data& adjacent_room, int door_index);
bool is_room_sunlit(const weather_data& weather, const room_data& room);

bool is_water_room(const room_data& room);
}
