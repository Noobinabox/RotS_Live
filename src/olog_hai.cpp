#include "db.h"
#include "structs.h"
#include "char_utils.h"
#include "comm.h"
#include <string>
#include "spells.h"
#include "handler.h"
#include "big_brother.h"
#include "skill_timer.h"
#include "utils.h"
#include <iostream>

extern struct room_data world;
extern struct char_data* waiting_list;
void appear(struct char_data* ch);
int check_overkill(struct char_data* ch);
const int FRENZY_TIMER = 600;
const int SMASH_TIMER = 60;
const int STOMP_TIMER = 60;
const int CLEAVE_TIMER = 30;
const int OVERRUN_TIMER = 60;

namespace olog_hai {
    int get_prob_skill(char_data* attacker, char_data* victim, int skill) {
        int prob = utils::get_skill(*attacker, skill);
        prob -= get_real_dodge(victim) / 2;
        prob -= get_real_parry(victim) / 2;
        prob += get_real_OB(attacker) / 2;
        prob += number(1, 100);
        prob -= 120;
        return prob;
    }

    bool is_skill_valid(char_data* ch, const int& skill_id) {
        if (utils::get_race(*ch) != RACE_OLOGHAI) {
            send_to_char("Unrecognized command.\r\n", ch);
            return false;
        }

        if (utils::is_shadow(*ch)) {
            send_to_char("Hmm, perhaps you've spent to much time in the mortal lands.\r\n", ch);
            return false;
        }

        const room_data& room = world[ch->in_room];
        if (utils::is_set(room.room_flags, (long)PEACEROOM)) {
            send_to_char("A peaceful feeling overwhelms you, and you cannot bring yourself to attack.\r\n", ch);
            return false;
        }

        if (utils::get_skill(*ch, skill_id) == 0) {
            std::string message;
            switch(skill_id) {
                case SKILL_SMASH:
                    message = "Learn how to smash first.\r\n";
                    break;
                case SKILL_FRENZY:
                    message = "Learn how to frenzy first.\r\n";
                    break;
                case SKILL_STOMP:
                    message = "Learn how to stomp first.\r\n";
                    break;
                case SKILL_CLEAVE:
                    message = "Learn how to cleave first.\r\n";
                    break;
                case SKILL_OVERRUN:
                    message = "Learn how to overrun first.\r\n";
                    break;
            }

            send_to_char(message.c_str(), ch);
            return false;
        }

        obj_data* weapon = ch->equipment[WIELD];
        if (!weapon) {
            send_to_char("Wield a weapon first!\r\n", ch);
            return false;
        }
        return true;
    }
    
    char_data* is_target_valid(char_data* attacker, waiting_type* target) {
        char_data* victim = nullptr;

        if (target->targ1.type == TARGET_TEXT) {
            victim = get_char_room_vis(attacker, target->targ1.ptr.text->text);
        } else if (target->targ1.type == TARGET_CHAR) {
            if (char_exists(target->targ1.ch_num)) {
                victim = target->targ1.ptr.ch;
            }
        }

        return victim;
    }

    bool is_target_in_room(char_data* attacker, char_data* victim) {
        return attacker->in_room == victim->in_room;
    }

    char_data* is_smash_target_valid(char_data* attacker, waiting_type* target) {
        char_data* victim = is_target_valid(attacker, target);

        if (victim == nullptr) {
            if (attacker->specials.fighting) {
                victim = attacker->specials.fighting;
            } else {
                send_to_char("Smash who?\r\n", attacker);
                return nullptr;
            }
        }

        if (!is_target_in_room(attacker, victim)) {
            send_to_char("Your victim is no longer here.\r\n", attacker);
            return nullptr;
        }

        if (!CAN_SEE(attacker, victim)) {
            send_to_char("Smash who?\r\n", attacker);
            return nullptr;
        }

        if (attacker == victim) {
            send_to_char("Funny aren't we?\r\n", attacker);
            return nullptr;
        }

        return victim;
    }

    void do_sanctuary_check(char_data* ch) {
        if (utils::is_affected_by(*ch, AFF_SANCTUARY)) {
            appear(ch);
            send_to_char("You cast off your santuary!\r\n", ch);
            act("$n renouces $s sanctuary!", FALSE, ch, 0, 0, TO_ROOM);
        }
    }

    bool does_skill_hit_random(int skill_id) {
        switch(skill_id) {
            case SKILL_SMASH:
                return number() >= 0.95;
            default:
                return false;
        }
    }

    char_data* get_random_target(char_data* ch, char_data* original_victim) {
        char_data* t;
        int num = 0;
        for (t = world[ch->in_room].people; t != nullptr; t = t->next_in_room) {
            if (t != ch && t != original_victim)
                num++;
        }

        if (!num) {
            return original_victim;
        }

        num = number(1, num);
        for (t = world[ch->in_room].people; t != nullptr; t = t->next_in_room) {
            if (t != ch && t != original_victim) {
                --num;
                if (!num) {
                    break;
                }
            }
        }
        return t;
    }

    int get_base_skill_damage(const char_data& olog_hai, int prob) {
        int base_damage = (2 + utils::get_prof_level(PROF_WARRIOR, olog_hai));
        base_damage *= (100 + prob);
        base_damage /= (1000 / utils::get_tactics(olog_hai));
        if (utils::is_twohanded(olog_hai)) {
            base_damage *= 3 / 2;
        }
        return base_damage;
    }

    int calculate_smash_damage(const char_data& attacker, int prob) {
        int damage = get_base_skill_damage(attacker, prob);

        if (utils::get_specialization(attacker) == game_types::PS_WildFighting) {
            damage += 5;
        }
        return damage;
    }

