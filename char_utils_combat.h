#pragma once

#include "base_utils.h"

struct char_data;
struct obj_data;
struct weather_data;
struct room_data;

namespace combat_utils
{
	double get_real_dodge(const char_data& character);
	double get_real_parry(const char_data& character, const weather_data& weather, const room_data& room);
	double get_real_ob(const char_data& character, const weather_data& weather, const room_data& room);
}

