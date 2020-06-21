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
extern struct skill_data skills[];
void appear(struct char_data* ch);
int check_overkill(struct char_data* ch);
const int FRENZY_TIMER = 600;
const int SMASH_TIMER = 1;
const int STOMP_TIMER = 60;
const int CLEAVE_TIMER = 30;
const int OVERRUN_TIMER = 60;
ACMD(do_dismount);

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

    void generate_smash_dismount_messages(char_data* attacker, char_data* victim) {
        sprintf(buf, "You smash into $N so hard, it knocks $M to the ground!");
        act(buf, FALSE, attacker, NULL, victim, TO_CHAR);
        sprintf(buf, "$n smashes into you so hard, it knocks prone to the ground!");
        act(buf, FALSE, attacker, NULL, victim, TO_VICT);
        sprintf(buf, "$n smashes into $N so hard, it knocks $M to the ground!");
        act(buf, TRUE, attacker, 0, victim, TO_NOTVICT);
    }

    void apply_smash_damage(char_data* attacker, char_data* victim, int prob) {
        int dam = calculate_smash_damage(*attacker, prob);
        if (IS_RIDING(victim) && number() >= 0.80) {
            dam *= 1.25;
            char_data* mount = victim->mount_data.mount;
            damage(attacker, victim, dam, SKILL_SMASH, 0);
            generate_smash_dismount_messages(attacker, victim);
            do_dismount(victim, "", 0, 0, 0);
            if (number() >= 0.50) {
                damage(attacker, mount, dam / 2, SKILL_SMASH, 0);
            }
            if (!IS_SET(victim->specials.affected_by, AFF_BASH)) {
                int wait_delay = (PULSE_VIOLENCE + number(0, PULSE_VIOLENCE) / 2);
                WAIT_STATE(victim, wait_delay);
            }
            return;
        }
        damage(attacker, victim, dam, SKILL_SMASH, 0);
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
            WAIT_STATE(victim, wait_delay);
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

    void generate_frenzy_message(char_data* character) {
        sprintf(buf, "You enter a frenzied state, filling your body with overwhelming power!\r\n");
        act(buf, FALSE, character, NULL, NULL, TO_CHAR);
        sprintf(buf, "$n enters a frenzied state, making $s strikes grow with fervor!\r\n");
        act(buf, TRUE, character, 0, NULL, TO_ROOM);
    }

    void apply_frenzy_affect(char_data* character) {
        // generate messages
        generate_frenzy_message(character);
        // create affect
        struct affected_type af;
        af.type = SKILL_FRENZY;
        af.duration = 15;
        af.modifier = 20;
        af.location = APPLY_NONE;
        af.bitvector = 0;
        // apply affect
        affect_to_char(character, &af);
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
    olog_hai::apply_smash_damage(ch, victim, prob);
}

ACMD(do_overrun) 
{
    return;
}
ACMD(do_frenzy) 
{
    one_argument(argument, arg);

    if (subcmd == -1) {
        send_to_char("You could not concentrate anymore!\r\n", ch);
        wtl->targ1.cleanup();
        wtl->targ2.cleanup();
        ch->specials.ENERGY = std::min(ch->specials.ENERGY, 0);
        return;
    }

    if (!olog_hai::is_skill_valid(ch, SKILL_FRENZY)) {
        return;
    }

    if (affected_by_spell(ch, SKILL_FRENZY)) {
        send_to_char("You are already in a frenzy!\r\n", ch);
        return;
    }

    game_timer::skill_timer& timer = game_timer::skill_timer::instance();
    if (!timer.is_skill_allowed(*ch, SKILL_FRENZY)) {
        send_to_char("You cannot use this skill yet.\r\n", ch);
        return;
    }

    olog_hai::do_sanctuary_check(ch);
    olog_hai::apply_frenzy_affect(ch);
    timer.add_skill_timer(*ch, SKILL_FRENZY, FRENZY_TIMER);

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
