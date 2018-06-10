/* ************************************************************************
*  File: magic.cpp                                                        *
*  Usage: low-level functions for magic; spell template code              *
*  All spells that aren't completely custom like teleport should          *
*  be sent this.                                                          *
************************************************************************ */

#include "char_utils.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "interpre.h"
#include "limits.h"
#include "platdef.h"
#include "spells.h"
#include "structs.h"
#include "utils.h"
#include "zone.h"
#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* external variables */
extern struct skill_data skills[];

#define RACE_SOME_ORC(caster) ((GET_RACE(caster) == RACE_URUK || GET_RACE(caster) == RACE_ORC || GET_RACE(caster) == RACE_MAGUS))

int get_mage_caster_level(const char_data* caster)
{
    int mage_level = utils::get_prof_level(PROF_MAGE, *caster);

    int intel_factor = caster->tmpabilities.intel / 5;
    if (number(0, intel_factor % 5) > 0) {
        ++intel_factor;
    }

    return mage_level + intel_factor;
}

int get_mystic_caster_level(const char_data* caster)
{
    int mystic_level = utils::get_prof_level(PROF_CLERIC, *caster);

    // Factor in will values not divisible by 5.
    int will_factor = caster->tmpabilities.wil / 5;
    if (number(0, will_factor % 5) > 0) {
        ++will_factor;
    }

    return mystic_level + will_factor;
}

int get_magic_power(const char_data* caster)
{
    int caster_level = get_mage_caster_level(caster);
    int level_modifier = GET_MAX_RACE_PROF_LEVEL(PROF_MAGE, caster) * GET_LEVELA(caster) / 30;

    // Factor in intel values not divisible by 5.
    int intel_factor = caster->tmpabilities.intel / 5;
    if (number(0, intel_factor % 5) > 0) {
        ++intel_factor;
    }

    return caster_level + level_modifier + intel_factor;
}

bool should_apply_spell_penetration(const char_data* caster)
{
    bool apply_spell_pen = utils::is_pc(*caster);
    if (apply_spell_pen == false) {
        if (utils::is_mob_flagged(*caster, MOB_ORC_FRIEND) && utils::is_affected_by(*caster, AFF_CHARM)) {
            apply_spell_pen = caster->master && utils::is_pc(*(caster->master));
        }
    }
    return apply_spell_pen;
}

double get_spell_pen_value(const char_data* caster)
{
    int mage_level = utils::get_prof_level(PROF_MAGE, *caster);
    if (utils::is_npc(*caster) && utils::is_affected_by(*caster, AFF_CHARM) && caster->master) {
        mage_level += utils::get_prof_level(PROF_MAGE, *(caster->master)) / 3;
    }

    return mage_level / 5.0;
}

double get_victim_saving_throw(const char_data* caster, const char_data* victim)
{
    double saving_throw = victim->specials2.saving_throw; // this value comes from gear and/or spells.

    if (should_apply_spell_penetration(caster)) {
        double spell_pen = get_spell_pen_value(caster);
        saving_throw = saving_throw - spell_pen;
        if (utils::is_pc(*victim)) {
            // Players that are a higher level than their attacker's mage level
            // get some innate damage reduction.  Players at a lower level take
            // extra damage.
            saving_throw += utils::get_level_a(*victim) / 5.0;
        }
    }

    return saving_throw;
}

int apply_spell_damage(char_data* caster, char_data* victim, int damage_dealt, int spell_number, int hit_location)
{
    double saving_throw = get_victim_saving_throw(caster, victim);

    double damage_multiplier = 1.0;
    if (saving_throw > 0) {
        damage_multiplier = 20.0 / (20.0 + saving_throw);
    } else if (saving_throw < 0) {
        damage_multiplier = 2.0 - (20.0 / (20.0 - saving_throw));
    }

    damage_dealt = int(damage_dealt * damage_multiplier);

    return damage(caster, victim, damage_dealt, spell_number, hit_location);
}