    int calculate_cleave_damage(const char_data& attacker, int prob) {
        int damage = get_base_skill_damage(attacker, prob);

        if (utils::get_specialization(attacker) == game_types::PS_HeavyFighting) {
            damage += 5;
        }

        return damage;
    }

    int calculate_stomp_damage(const char_data& attacker, int prob) {
        return get_base_skill_damage(attacker, prob) / 2;
    }

    void apply_stomp_affect(char_data* attacker, char_data* victim) {
        game_rules::big_brother& bb_instance = game_rules::big_brother::instance();
        if (!bb_instance.is_target_valid(attacker, victim)) {
            send_to_char("You feel the Gods looking down upon you, and protecting your target.\r\n", attacker);
            return;
        }

        int prob = get_prob_skill(attacker, victim, SKILL_STOMP);

        if (prob < 0) {
            damage(attacker, victim, 0, SKILL_STOMP, 0);
            return;
        }
        int wait_delay = PULSE_VIOLENCE * 4 / 3 + number(0, PULSE_VIOLENCE);
        damage(attacker, victim, calculate_stomp_damage(*attacker, prob), SKILL_STOMP, 0);
        if (!IS_SET(victim->specials.affected_by, AFF_BASH)) {
            WAIT_STATE_FULL(victim, wait_delay, CMD_STOMP, 2, 80, 0, 0, 0, AFF_WAITING | AFF_BASH, TARGET_IGNORE);
        }
    }

    void apply_cleave_damage(char_data* attacker, char_data* victim) {
        game_rules::big_brother& bb_instance = game_rules::big_brother::instance();

        if (!bb_instance.is_target_valid(attacker, victim)) {
            send_to_char("You feel the Gods looking down upon you, and protecting your target.\r\n", attacker);
            return;
        }

        int prob = get_prob_skill(attacker, victim, SKILL_CLEAVE);

        if (prob < 0) {
            damage(attacker, victim, 0, SKILL_CLEAVE, 0);
            return;
        }

        damage(attacker, victim, calculate_cleave_damage(*attacker, prob), SKILL_CLEAVE, 0);
    }
}

ACMD(do_cleave)
{
    one_argument(argument, arg);
    
    if (!olog_hai::is_skill_valid(ch, SKILL_CLEAVE)) {
        return;
    }

    game_timer::skill_timer& timer = game_timer::skill_timer::instance();

    if (!timer.is_skill_allowed(*ch, SKILL_CLEAVE)) {
        send_to_char("You can't use this skill yet.\r\n", ch);
        return;
    }

    olog_hai::do_sanctuary_check(ch);
    char_data* victim = nullptr;
    char_data* nxt_victim = nullptr;

    for (victim = world[ch->in_room].people; victim; victim = nxt_victim) {
        nxt_victim = victim->next_in_room;
        if (victim != ch) {
            olog_hai::apply_cleave_damage(ch, victim);
        }
    }
    timer.add_skill_timer(*ch, SKILL_CLEAVE, CLEAVE_TIMER);    
}

ACMD(do_smash) 
{
    one_argument(argument, arg);

    if (!olog_hai::is_skill_valid(ch, SKILL_SMASH)) {
        return;
    }

    game_timer::skill_timer& timer = game_timer::skill_timer::instance();

    if (!timer.is_skill_allowed(*ch, SKILL_SMASH)) {
        send_to_char("You can't use this skill yet.\r\n", ch);
        return;
    }

    char_data* victim = olog_hai::is_smash_target_valid(ch, wtl);

    if (victim == nullptr) {
        return;
    }

    game_rules::big_brother& bb_instance = game_rules::big_brother::instance();
    if (!bb_instance.is_target_valid(ch, victim)) {
        send_to_char("You feel the Gods looking down upon you, and protecting your target.\r\n", ch);
        return;
    }

    olog_hai::do_sanctuary_check(ch);

    int prob = olog_hai::get_prob_skill(ch, victim, SKILL_SMASH);
    timer.add_skill_timer(*ch, SKILL_SMASH, SMASH_TIMER);

    if (prob < 0 ) { 
        damage(ch, victim, 0, SKILL_SMASH, 0);
        return;
    }


    /* 5% chance to swing on the wrong target */
    if (olog_hai::does_skill_hit_random(SKILL_SMASH)) {
        // find our target.
        char_data* new_target = olog_hai::get_random_target(ch, victim);
        if (new_target != victim) {
            damage(ch, victim, 0, SKILL_SMASH, 0);
            victim = new_target;
        }
    }

    damage(ch, victim, olog_hai::calculate_smash_damage(*ch, prob), SKILL_SMASH, 0);
}
ACMD(do_overrun) 
{
    return;
}
ACMD(do_frenzy) 
{
    return;
}
ACMD(do_stomp) 
{
    one_argument(argument, arg);
    if (!olog_hai::is_skill_valid(ch, SKILL_STOMP)) {
        return;
    }

    game_timer::skill_timer& timer = game_timer::skill_timer::instance();
    if (!timer.is_skill_allowed(*ch, SKILL_STOMP)) {
        send_to_char("You cannot use this skill yet.\r\n", ch);
        return;
    }

    olog_hai::do_sanctuary_check(ch);
    char_data* victim = nullptr;
    char_data* nxt_victim = nullptr;
    for (victim = world[ch->in_room].people; victim; victim = nxt_victim) {
        nxt_victim = victim->next_in_room;
        if (victim != ch) {
            olog_hai::apply_stomp_affect(ch, victim);
        }
    }
    timer.add_skill_timer(*ch, SKILL_STOMP, STOMP_TIMER);
}
