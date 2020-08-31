#include "warrior_spec_handlers.h"
#include "char_utils.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "spells.h"

namespace player_spec
{
    //============================================================================
    // Battle Mage Implementation
    //============================================================================
    battle_mage_handler::battle_mage_handler(const char_data *in_character)
    {
        is_battle_spec = utils::get_specialization(*in_character) == game_types::PS_BattleMage;
        tactics = in_character->specials.tactics;
        mage_level = utils::get_prof_level(PROF_MAGE, *in_character);
        warrior_level = utils::get_prof_level(PROF_WARRIOR, *in_character);
    }

    int battle_mage_handler::get_bonus_spell_pen(int spell_pen) const
    {
        if (!is_battle_spec)
        {
            return spell_pen;
        }

        return spell_pen + (tactics / 2) + (mage_level / 12);
    }

    int battle_mage_handler::get_bonus_spell_power(int spell_power) const
    {
        if (!is_battle_spec)
        {
            return spell_power;
        }

        return spell_power + (tactics / 2) + (mage_level / 12);
    }

    bool battle_mage_handler::can_prepare_spell() const
    {
        return !is_battle_spec;
    }

    bool battle_mage_handler::does_spell_get_interrupted() const
    {
        if (!is_battle_spec)
        {
            return true;
        }

        if (tactics < TACTICS_AGGRESSIVE)
        {
            return number() > base_chance;
        }

        float warrior_bonus = warrior_level / 100.0f;
        float mage_bonus = mage_level / 100.0f;
        float tactic_bonus = (tactics * 2) / 100.0f;
        float total_bonus = base_chance + warrior_bonus + mage_bonus + tactic_bonus;
        return number() > total_bonus;
    }

    bool battle_mage_handler::does_armor_fail_spell() const
    {
        if (!is_battle_spec)
        {
            return true;
        }

        if (tactics < TACTICS_AGGRESSIVE)
        {
            return number() > base_chance;
        }

        float tactic_bonus = (tactics * 2) / 100.0f;
        float warrior_bonus = warrior_level / 100.0f;
        float total_bonus = base_chance + tactic_bonus + warrior_bonus;
        return number() > total_bonus;
    }

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

    //============================================================================
    // Weapon-master implementation
    //============================================================================

    namespace
    {
        constexpr float smiting_proc_chance(int damage)
        {
            return damage * 0.005f;
        }
        constexpr const float bludgeon_proc_chance = 0.25f;
        constexpr const float cleave_proc_chance = 0.50f;

        constexpr const float flail_proc_chance = 0.40f;
        constexpr const float piercing_proc_chance = 0.25f;
        constexpr const float slashing_proc_chance = 0.40f;
        constexpr const float stabbing_proc_chance = 0.50f;
        constexpr const float whipping_proc_chance = 0.40f;

    } // namespace

    weapon_master_handler::weapon_master_handler(char_data *in_character)
        : weapon_master_handler(in_character, in_character->equipment[WIELD])
    {
    }

    weapon_master_handler::weapon_master_handler(char_data *in_character, const obj_data *in_weapon) : character(in_character), weapon(in_weapon), spec(utils::get_specialization(*in_character))
    {
        if (weapon != nullptr && weapon->obj_flags.type_flag == ITEM_WEAPON)
        {
            weapon_type = weapon->get_weapon_type();
        }
    }

    float weapon_master_handler::get_attack_speed_multiplier() const
    {
        if (spec != game_types::PS_WeaponMaster)
            return 1.0f;

        if (weapon_type == game_types::WT_PIERCING || weapon_type == game_types::WT_WHIPPING)
            return 1.15f;

        return 1.0f;
    }

    int weapon_master_handler::get_total_damage(int starting_damage) const
    {
        if (spec != game_types::PS_WeaponMaster)
            return starting_damage;

        if (weapon_type == game_types::WT_CLEAVING || weapon_type == game_types::WT_CLEAVING_TWO || weapon_type == game_types::WT_FLAILING)
        {
            // 15% bonus damage with these weapon types
            int bonus_damage = starting_damage * 15 / 100;
            return starting_damage + bonus_damage;
        }

        return starting_damage;
    }