void affect_update(void)
{
    struct affected_type *af, *next;
    struct char_data* i;

    for (i = character_list; i; i = i->next) {
        for (af = i->affected; af; af = next) {
            next = af->next;
            if (af->duration >= 1) {
                af->duration--;
            } else if (af->duration == -1) /* No action taken */
            {
                af->duration = -1; /* Used for items which provide unlimited duration */
            } else {
                if ((af->type > 0) && (af->next->type != af->type) || (af->next->duration > 0)) {
                    if (skills[af->type].wear_off_msgs) {
                        send_to_char(i, "%s\r\n", skills[af->type].wear_off_msg);
                        affect_remove(i, af);
                    }
                }
            }
        }
    }
}

/*
 * Every spell that does damage comes through here. This calculates the
 * amount of damage, adds in any modifiers, determines what the saves are,
 * tests for saves and calls apply_damage().
 * 
 * -1 = dead, otherwise the amount of damge done.
 */
int mag_damage(int level, struct char_data* caster, struct char_data* victim, int spellnum, int savetype)
{
    int damage = 0;
    int save_bonus = 0;

    if (victim == null || caster == null) {
        return (0);
    }
    int mag_power = get_magic_power(caster);
    bool is_cold_spec = utils::get_specialization(*caster) == game_types::PS_Cold;
    bool is_fire_spec = utils::get_specialization(*caster) == game_types::PS_Fire;
    bool is_lightning_spec = utils::get_specialization(*caster) == game_types::PS_Lightning;
    bool is_darkness_spec = utils::get_specialization(*caster) == game_types::PS_Darkness;

    switch (spellnum) {
    case SPELL_MAGIC_MISSILE:
        damage = number(1, mag_power / 6) + 12;
        break;

    case SPELL_CHILL_RAY: // This needs to be finished
        damage = number(1, mag_power) / 2 + 20;

        save_bonus = get_save_bonus(*caster, *victim, game_types::PS_Cold, game_types::PS_Fire);
        break;

    case SPELL_LIGHTNING_BOLT:
        damage = number(1, mag_power) / 2 + 25;
        if (OUTSIDE(caster) || is_lightning_spec)
            damage += number(0, mag_power / 4) + 4;
        else
            send_to_char("Your lightning is weaker inside, as you can not call on nature's full force here.\r\n", caster);

        if (is_lightning_spec)
            damage += damage / 10;

        save_bonus = get_save_bonus(*caster, *victim, game_types::PS_Lightning, game_types::PS_Darkness);
        break;

    case SPELL_DARK_BOLT:
        damage = number(0, mag_power) / 2 + 25;
        if (!SUN_PENALTY(caster))
            damage += number(0, mag_power / 4) + 4;
        else
            send_to_char("Your spell is weakened by the intensity of light.\r\n", caster);

        if (is_darkness_spec)
            damage += damage / 10;

        save_bonus = get_save_bonus(*caster, *victim, game_types::PS_Darkness, game_types::PS_Lightning);
        break;

    case SPELL_FIREBOLT:
        damage = number(1, mag_power) / 4 + number(1, mag_power) / 4 + number(1, mag_power) / 8 + number(1, mag_power) / 8 + number(1, mag_power) / 16 + number(1, mag_power) / 16 + number(1, 65);

        if (is_fire_spec)
            damage = std::max(damage, get_mage_caster_level(caster));

        save_bonus = get_save_bonus(*caster, *victim, game_types::PS_Fire, game_types::PS_Cold);
        break;

    case SPELL_CONE_OF_COLD: // This needs to be finished.
        damage = number(1, mag_power) / 2 + mag_power / 4 + 25;

        save_bonus = get_save_bonus(*caster, *victim, game_types::PS_Fire, game_types::PS_Cold);
        break;

    case SPELL_EARTHQUAKE:
        dam_value = number(1, 30) + get_mage_caster_level(caster);

        save_bonus = victim->tmpabilities.dex / 4;
        break;

    case SPELL_LIGHTNING_STRIKE:
        if (!OUTSIDE(caster)) {
            send_to_char("You can not call lightning inside!\r\n", caster);
            return;
        }

        damage = number(0, mag_power) / 2 * 2 + number(0, mag_power) / 2 + 30;
        if (weather_info.sky[world[caster->in_room].sector_type] != SKY_LIGHTNING) {
            if (is_lightning_spec) {
                send_to_char("You manage to create some lightning, but the effect is reduced.\r\n", caster);
                damage = damage * 4 / 5;
            } else {
                send_to_char("The weather is not appropriate for this spell.\r\n", caster);
                return;
            }
        }

        save_bonus = get_save_bonus(*caster, *victim, game_types::PS_Lightning, game_types::PS_Darkness);
        break;

    case SPELL_SEARING_DARKNESS:
        int fire_damage = number(0, mag_power) / 2 + 15;
        if (is_fire_spec) {
            fire_damage += fire_damage / 2;
        }

        save_bonus = get_save_bonus(*caster, *victim, game_types::PS_Fire, game_types::PS_Cold);
        bool saves_fire = new_saves_spell(caster, victim, save_bonus);
        if (saves_fire)
            fire_damage = fire_damage * 1 / 3;

        int darkness_damage = number(0, mag_power) / 2 + 15;
        if (!SUN_PENALTY(caster))
            darkness_damage += number(0, mag_power / 4) + 5;
        else
            send_to_char("Your spell is weakened by the intensity of light.\r\n", ch);

        if (is_darkness_spec)
            darkness_damage += darkness_damage / 10;

        damage = fire_damage + darkness_damage;
        break;

    case SPELL_FIREBALL: // This needs to be finished.
        damage = number(1, mag_power) / 2 + number(1, mag_power) / 2 + number(1, mag_power) / 2 + 30;
        if (RACE_SOME_ORC(caster))
            damage -= 5;

        save_bonus = get_save_bonus(*caster, *victim, game_types::PS_Fire, game_types::PS_Cold);
        break;

    case SPELL_WORD_OF_PAIN:
        damage = number(1, mag_power / 6) + 12;
        break;

    case SPELL_LEACH:
        damage = number(1, mag_power / 4) + 18;
        bool leach_save = new_saves_spells(caster, victim, save_bonus);
        if (!leach_save) {
            int moves = std::min((int)GET_MOVE(victim), number(0, 5));
            GET_MOVE(victim) += -moves;
            GET_MOVE(caster) = std::min((int)GET_MAX_MOVE(caster), GET_MOVE(caster) + moves);
            GET_HIT(caster) = std::min(GET_MAX_HIT(caster), GET_HIT(caster) + damage / 2);
            send_to_char("Your life's ichor is drained!\r\n", victim);
        }
        break;

    case SPELL_BLACK_ARROW:
        damage = number(1, mag_power) / 2 + number(1, mag_power) / 2 + 13;

        if (!SUN_PENALTY(caster) || is_darkness_spec)
            damage += number(0, mag_power / 6) + 2;
        else
            send_to_char("Your spell is weakened by the intensity of light.\r\n", ch);

        save_bonus = get_save_bonus(*caster, *victim, game_types::PS_Darkness, game_types::PS_Lightning);
        break;

    case SPELL_WORD_OF_AGONY:
        damage = number(1, mag_power) / 2 + number(1, mag_power) / 2 + 20;
        bool agony_save = new_saves_spell(caster, victim, -2);
        if (!agony_save)
            apply_chilled_effect(caster, victim);

        break;

    case SPELL_SHOUT_OF_PAIN:
        damage = number(1, 50) + mag_power / 2;
        break;

    default:
        break;
    }
    bool saved = new_saves_spells(caster, victim, save_bonus);
    if (saved) {
        damage >>= 1;
    }
    return (apply_spell_damage(caster, victim, dam, spellnum, 0));
}

