#pragma once

#include "base_utils.h"

struct char_data;
struct obj_data;
struct weather_data;
struct room_data;

namespace utils
{
	typedef signed short sh_int;
	typedef unsigned char byte;

	bool is_pc(const char_data& character);
	bool is_npc(const char_data& character);
	bool is_mob(const char_data& character);
	bool is_retired(const char_data& character);
	bool is_mob_flagged(const char_data& mob, long flag);
	bool is_player_flagged(const char_data& character, long flag);
	bool is_preference_flagged(const char_data& character, long flag);
	bool is_player_mode_on(const char_data& character, long flag);
	bool is_affected_by(const char_data& character, long skill_id);

	const char* his_or_her(const char_data& character);
	const char* he_or_she(const char_data& character);
	const char* him_or_her(const char_data& character);

	int get_tactics(const char_data& character);
	void set_tactics(char_data& character, int value);

	int get_condition(const char_data& character, int index);
	void set_condition(char_data& character, int index, sh_int value);
	
	int get_index(const char_data& character);
	const char* get_name(const char_data& character);

	int get_level_a(const char_data& character);
	int get_level_legend_cap(const char_data& character);
	int get_level_b(const char_data& character);

	int get_prof_level(int prof, const char_data& character);
	int get_max_race_prof_level(int prof, const char_data& character);
	void set_prof_level(int prof, char_data& character, sh_int value);
	int get_prof_coof(int prof, const char_data& character);
	int get_prof_points(int prof, const char_data& character);

	// Returns a capped strength score to offset some of the unbalancing impacts of
	// overly high strength scores.
	int get_bal_strength(const char_data& character);

	bool is_evil_race(const char_data& character);

	int get_skill_penalty(const char_data& character);
	int get_dodge_penalty(const char_data& character);

	int get_idnum(const char_data& character);
	bool is_awake(const char_data& character);

	int get_raw_skill(const char_data& character, int skill_index);
	int get_skill(const char_data& character, int skill_index);
	void set_skill(char_data& character, int skill_index, byte value);

	int get_raw_knowledge(const char_data& character, int skill_index);
	int get_knowledge(const char_data& character, int skill_index);
	void set_knowledge(char_data& character, int skill_index, byte value);

	bool can_see(const char_data& character, const weather_data& weather, const room_data& room);

	bool get_carry_weight_limit(const char_data& character);
	bool get_carry_item_limit(const char_data& character);
	bool is_twohanded(const char_data& character);
	bool can_carry_object(const char_data& character, const obj_data& object);
	bool can_see_object(const char_data& character, const obj_data& object, const weather_data& weather, const room_data& room);
	bool can_get_object(const char_data& character, const obj_data& object, const weather_data& weather, const room_data& room);

	bool is_shadow(const char_data& character);

	bool is_race_good(const char_data& character);
	bool is_race_evil(const char_data& character);
	bool is_race_easterling(const char_data& character);
	bool is_race_magi(const char_data& character);

	const char* get_object_string(const char_data& character, const obj_data& object, const weather_data& weather, const room_data& room);
	const char* get_object_name(const char_data& character, const obj_data& object, const weather_data& weather, const room_data& room);

	bool is_good(const char_data& character);
	bool is_evil(const char_data& character);
	bool is_neutral(const char_data& character);

	bool is_hostile_to(const char_data& character, const char_data& victim);
	bool is_rp_race_check(const char_data& character, const char_data& victim);

	bool is_riding(const char_data& character);
	bool is_ridden(const char_data& character);

	const char* get_prof_abbrev(const char_data& character);
	const char* get_race_abbrev(const char_data& character);

	int get_perception(const char_data& character);
	bool is_mental(const char_data& character);

	int get_specialization(const char_data& character);
	void set_specialization(char_data& character, int value);

	bool is_resistant(const char_data& character, int attack_group);
	bool is_vulnerable(const char_data& character, int attack_group);

	bool is_guardian(const char_data& character);
}