    int weapon_master_handler::do_on_damage_rolled(int damage_roll, char_data *victim)
    {
        if (spec != game_types::PS_WeaponMaster)
            return damage_roll;

        if (weapon_type == game_types::WT_CLEAVING || weapon_type == game_types::WT_CLEAVING_TWO)
        {
            if (number() < cleave_proc_chance)
            {
                int new_roll = number(1, 100);
                if (new_roll > damage_roll)
                {

                    act("Your axe strikes hard and true!", FALSE, character, NULL, victim, TO_CHAR);
                    return new_roll;
                }
            }
        }

        return damage_roll;
    }

    bool weapon_master_handler::ignores_shields(char_data *victim)
    {
        if (spec != game_types::PS_WeaponMaster)
            return false;

        if (victim == nullptr || victim->equipment[WEAR_SHIELD] == nullptr)
            return false;

        // whips and flails can ignore armor
        if (weapon_type != game_types::WT_FLAILING && weapon_type != game_types::WT_WHIPPING)
            return false;

        if (weapon_type == game_types::WT_FLAILING)
        {
            if (number() <= flail_proc_chance)
            {
                act("Your flail wraps around $N's shield, rendering it immaterial!", FALSE, character, NULL, victim, TO_CHAR);
                act("$n's flail wraps around your shield, rendering it immaterial!.", FALSE, character, NULL, victim, TO_VICT);
                return true;
            }
        }
        else if (weapon_type == game_types::WT_WHIPPING)
        {
            if (number() <= whipping_proc_chance)
            {
                act("Your whip snakes past $N's shield, rendering it immaterial!", FALSE, character, NULL, victim, TO_CHAR);
                act("$n's whip snakes past your shield, rendering it immaterial!", FALSE, character, NULL, victim, TO_VICT);
                return true;
            }
        }

        return false;
    }

    bool weapon_master_handler::ignores_armor(char_data *victim)
    {
        if (spec != game_types::PS_WeaponMaster)
            return false;

        if (weapon_type != game_types::WT_PIERCING)
            return false;

        if (number() > piercing_proc_chance)
            return false;

        // act so people know armor is ignored
        act("You find a weakness in $N's armor and slip your weapon right through!", FALSE, character, NULL, victim, TO_CHAR);
        act("$n finds a weakness in your armor and slips $s weapon right through!", FALSE, character, NULL, victim, TO_VICT);
        return true;
    }

    bool weapon_master_handler::does_spear_proc(char_data *victim)
    {
        if (spec != game_types::PS_WeaponMaster)
            return false;

        if (weapon_type != game_types::WT_STABBING)
            return false;

        if (number() > stabbing_proc_chance)
            return false;

        act("Your spear punches through $N's armor!", FALSE, character, nullptr, victim, TO_CHAR);
        act("$n thrusts $s spear forcefully, punching a hole in your armor!", FALSE, character, nullptr, victim, TO_VICT);
        return true;
    }

    void weapon_master_handler::regain_energy(char_data *victim)
    {
        if (!does_sword_proc())
            return;

        // It's possible that victim has died in response to the strike
        // that caused this.  Only send a message if the character is currently
        // in combat.
        if (character->specials.fighting != nullptr)
        {
            act("You gain a rush of momentum!", FALSE, character, NULL, victim, TO_CHAR);
            act("$n gains a rush of momentum!", FALSE, character, 0, victim, TO_ROOM, FALSE);
        }

        character->specials.ENERGY += ENE_TO_HIT / 2;
    }

    bool weapon_master_handler::does_sword_proc() const
    {
        if (spec != game_types::PS_WeaponMaster)
            return false;

        if (weapon_type != game_types::WT_SLASHING && weapon_type != game_types::WT_SLASHING_TWO)
            return false;

        return number() <= slashing_proc_chance;
    }

