#include "warrior_spec_handlers.h"
#include "char_utils.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "spells.h"

namespace player_spec
{
    //============================================================================
    // Wild fighting implementation
    //============================================================================
    wild_fighting_handler::wild_fighting_handler(char_data *in_character)
    {
        character = in_character;
        spec = utils::get_specialization(*character);
        tactics = character->specials.tactics;
        max_health = character->abilities.hit;
        current_health = character->tmpabilities.hit;
        health_percentage = current_health / (float)max_health;
    }

    void wild_fighting_handler::on_unit_killed(const char_data *victim)
    {
        if (spec != game_types::PS_WildFighting)
            return;

        if (tactics != TACTICS_BERSERK)
            return;

        if (victim->get_level() >= character->get_capped_level() * 6 / 10)
        {
            int missing_health = max_health - current_health;
            character->tmpabilities.hit += missing_health * 0.1f;

            // let people know that shit's getting real
            act("$n roars and seems invigorated after the kill!", FALSE, character, nullptr, 0, TO_ROOM);
            act("You roar and feel a rush of vigor as your bloodlust is satisfied!", FALSE, character, nullptr, 0, TO_CHAR);

            update_health(character->tmpabilities.hit);
        }
    }

    void wild_fighting_handler::update_health(int new_value)
    {
        if (spec != game_types::PS_WildFighting)
        {
            return;
        }

        float old_health_percentage = health_percentage;

        current_health = new_value;
        health_percentage = current_health / (float)max_health;

        // broadcast rage message
        if (old_health_percentage > 0.45f && health_percentage <= 0.45f)
        {
            on_enter_rage();
        }
    }

    void wild_fighting_handler::update_tactics(int new_value)
    {
        if (spec != game_types::PS_WildFighting)
            return;

        if (tactics == new_value)
            return;

        tactics = new_value;
        if (health_percentage <= 0.45f)
        {
            on_enter_rage();
        }
    }

    float wild_fighting_handler::get_rush_chance() const
    {
        if (spec != game_types::PS_WildFighting)
            return 0.0f;

        if (tactics == TACTICS_BERSERK)
            return 0.15f;

        if (tactics == TACTICS_AGGRESSIVE)
            return 0.10f;

        if (tactics == TACTICS_NORMAL)
            return 0.05f;

        return 0.0f;
    }

    int wild_fighting_handler::do_rush(int starting_damage) const
    {
        if (spec != game_types::PS_WildFighting)
            return starting_damage;

        float rush_chance = get_rush_chance();
        if (number() > rush_chance)
            return starting_damage;

        send_to_char("You rush forward wildly.\n\r", character);
        act("$n rushes forward wildly.", TRUE, character, 0, 0, TO_ROOM);

        int rush_damage = starting_damage / 2;

        wild_fighting_data *data = static_cast<wild_fighting_data *>(character->extra_specialization_data.current_spec_info);
        data->add_rush_damage(rush_damage);

        return starting_damage + rush_damage;
    }

    float wild_fighting_handler::get_wild_swing_damage_multiplier() const
    {
        if (tactics != TACTICS_BERSERK)
            return 1.0f;

        if (health_percentage <= 0.25f)
            return 1.33f;

        return 1.0f;
    }

    bool wild_fighting_handler::has_bonus_attack_speed() const
    {
        return spec == game_types::PS_WildFighting && tactics == TACTICS_BERSERK && health_percentage <= 0.45f;
    }

    float wild_fighting_handler::get_attack_speed_multiplier() const
    {
        if (spec != game_types::PS_WildFighting)
            return 1.0f;

        if (tactics != TACTICS_BERSERK)
            return 1.0f;

        if (health_percentage > 0.45f)
            return 1.0f;

        return 1.0f + 1.0f - health_percentage - 0.4f; // 15% bonus at 45% health, scaling to 59% at 1% health.
    }

    void wild_fighting_handler::on_enter_rage()
    {
        // let people know that shit's getting real
        act("$n roars and enters a battle frenzy!", FALSE, character, nullptr, 0, TO_ROOM);
        act("You feel your pulse quicken as you enter a battle frenzy!", FALSE, character, nullptr, 0, TO_CHAR);
    }
} // namespace player_spec
