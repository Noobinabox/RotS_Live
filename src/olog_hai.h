#pragma once

#include "structs.h"
#include "interpre.h"

namespace olog_hai {
    int add_frenzy_damage(char_data* character, int damage);
    bool is_frenzy_active(char_data& character);
    int add_bash_bonus(const char_data& character);
    bool item_restriction(char_data* character, obj_data* item);
    int get_prob_skill(char_data* attacker, char_data* victim, int skill);
    void apply_victim_delay(char_data* victim, int delay);
    bool is_skill_valid(char_data* ch, const int& skill_id);
    char_data* is_target_valid(char_data* attacker, waiting_type* target);
    bool is_target_in_room(char_data* attacker, char_data* victim);
    void do_sanctuary_check(char_data* ch);
    char_data* get_random_target(char_data* ch, char_data* original_victim);
    int get_base_skill_damage(char_data& olog_hai, int prob);
    int calculate_overrun_damage(char_data& attacker, int prob);
    int calculate_smash_damage(char_data& attacker, int prob);
    void generate_smash_dismount_messages(char_data* attacker, char_data* victim);
    void apply_smash_damage(char_data* attacker, char_data* victim, int prob);
    int calculate_cleave_damage(char_data& attacker, int prob);
    int calculate_stomp_damage(char_data& attacker, int prob);
    void apply_stomp_affect(char_data* attacker, char_data* victim);
    void apply_overrun_damage(char_data* attacker, char_data* victim);
    void apply_cleave_damage(char_data* attacker, char_data* victim);
    void generate_frenzy_message(char_data* character);
    void apply_frenzy_affect(char_data* character);
    void room_target(char_data* ch, void (*skill_damage)(char_data* character, char_data* victim));
}

bool is_direction_valid(char_data* ch, int cmd);
int get_direction(std::string direction);

ACMD(do_cleave);
ACMD(do_smash);
ACMD(do_overrun);
ACMD(do_frenzy);
ACMD(do_stomp);