    void weapon_master_handler::do_on_damage_dealt(int damage, char_data *victim)
    {
        if (spec != game_types::PS_WeaponMaster)
            return;

        if (weapon_type == game_types::WT_BLUDGEONING || weapon_type == game_types::WT_BLUDGEONING_TWO)
        {
            if (number() <= bludgeon_proc_chance)
            {
                int lost_energy = damage * 10; // victim loses 10x damage taken as energy
                victim->specials.ENERGY -= lost_energy;
                victim->specials.ENERGY = std::max(victim->specials.ENERGY, 0);

                act("Your crushing blow knocks the wind from $N!", FALSE, character, NULL, victim, TO_CHAR);
                act("$n's blow staggers you and knocks the wind from your chest!", FALSE, character, NULL, victim, TO_VICT);
            }
        }
        else if (weapon_type == game_types::WT_SMITING)
        {
            // damage influences chance to inflict haze
            if (number() <= smiting_proc_chance(damage))
            {
                // victim is afflicted with haze

                auto haze = affected_by_spell(victim, SPELL_HAZE);
                if (haze == nullptr)
                {
                    affected_type af;
                    af.type = SPELL_HAZE;
                    af.duration = 1; // the spell's default duration
                    af.modifier = 1;
                    af.location = APPLY_NONE;
                    af.bitvector = AFF_HAZE;

                    affect_to_char(victim, &af);

                    act("You stagger $N with your crushing blow!", FALSE, character, NULL, victim, TO_CHAR);
                    act("$n blow crushes into you, causing your world to spin!", FALSE, character, NULL, victim, TO_VICT);
                    act("$N staggers from $n's crushing blow, overcome by dizziness!", FALSE, character, 0, victim, TO_NOTVICT, FALSE);
                }
                else
                {
                    haze->duration = 1; // refresh duration
                }
            }
        }
    }

    int weapon_master_handler::get_bonus_OB() const
    {
        if (spec != game_types::PS_WeaponMaster)
            return 0;

        if (weapon_type == game_types::WT_BLUDGEONING || weapon_type == game_types::WT_BLUDGEONING_TWO || weapon_type == game_types::WT_SMITING)
        {
            return 10;
        }
        else if (weapon_type == game_types::WT_SLASHING || weapon_type == game_types::WT_SLASHING_TWO)
        {
            return 5;
        }

        return 0;
    }

    int weapon_master_handler::get_bonus_PB() const
    {
        if (spec != game_types::PS_WeaponMaster)
            return 0;

        if (weapon_type == game_types::WT_STABBING)
        {
            return 10;
        }
        else if (weapon_type == game_types::WT_SLASHING || weapon_type == game_types::WT_SLASHING_TWO)
        {
            return 5;
        }

        return 0;
    }

    int weapon_master_handler::append_score_message(char *message_buffer) const
    {
        if (spec != game_types::PS_WeaponMaster)
            return 0;

        switch (weapon_type)
        {
        case game_types::WT_BLUDGEONING:
        case game_types::WT_BLUDGEONING_TWO:
            return sprintf(message_buffer, "Your mastery grants offensive prowess and staggering blows.\r\n");
        case game_types::WT_CLEAVING:
        case game_types::WT_CLEAVING_TWO:
            return sprintf(message_buffer, "Your mastery grants unrivaled power to your blows.\r\n");
        case game_types::WT_FLAILING:
            return sprintf(message_buffer, "Your mastery grants power to your blows and renders shields ineffective.\r\n");
        case game_types::WT_PIERCING:
            return sprintf(message_buffer, "Your mastery grants swiftness to your blows and renders armor useless.\r\n");
        case game_types::WT_SLASHING:
        case game_types::WT_SLASHING_TWO:
            return sprintf(message_buffer, "Your mastery grants balanced prowess and occasional swift strikes.\r\n");
        case game_types::WT_SMITING:
            return sprintf(message_buffer, "Your mastery grants offensive prowess and dazing blows.\r\n");
        case game_types::WT_STABBING:
            return sprintf(message_buffer, "Your mastery grants defensive prowess and armor piercing blows.\r\n");
        case game_types::WT_WHIPPING:
            return sprintf(message_buffer, "Your mastery grants swiftness to your blows and renders armor useless.\r\n");
        default:
            break;
        }

        return 0;
    }

} // namespace player_spec