/* Currently the game only supports one affect from the same spell but if this is ever fixed increase this number */
#define MAX_SPELL_AFFECTS 1

/*
 * Every spell that does an affect comes through here.  This determines
 * the effect, whether it is added or replacement, whether it is legal or
 * not, etc.
 *
 * affect_join(vict, aff, add_dur, avg_dur, add_mod, avg_mod)
 */
void mag_affects(int level, struct char_data* caster, struct char_data* victim,
    int spellnum, int savetype)
{
    struct affected_type af[MAX_SPELL_AFFECTS];
    bool accum_affect = FALSE, accum_duration = FALSE, affect_room = FALSE;
    const char *to_vict = NULL, *to_room = NULL, *to_caster = NULL;
    int i;

    if (victim == NULL || caster == NULL)
        return;

    int mystic_level = get_mystic_caster_level(caster);
    int mage_level = get_mage_caster_level(caster);

    for (i = 0; i < MAX_SPELL_AFFECTS; i++) {
        af[i].type = spellnum;
        af[i].bitvector = 0;
        af[i].modifier = 0;
        af[i].location = APPLY_NONE;
    }

    switch (spellnum) {
    case SPELL_POISON:
        if (!victim && !(caster->specials.fightning)) {
            affect_room = TRUE;
            af[0].type = ROOMAFF_SPELL;
            af[0].duration = (mystic_level) / 3;
            af[0].modifier = mystic_level / 2;
            af[0].bitvector = 0;
            to_caster = "You breathe out poison.";
            to_room = "$n breathes out a cloud of smoke.";
        }

        if (GET_POSITION(caster) == POSITION_FIGHTING)
            victim = caster->specials.fighting;

        if (victim) {
            if (!saves_poison(victim, caster) && (number(0, magus_save) < 50)) {
                af[0].duration = mystic_level + 1;
                af[0].modifier = -2;
                af[0].location = APPLY_STR;
                af.bitvector = AFF_POISON;
                to_victim = "You feel very sick.";
                damage((caster) ? caster : victim, victim, 5, SPELL_POISON, 0);
            } else {
                to_victim = "You feel your body fend off the poison.";
                act("$N shrugs off your poison with ease.", FALSE, caster, 0, victim, TO_CHAR);
            }
        }
        break;
    case SPELL_DETECT_MAGIC:
        if (!victim)
            victim = caster;

        if (affect_by_spell(victim, SPELL_DETECT_MAGIC)) {
            if (victim == caster)
                to_caster = "You already can sense magic";
            else
                act("$E already can sense magic.\n\r", TRUE, caster, 0, victim, TO_CHAR);
            return;
        }

        af[0].type = SPELL_DETECT_MAGIC;
        af[0].duration = mystic_level * 5;
        af[0].modifier = 0;
        af[0].location = APPLY_NONE;
        af[0].bitvector = AFF_DETECT_MAGIC;
        to_victim = "Your eyes tingle.";
        break;

    case SPELL_EVASION:
    }

    /*
	 * If this is a mob that has this affect set in its mob file, do not
	 * perform the affect. This prevents people from un-sancting mobs
	 * by sancting them and waiting for it to fade, for example.
	 */
    if (IS_NPC(victim) && !affected_by_spell(victim, spellnum)) {
        for (i = 0; i < MAX_SPELL_AFFECTS; i++) {
            if (AFF_FLAGGED(victim, af[i].bitvector)) {
                send_to_char(ch, "%s", NOEFFECT);
                return;
            }
        }
    }

    /*
	 * If the victim is already affected by this spell, and the spell does
	 * not have an accumlative affect, then fail the spell.
	 */
    if (affected_by_spell(victim, spellnum) && !(accum_duration || accum_affect)) {
        send_to_char(ch, "%s", NOEFFECT);
        return;
    }

    /*
	 * Loop through the affects and apply it to the victim
	 */
    for (i = 0; i < MAX_SPELL_AFFECTS; i++) {
        if (af[i].bitvector || (af[i].location != APPLY_NONE)) {
            affect_join(victim, af + i, accum_duration, FALSE, accum_affect, FALSE);
        }
    }

    if (to_vict != NULL)
        act(to_vict, FALSE, victim, 0, caster, TO_CHAR);

    if (to_caster != NULL)
        act(to_caster, TRUE, victim, )

            if (to_room != NULL)
                act(to_room, TRUE, victim, 0, ch, TO_ROOM);
}

