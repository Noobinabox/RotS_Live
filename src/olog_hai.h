#pragma once
#include "structs.h"

namespace olog_hai {
    bool is_skill_valid(char_data* ch, const int& skill_id);
    char_data* is_target_valid(char_data* attacker, waiting_type* target);
    void on_success_smash_hit(const char_data& attacker, char_data* victim);
    void on_failed_smash_hit(const char_data& attacker, char_data* victim);
}