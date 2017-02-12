#pragma once

#include "base_utils.h"
#include <vector>

struct char_data;
struct obj_data;
struct weather_data;
struct room_data;

namespace utils
{
	double get_real_dodge(const char_data& character);
	double get_real_parry(const char_data& character, const weather_data& weather, const room_data& room);
	double get_real_ob(const char_data& character, const weather_data& weather, const room_data& room);

	//============================================================================
	// Returns a vector of all of the characters that are currently engaged with the
	// character passed in.
	//============================================================================
	std::vector<char_data*> get_engaged_characters(const char_data* character, const room_data& room);
}