/*
 * This function is used to provide services to mag_groups. This function is
 * the one you should change to add new group spells.
 * 
 * No spells of this class are currently implemented.
 */
void perform_mag_groups(int level, struct char_data* caster, struct char_data* tch,
    int spellnum, int savetype)
{
    switch (spellnum) {
    case SPELL_MASS_REGENERATION:
        mag_affects(level, caster, tch, SPELL_REGENERATION, savetype);
        break;
    }
}

/*
 * Every spell that affects the group should run through here
 * perform_mag_groups contains the switch statement to send us to the right
 * magic.
 *
 * group spells affect everyone grouped with the caster who is in the room,
 * caster last.
 *
 * To add new group spells, you shouldn't have to change anything in
 * mag_groups -- just add a new case to perform_mag_groups.
 */
void mag_groups(int level, struct char_data* caster, int spellnum, int savetype)
{
    struct char_data *tch, *k;
    struct follow_type *f, *f_next;

    if (caster == NULL) {
        return;
    }

    if (caster->group == NULL) {
        return;
    }

    if (caster->master != NULL) {
        k = caster->master;
    } else {
        k = caster;
    }

    for (char_iter iter = caster->group->begin(); iter != caster->group->end(); ++iter) {
        if (IN_ROOM(iter) != IN_ROOM(caster))
            continue;
        if (iter == caster)
            continue;

        perform_mag_groups(level, caster, *iter, spellnum, savetype);
    }

    perform_mag_groups(level, caster, caster, spellnum, savetype); // Applies spell to caster last.
}

