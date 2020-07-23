#pragma once

#include "base_utils.h"
#include "structs.h"

struct char_data;
struct obj_data;
struct weather_data;
struct room_data;
struct affected_type;

namespace utils {
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
affected_type* is_affected_by_spell(char_data& character, int skill_id);

const char* his_or_her(const char_data& character);
const char* he_or_she(const char_data& character);
const char* him_or_her(const char_data& character);

int get_tactics(const char_data& character);
void set_tactics(char_data& character, int value);

int get_shooting(const char_data& character);
void set_shooting(char_data& character, int value);

int get_casting(const char_data& character);
void set_casting(char_data& character, int value);

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
int get_race(const char_data& character);
int get_highest_coeffs(const char_data& character);

// Returns a capped strength score to offset some of the unbalancing impacts of
// overly high strength scores.
int get_bal_strength(const char_data& character);
double get_bal_strength_d(const char_data& character);

bool is_evil_race(const char_data& character);

int get_worn_weight(const char_data& character);
int get_encumbrance_weight(const char_data& character);
int get_encumbrance(const char_data& character);
int get_leg_encumbrance(const char_data& character);
int get_skill_penalty(const char_data& character);
int get_dodge_penalty(const char_data& character);

long get_idnum(const char_data& character);
bool is_awake(const char_data& character);
int get_ranking_tier(const char_data& character);
int get_ranking_tier(int ranking);

int get_raw_skill(const char_data& character, int skill_index);
int get_skill(const char_data& character, int skill_index);
void set_skill(char_data& character, int skill_index, byte value);

int get_raw_knowledge(const char_data& character, int skill_index);
int get_knowledge(const char_data& character, int skill_index);
void set_knowledge(char_data& character, int skill_index, byte value);

bool can_see(const char_data& character, const weather_data& weather, const room_data& room);

int get_carry_weight_limit(const char_data& character);
int get_carry_item_limit(const char_data& character);
bool is_twohanded(const char_data& character);
bool can_carry_object(const char_data& character, const obj_data& object);
bool can_see_object(const char_data& character, const obj_data& object, const weather_data& weather, const room_data& room);
bool can_get_object(const char_data& character, const obj_data& object, const weather_data& weather, const room_data& room);

bool is_shadow(const char_data& character);

bool is_race_good(const char_data& character);
bool is_race_evil(const char_data& character);
bool is_race_easterling(const char_data& character);
bool is_race_magi(const char_data& character);
bool is_race_haradrim(const char_data& character);

bool is_race_good(int race);
bool is_race_evil(int race);
bool is_race_easterling(int race);
bool is_race_magi(int race);
bool is_race_haradrim(int race);

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

game_types::player_specs get_specialization(const char_data& character);
void set_specialization(char_data& character, game_types::player_specs value);

bool is_resistant(const char_data& character, int attack_group);
bool is_vulnerable(const char_data& character, int attack_group);

bool is_guardian(const char_data& character);

void report_status_to_group(const group_data& group);
void perform_group_roll(const group_data& group);
void split_gold(const group_data& group, int gold);
void split_exp(const group_data& group, int exp);

int get_energy_regen(const char_data& character);

}

// class that wraps a character pointer and answers wild fighting questions about them
class wild_fighting_handler
{
public:

	wild_fighting_handler(char_data* in_character);

	void on_unit_killed(const char_data* victim);
	void update_health(int new_value);
	void update_tactics(int new_value);
	
	int do_rush(int starting_damage) const;
	
	float get_wild_swing_damage_multiplier() const;
	bool has_bonus_attack_speed() const;
	float get_attack_speed_multiplier() const;

	void on_enter_rage();

private:

	float get_rush_chance() const;

	char_data* character = nullptr;
	game_types::player_specs spec;
	int tactics = -1;
	int max_health = 0;
	int current_health = 0;
	float health_percentage = 0.0f;
};

// class that wraps a character pointer and answers weaponmaster spec questions about them
class weapon_master_handler
{
public:

	weapon_master_handler(char_data* in_character);
	weapon_master_handler(char_data* in_character, const obj_data* in_weapon);

	float get_attack_speed_multiplier() const;
	int get_total_damage(int starting_damage) const;
	int do_on_damage_rolled(int damage_roll, char_data* victim);
	bool ignores_shields(char_data* victim);
	bool ignores_armor(char_data* victim);
	bool does_spear_proc(char_data* victim);
	void do_double_strike(char_data* victim);
	void do_on_damage_dealt(int damage, char_data* victim);
	int get_bonus_OB(const obj_data* in_weapon) const;
	int get_bonus_PB(const obj_data* in_weapon) const;
	int append_score_message(char* message_buffer) const;


private:

	bool does_double_strike() const;

	char_data* character = nullptr;
	const obj_data* weapon = nullptr;
	game_types::player_specs spec;
	game_types::weapon_type weapon_type;

};