/*
 * Mass spells affect every creature in the room except the caster. These are
 * generally offensive spells. This calls mag_damage to do the actual damage
 * -- all spells listed here must also have a cast in mag_damage() in order
 * for them to work.
 * 
 */
void mag_masses(int level, struct char_data* caster, int spellnum, int savetype)
{
    struct char_data *tch, *tch_next;

    if (!caster || caster->in_room == NOWHERE)
        return;

    for (tch = world[caster->in_room].people; tch; tch = tch_next) {
        tch_next = tch->next_in_room;
        if (tch == caster)
            continue;

        switch (spellnum) {
        case SPELL_SHOUT_OF_PAIN:
            mag_damage(level, caster, tch, spellnum, savetype);
            break;
        case SPELL_EARTHQUAKE:
            mag_damage(level, caster, tch, spellnum, savetype);
            break;
        }
    }

    /* Add special room spells here for example earthquake has the possibility of adding an
   * additional room below the caster and dealing damage again on falling.
   */
    switch (spellnum) {
    case SPELL_EARTHQUAKE:
        int crack_chance, tmp;
        room_data* cur_room;
        int crack;

        if ((cur_room->sector_type == SECT_CITY) || (cur_room->sector_type == SECT_CRACK) || (cur_room->sector_type == SECT_WATER_SWIM) || (cur_room->sector_type == SECT_WATER_NOSWIM) || (cur_room->sector_type == SECT_UNDERWATER)) {
            crack_chance = 0;
        }

        if (IS_SET(cur_room->room_flags, INDOORS)) {
            crack_chance = 0;
        }

        if (cur_room->dir_option[DOWN] && IS_SET(cur_room->dir_option[DOWN]->exit_info, EX_CLOSED | EX_DOORISHEAVY)) {
            crack_chance = 0;
        }

        if (cur_room->dir_option[DOWN] && !cur_room->dir_option[DOWN]->exit_info) {
            crack_chance = 1;
        } else {
            if (number(0, 100) > level) {
                crack_chance = 0;
            }
        }

        if (crack_chance) {
            if (cur_room->dir_option[DOWN] && (cur_room->dir_option[DOWN]->to_room != NOWHERE)) {
                crack = cur_room->dir_option[DOWN]->to_room;
                if (crack != NOWHERE) {
                    if (cur_room->dir_option[DOWN]->exit_info) {
                        act("The way down crashes open!", FALSE, caster, 0, 0, TO_ROOM);
                        send_to_char("The way down crashes open!\r\n", caster);
                        cur_room->dir_option[DOWN]->exit_info = 0;
                        if (world[crack].dir_option[UP] && (world[crack].dir_option[UP]->to_room == caster->in_room) && world[crack].dir_option[UP]->exit_info) {
                            tmp = caster->in_room;
                            caster->in_room = crack;
                            act("The way up crashes open!", FALSE, caster, 0, 0, TO_ROOM);
                            world[crack].dir_option[UP]->exit_info = 0;
                            caster->in_room = tmp;
                        }
                    }
                }
            }
            /* no room there, so create one */
            else {
                crack = world.create_room(world[caster->in_room].zone);
                world[caster->in_room].create_exit(DOWN, crack);

                RELEASE(world[crack].name);
                world[crack].name = str_dup("Deep Crevice");
                RELEASE(world[crack].description);
                world[crack].description = str_dup(
                    "   The crevice is deep, dark and looks unsafe. The walls of fresh broken\r\n"
                    "rock are uneven and still crumbling. Some powerful disaster must have \r\n"
                    "torn the ground here recently.\r\n");
                world[crack].sector_type = SECT_CRACK;
                world[crack].room_flags = cur_room->room_flags;
                act("The ground is cracked under your feet!", FALSE, caster, 0, 0, TO_ROOM);
                send_to_char("Your spell cracks the ground open!\r\n", caster);
            }

            /* Deal additional damage to everybody in the room that doesn't fall in the crack */
            for (tch = cur_room->people; tch; tch = tch_next) {
                tch_next = tch->next_in_room;
                if (tch == caster)
                    continue;

                mag_damage(level, caster, tch, SPELL_EARTHQUAKE2, savetype);
            }
            break;
        }
    }

    /*
 * Every spell that affects an area (room) runs through here.  These are
 * generally offensive spells.  This calls mag_damage to do the actual
 * damage -- all spells listed here must also have a case in mag_damage()
 * in order for them to work.
 *
 *  area spells have limited targets within the room.
 *  Currently there are no spells that implement this function
 */
    void mag_areas(int level, struct char_data* caster, int spellnum, int savetype)
    {
        struct char_data *tch, *next_tch;
        const char *to_char = NULL, *to_room = NULL;

        if (caster == NULL)
            return;

        /*
   * to add spells to this fn, just add the message here plus an entry
   * in mag_damage for the damaging part of the spell
   */
        switch (spellnum) {
        default:
            break;
        }
    }

    void mag_points(int level, struct char_data* caster, struct char_data* victim, int spellnum, int savetype)
    {
        int healing = 0, move = 0;
        int caster_level = get_mage_caster_level(caster);
        bool is_regen_spec = utils::get_specialization(*caster) == games_types::PS_Regeneration;

        if (!caster) {
            return;
        }

        if (!victim) {
            victim = caster;
        }

        switch (spellnum) {
        case SPELL_CURE_SELF:
            affected_type* af;
            af = affected_by_spell(victim, SKILL_MARK);

            if (af) {
                af->duration = std::max(af->duration - 30, 0);
                send_to_char("You close the wounds in your chest.\r\n", victim);
            }

            if (victim->tmpabilities.hit >= victim->abilities.hit) {
                send_to_char("You are already healthy.\r\n", victim);
                return;
            }
            healing = caster_level / 2 + 10;
            if (is_regen_spec) {
                healing += 5;
            }
            send_to_char("You feel better.\r\n", victim);
            break;

        case SPELL_VITALIZE_SELF:
            if (victim->tmpabilities.move >= victim->abilities.move) {
                send_to_char("You are already rested.\r\n", victim);
                return;
            }

            move = 2 * caster_level;
            if (is_regen_spec) {
                move += 10;
            }
            send_to_char("You feel refreshed.\r\n", victim);
            break;
        }

        GET_HIT(victim) = std::min(GET_MAX_HIT(victim), GET_HIT(victim) + healing);
        GET_MOVE(victim) = std::min(GET_MAX_MOVE(victim), GET_MOVE(victim) + move);
        update_pos(victim);
    }