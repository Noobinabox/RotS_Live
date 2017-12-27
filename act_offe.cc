/* ************************************************************************
*   File: act.offensive.c                               Part of CircleMUD *
*  Usage: player-level commands of an offensive nature                    *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993 by the Trustees of the Johns Hopkins University     *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <string.h>
#include "platdef.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpre.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "limits.h"
#include "color.h"

#include "big_brother.h"
#include "char_utils.h"

/* extern variables */
extern struct room_data world;
extern struct descriptor_data *descriptor_list;
extern struct char_data * waiting_list;
extern char *  dirs[];
extern int	rev_dir[];
extern struct skill_data skills[MAX_SKILLS];

ACMD(do_look);
ACMD(do_stand);
ACMD(do_move);
ACMD(do_mental);

/* extern functions */
extern void raw_kill(char_data* ch, char_data* killer, int attacktype);
int	check_simple_move(struct char_data *ch, int cmd, int * move_cost, int mode);
int get_real_stealth(struct char_data * ch);
int	find_door(struct char_data *ch, char *type, char *dir);
void	group_gain(struct char_data *ch, struct char_data *victim);
int	get_number(char **name);
int check_overkill(struct char_data *ch);
int check_hallucinate(struct char_data *ch, struct char_data *victim);
void    appear(struct char_data *ch);

ACMD(do_hit)
{
  struct char_data *victim;
  
  // is this a peace room?  - Seether
  if(IS_SET(world[ch->in_room].room_flags, PEACEROOM)) {
    send_to_char("A peaceful feeling overwhelms you, and you cannot bring "
		 "yourself to attack.\n\r", ch);
    return;
  }
  
  if(IS_MENTAL(ch) || (subcmd == SCMD_WILL)) {	// You really meant 'will $n'
    do_mental(ch, argument, wtl, CMD_HIT, 0);
    return;
  }
  
  if(argument)
    one_argument(argument, arg);
  else
    *arg = 0;
  
  victim = 0;
  
  if(wtl && (wtl->targ1.type==TARGET_CHAR) && char_exists(wtl->targ1.ch_num))
    victim = wtl->targ1.ptr.ch;
  else
    if(*arg)
      victim = get_char_room_vis(ch, arg);
  
  if(victim && CAN_SEE(ch, victim) && (victim->in_room == ch->in_room)) {
    if(victim == ch) {
      send_to_char("You hit yourself...OUCH!.\n\r", ch);
      act("$n hits $mself, and says OUCH!", FALSE, ch, 0, victim, TO_ROOM);
      return;
    }
	if ((IS_AFFECTED(ch, AFF_CHARM) && ch->master == victim) || 
		(IS_AFFECTED(victim, AFF_CHARM) && victim->master == ch))
	{
		act("$N is just such a good friend, you simply can't hit $M.",
			FALSE, ch, 0, victim, TO_CHAR);
		return;
	}
    if(!IS_NPC(ch) && IS_NPC(victim) && (subcmd == SCMD_HIT) && 
       (GET_LEVEL(ch) < 4) && (GET_LEVEL(victim) > GET_LEVEL(ch)*3 + 2)) {
      act("Your arm refuses to attack such a powerful opponent.\n\r"
	  "Try to consider your enemy first.\n\r", 
	  FALSE, ch, 0, victim, TO_CHAR);
      return;
    }
    if(!IS_NPC(ch) && GET_LEVEL(ch) < 10 && !IS_NPC(victim) && RACE_GOOD(ch) &&
       RACE_GOOD(victim)) {
      act("The will of the Valar holds your attack in check.\n\r",
	  FALSE, ch, 0, victim, TO_CHAR);
      return;
    }
	if (!IS_NPC(ch) && !IS_NPC(victim) &&
		((GET_LEVEL(ch) + GET_LEVEL(victim)) < 8) && !other_side(ch, victim)) {
		act("You can't do it now. Group with this person and get some levels "
			"instead.\n\r", FALSE, ch, 0, victim, TO_CHAR);
		return;
	}

	game_rules::big_brother& bb_instance = game_rules::big_brother::instance();
	if (!bb_instance.is_target_valid(ch, victim))
	{
		send_to_char("You feel the Gods looking down upon you, and protecting your target.  Your hand is stayed.\r\n", ch);
		return;
	}
    
    if(!check_sanctuary(ch, victim))
      return;
    
    if ((GET_POS(ch) == POSITION_STANDING) && 
	(victim != ch->specials.fighting)) {
      act("$n attacks $N!", TRUE, ch, 0, victim, TO_NOTVICT);
      act("$CD$n attacks YOU!", FALSE, ch, NULL, victim, TO_VICT);
      act("$CHYou attack $N!", FALSE, ch, NULL, victim, TO_CHAR);
      
      hit(ch, victim, TYPE_UNDEFINED);
      
      WAIT_STATE(ch, 2); /* HVORFOR DET?? */
    } else {
      if(victim == ch->specials.fighting)
	send_to_char("You do the best you can!\n\r", ch);
      else if(victim->specials.fighting != ch)
	send_to_char("You already have a fight to worry about.\n\r", ch);
      else {
	ch->specials.fighting = victim;
	act("$n turns to fight you!", TRUE, ch, 0, victim, TO_VICT);
	act("$n turns to fight $N.", TRUE, ch, 0, victim, TO_NOTVICT);
	act("You turn to fight $N.", FALSE, ch, 0, victim, TO_CHAR);
      }
    }
  } else {
    if(*arg)
      send_to_char("They aren't here.\n\r", ch);
    else
      send_to_char("Hit who?\n\r", ch);
  }
}



ACMD(do_assist)
{
	struct char_data *helpee, *opponent;
	struct waiting_type tmpwtl;

	if (ch->specials.fighting) {
		send_to_char("You're already fighting!  How can you assist someone else?\n\r", ch);
		return;
	}

	one_argument(argument, arg);

	if (!wtl) helpee = get_char_room_vis(ch, arg);
	else if ((wtl->targ1.type == TARGET_CHAR) && char_exists(wtl->targ1.ch_num)) {
		helpee = wtl->targ1.ptr.ch;
		if (helpee && helpee->in_room != ch->in_room) helpee = 0;
	}
	else helpee = 0;

	if (!helpee) {
		send_to_char("Whom do you wish to assist?\n\r", ch);
		return;
	}

	if (helpee == ch)
		send_to_char("You can't help yourself any more than this!\n\r", ch);
	else
	{
		//      for (opponent = world[ch->in_room].people; opponent && 
		//	     (opponent->specials.fighting != helpee); 
		//	   opponent = opponent->next_in_room)
		//	;
		opponent = helpee->specials.fighting;
		if (!opponent)
			act("But nobody is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
		else if (!CAN_SEE(ch, opponent))
			act("You can't see who is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
		else {

			game_rules::big_brother& bb_instance = game_rules::big_brother::instance();
			if (!bb_instance.is_target_valid(ch, opponent))
			{
				send_to_char("You feel the Gods looking down upon you, and protecting your target.  Your hand is stayed.\r\n", ch);
				return;
			}

			send_to_char("You join the fight!\n\r", ch);
			act("$N assists you!", 0, helpee, 0, ch, TO_CHAR);
			act("$n assists $N.", FALSE, ch, 0, helpee, TO_NOTVICT);
			tmpwtl.targ1.type = TARGET_CHAR;
			tmpwtl.targ1.ptr.ch = opponent;
			tmpwtl.targ1.ch_num = opponent->abs_number;
			tmpwtl.cmd = CMD_HIT;
			tmpwtl.subcmd = 0;
			do_hit(ch, argument, &tmpwtl, CMD_HIT, SCMD_MURDER);
		}
	}
}


ACMD(do_slay)
{
	struct char_data *victim;

	if ((GET_LEVEL(ch) < LEVEL_GOD) || IS_NPC(ch)) {
		do_hit(ch, argument, wtl, cmd, subcmd);
		return;
	}

	one_argument(argument, arg);

	if (!*arg) {
		send_to_char("Kill who?\n\r", ch);
	}
	else {
		if (!(victim = get_char_room_vis(ch, arg)))
			send_to_char("They aren't here.\n\r", ch);
		else if (ch == victim)
			send_to_char("Your mother would be so sad.. :(\n\r", ch);
		else if (GET_LEVEL(ch) <= GET_LEVEL(victim))
			send_to_char("You can only slay wimpier things\n\r", ch);
		else {
			act("You chop $M to pieces!  Ah!  The blood!", FALSE, ch, 0, victim, TO_CHAR);
			act("$N chops you to pieces!", FALSE, victim, 0, ch, TO_CHAR);
			act("$n bloodlessly slays $N!", FALSE, ch, 0, victim, TO_NOTVICT);
			raw_kill(victim, NULL, 0);
		}
	}
}

ACMD(do_order)
{
   char	name[100], message[256];
   char	buf[256];
   char found = FALSE;
   int	org_room;
   struct char_data *victim;
   struct follow_type *k;

   if (IS_SHADOW(ch)){
	send_to_char("Who'd listen to a shadow?.\n\r", ch);
	return;
  }

   half_chop(argument, name, message);

   if (!*name || !*message)
      send_to_char("Order who to do what?\n\r", ch);
   else if (!(victim = get_char_room_vis(ch, name)) && !is_abbrev(name, "followers"))
      send_to_char("That person isn't here.\n\r", ch);
   else if (ch == victim)
      send_to_char("You obviously suffer from skitzofrenia.\n\r", ch);

   else {
      if (IS_AFFECTED(ch, AFF_CHARM)) {
	 send_to_char("Your superior would not aprove of you giving orders.\n\r", ch);
	 return;
      }

      if (victim) 
	  {
         if (utils::is_guardian(*victim)) 
		 {
            send_to_char("Your guardian appears unwilling to do your bidding.\n\r", ch);
            return;
         }

	 sprintf(buf, "$N orders you to '%s'", message);
	 act(buf, FALSE, victim, 0, ch, TO_CHAR);

	 if(GET_SPEC(ch) != PLRSPEC_PETS)
	   act("$n gives $N an order.", FALSE, ch, 0, victim, TO_ROOM);

	 if ( (victim->master != ch) || !IS_AFFECTED(victim, AFF_CHARM) )
	    act("$n has an indifferent look.", FALSE, victim, 0, 0, TO_ROOM);
	 else {
	    send_to_char("Ok.\n\r", ch);
	    command_interpreter(victim, message);
	 }
      } else {  /* This is order "followers" */
	 sprintf(buf, "$n issues an order.");
	 act(buf, FALSE, ch, 0, victim, TO_ROOM);

	 org_room = ch->in_room;

	 for (k = ch->followers; k; k = k->next) {
	    if (org_room == k->follower->in_room)
	       if (IS_AFFECTED(k->follower, AFF_CHARM) && !utils::is_guardian(*k->follower)) {
		  found = TRUE;
		  command_interpreter(k->follower, message);
	       }
	 }
	 if (found)
	    send_to_char("Ok.\n\r", ch);
	 else
	    send_to_char("Nobody here is willing to carry out your orders!\n\r", ch);
      }
   }
}



/* XXX: Fix double message on flee */
ACMD(do_flee)
{
  int i, attempt, loose, die, res, move_cost;
  struct char_data *tmpch;
  void gain_exp(struct char_data *, int);
  
  if (GET_TACTICS(ch) == TACTICS_BERSERK) {
    send_to_char("You are too enraged to flee!", ch);
    return;
  }

  if (GET_POS(ch) < POSITION_SLEEPING)
    return;
  
  if (GET_POS(ch) < POSITION_FIGHTING)
    do_stand(ch, "", 0, 0, 0);
  
  for (i = 0; i < 6; i++) {
    attempt = number(0, NUM_OF_DIRS-1);
    res = CAN_GO(ch, attempt) && 
      !IS_SET(EXIT(ch, attempt)->exit_info, EX_NOFLEE) &&
      !IS_SET(EXIT(ch, attempt)->exit_info, EX_NOWALK);

    if (res) {
      res = !IS_SET(world[EXIT(ch, attempt)->to_room].room_flags, DEATH);
      if (IS_NPC(ch)) {
	res = res && (
		      (!IS_SET(ch->specials2.act, MOB_STAY_ZONE) ||
		       (world[EXIT(ch, attempt)->to_room].zone == 
			world[ch->in_room].zone)) &&
		      (!IS_SET(ch->specials2.act, MOB_STAY_TYPE) ||
		       (world[EXIT(ch, attempt)->to_room].sector_type == 
			world[ch->in_room].sector_type))
		      );
      }
    }
    
    if (res) {
      act("$n panics, and attempts to flee!",
	  TRUE, ch, 0, 0, TO_ROOM);

      die = check_simple_move(ch, attempt, &move_cost, SCMD_FLEE);
      if (!special(ch, cmd + 1, "", SPECIAL_COMMAND, 0) && 
	  number(0, 1) && die) {
	/* The escape has not succeded */
	switch (die) {
	case 1:
	case 2:
	case 5:
	case 7:
	  break;
	case 3:
	  act("$n tries to flee, but is too exhausted!", 
	      TRUE, ch, 0, 0, TO_ROOM);	       
	  send_to_char("You are too exhausted to flee!\n\r", ch);
	  break;
	case 6:
	case 4:
	  stop_riding(ch); 
	  break;
	};
      }

      /* If die = 0, implies success */
      if (!die) {
	if (ch->specials.fighting) {
	  loose = GET_LEVEL(ch) + GET_LEVEL(ch->specials.fighting);
	  if (!IS_NPC(ch))
	    gain_exp(ch, -loose);
	  
	  /* disengaging now moved into char_from_room  **********/
	  //	  if (ch->specials.fighting->specials.fighting == ch)
	  for(tmpch = world[ch->in_room].people; tmpch; 
	      tmpch = tmpch->next_in_room)
	    if (tmpch->specials.fighting == ch)
	      stop_fighting(tmpch);
	  stop_fighting(ch);
	}

	if (IS_AFFECTED(ch, AFF_HUNT))
	  REMOVE_BIT(ch->specials.affected_by, AFF_HUNT);

	send_to_char("You flee head over heels.\n\r", ch);
	act("$n flees head over heels!",
	    FALSE, ch, 0, 0, TO_ROOM);
	do_move(ch, dirs[attempt], 0, attempt + 1, SCMD_FLEE);
	return;
      }
      break;
    }
  }

  /* All flees were failed */
  send_to_char("PANIC!  You couldn't escape!\n\r", ch);
  act("$n panics, and attempts to flee!",
      TRUE, ch, 0, 0, TO_ROOM);

  return;
}



ACMD(do_bash)
{
	struct char_data *victim;
	int prob, door, next_mode, other_room, tmp;
	struct room_direction_data *back;
	char type[MAX_INPUT_LENGTH], dir[MAX_INPUT_LENGTH];

	if (IS_SHADOW(ch)) {
		send_to_char("You are too airy for that.\n\r", ch);
		subcmd = -1;
	}

	game_rules::big_brother& bb_instance = game_rules::big_brother::instance();
	one_argument(argument, arg);

	switch (subcmd) {
	case 0:
		if (GET_POS(ch) < POSITION_FIGHTING) {
			send_to_char("You need to stand up first.\n\r", ch);
			return;
		}

		door = -1;
		if (!(victim = get_char_room_vis(ch, arg))) {
			if (ch->specials.fighting)
				victim = ch->specials.fighting;
			else {
				argument_interpreter(argument, type, dir);
				door = find_door(ch, type, dir);
			}
		}

		if (victim) {
			if (IS_SET(world[ch->in_room].room_flags, PEACEROOM)) {
				send_to_char("A peaceful feeling overwhelms you, and you "
					"cannot bring yourself to attack.\n\r", ch);
				return;
			}
			if (victim == ch) {
				send_to_char("Aren't we funny today...\n\r", ch);
				return;
			}

			if (!bb_instance.is_target_valid(ch, victim, SKILL_BASH))
			{
				send_to_char("You feel the Gods looking down upon you, and protecting your target.  You freeze momentarily, and reconsider your action.\r\n", ch);
				return;
			}

			if (!check_hallucinate(ch, victim))
				return;
			if (IS_SHADOW(victim)) {
				send_to_char("Your bash encounters nothing but thin air.\n\r", ch);
				return;
			}
			if (!GET_SKILL(ch, SKILL_BASH)) {
				send_to_char("Learn how to bash properly first!\n\r", ch);
				return;
			}

			if (IS_SET(victim->specials.affected_by, AFF_BASH)) {
				send_to_char("Your victim is already bashed!\n\r", ch);
				return;
			}
			next_mode = 1;
		}
		else if (door >= 0) {
			if (IS_RIDING(ch)) {
				send_to_char("You cannot do it while riding.\n\r", ch);
				return;
			}
			if (!IS_SET(EXIT(ch, door)->exit_info, EX_ISDOOR)) {
				send_to_char("That's impossible, I'm afraid.\n\r", ch);
				return;
			}
			else if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED)) {
				send_to_char("It's already open!\n\r", ch);
				return;
			}
			next_mode = 3;
			if (GET_MOVE(ch) < 10) {
				send_to_char("You are too tired for that.\n\r", ch);
				return;
			}
		}
		else 
		{
			send_to_char("Bash who?\n\r", ch);
			return;
		}
		if (door >= 0)
			if (!check_overkill(victim)) {
				send_to_char("You cannot get close enough to bash.\n\r", ch);
				return;
			}

		WAIT_STATE_FULL(ch, skills[SKILL_BASH].beats, CMD_BASH,
			next_mode, 50, 0, (next_mode == 1) ?
			GET_ABS_NUM(victim) : door, victim,
			AFF_WAITING | AFF_WAITWHEEL,
			(next_mode == 1) ? TARGET_CHAR : TARGET_OTHER);
		return;

	case 1:
		if (GET_POS(ch) < POSITION_FIGHTING) {
			send_to_char("You need to stand up first.\n\r", ch);
			return;
		}
		if ((wtl->targ1.type != TARGET_CHAR) || !char_exists(wtl->targ1.ch_num)) {
			send_to_char("Your victim is no longer among us.\n\r", ch);
			return;
		}
		victim = (struct char_data *) wtl->targ1.ptr.ch;
		if (!CAN_SEE(ch, victim)) {
			send_to_char("Bash who?\n\r", ch);
			return;
		}
		if (ch->in_room != victim->in_room) {
			send_to_char("You target is not here any longer\n\r", ch);
			return;
		}
		if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_NOBASH)) {
			act("You try to bash $N, but only bruise yourself.",
				FALSE, ch, 0, victim, TO_CHAR);
			act("$n tries to bash $N, but only bruises $mself.",
				FALSE, ch, 0, victim, TO_ROOM);
			return;
		}
		if (IS_SET(victim->specials.affected_by, AFF_BASH)) {
			send_to_char("Your victim is already bashed!\n\r", ch);
			return;
		}

		if (!bb_instance.is_target_valid(ch, victim, SKILL_BASH))
		{
			send_to_char("You feel the Gods looking down upon you, and protecting your target.  You freeze momentarily, and reconsider your action.\r\n", ch);
			return;
		}

		/*
		 * bash prob. is like a hit, except victim parry is halved,
		 * and OB = OB*3/2 -= (1d60-30)
		 */
		prob = GET_SKILL(ch, SKILL_BASH);
		prob += get_real_OB(ch) * 3 / 2;
		prob -= get_real_dodge(victim);
		prob -= get_real_parry(victim) / 2;
		prob -= GET_LEVELA(victim) * 3;
		prob += GET_PROF_LEVEL(PROF_WARRIOR, ch);
		prob += number(0, MAX(1, 35 + get_real_OB(ch) / 4));
		prob += number(-40, 40);
		prob -= 160;
		if (prob < 0)
			damage(ch, victim, 0, SKILL_BASH, 0);
		else {
			WAIT_STATE_FULL(victim,
				PULSE_VIOLENCE * 3 / 2 + number(0, PULSE_VIOLENCE / 2),
				CMD_BASH, 2, 80, 0, 0, 0, AFF_WAITING | AFF_BASH,
				TARGET_IGNORE);
			damage(ch, victim, 1, SKILL_BASH, 0);
		}
		return;

	case 2:
		abort_delay(ch);
		REMOVE_BIT(ch->specials.affected_by, AFF_BASH);
		if (GET_POS(ch) > POSITION_INCAP) {
			act("$n has recovered from a bash!", TRUE, ch, 0, 0, TO_ROOM);
			act("You have regained your balance!", TRUE, ch, 0, 0, TO_CHAR);
			if (GET_POS(ch) < POSITION_FIGHTING)
				do_stand(ch, "", 0, 0, 0);
		}
		break;


	case 3:
		if (wtl->targ1.type != TARGET_OTHER) {
			send_to_char("Something went wrong in your bashing.\n\r", ch);
			return;
		}
		door = wtl->targ1.ch_num;
		if (!EXIT(ch, door)) {
			//       abort_delay(ch);
			return;
		}
		if (!IS_SET(EXIT(ch, door)->exit_info, EX_ISDOOR)) {
			send_to_char("That's impossible, I'm afraid.\n\r", ch);
			return;
		}
		else if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED)) {
			send_to_char("It's already open!\n\r", ch);
			return;
		}

		prob = (GET_SKILL(ch, SKILL_BASH) / 10 + GET_STR(ch) - 20) * 5;

		tmp = GET_RACE(ch);
		if ((tmp == RACE_WOOD) || (tmp == RACE_HIGH) || (tmp == RACE_HOBBIT)) {
			sprintf(buf, "You throw your light body on the %s.\n\r",
				EXIT(ch, door)->keyword);
			send_to_char(buf, ch);
			sprintf(buf, "$n throws $mself on the %s.\n\r",
				EXIT(ch, door)->keyword);
			act(buf, TRUE, ch, 0, 0, TO_ROOM);
			prob = -1;
		}
		else {
			sprintf(buf, "You throw yourself on the %s.\n\r",
				EXIT(ch, door)->keyword);
			send_to_char(buf, ch);
			sprintf(buf, "$n throws $mself on the %s.\n\r",
				EXIT(ch, door)->keyword);
			act(buf, TRUE, ch, 0, 0, TO_ROOM);
		}
		if ((tmp == RACE_URUK) || (tmp == RACE_DWARF)) prob += 10;

		if ((tmp == RACE_BEORNING) || (tmp == RACE_OLOGHAI)) prob += 20;

		if (IS_SET(EXIT(ch, door)->exit_info, EX_DOORISHEAVY)) prob = -1;
		if (IS_SET(EXIT(ch, door)->exit_info, EX_NOBREAK)) prob = -1;

		if (prob < 0) {
			sprintf(buf, "The %s would not budge.\n\r",
				EXIT(ch, door)->keyword);
			send_to_char(buf, ch);
			return;
		}
		if (number(0, 99) > prob) { //failure
			send_to_char("You only injure yourself.\n\r", ch);
			GET_HIT(ch) -= number(1, 20);
			GET_MOVE(ch) -= 10;
			update_pos(ch);
			if (GET_POS(ch) == POSITION_STANDING)
				GET_POS(ch) = POSITION_RESTING;
			return;
		}
		SET_BIT(EXIT(ch, door)->exit_info, EX_ISBROKEN);
		REMOVE_BIT(EXIT(ch, door)->exit_info, EX_CLOSED | EX_LOCKED);
		sprintf(buf, "The %s crashes open! You fall through it.\n\r",
			EXIT(ch, door)->keyword);
		send_to_char(buf, ch);
		sprintf(buf, "The %s crashes open! $n falls through it.\n\r",
			EXIT(ch, door)->keyword);
		act(buf, TRUE, ch, 0, 0, TO_ROOM);
		if ((other_room = EXIT(ch, door)->to_room) != NOWHERE) {
			if ((back = world[other_room].dir_option[rev_dir[door]]))
				if (back->to_room == ch->in_room) {
					SET_BIT(back->exit_info, EX_ISBROKEN);
					REMOVE_BIT(back->exit_info, EX_CLOSED | EX_LOCKED);
					if (back->keyword) {
						sprintf(buf, "The %s suddenly crashes open.\n\r",
							fname(back->keyword));
						send_to_room(buf, EXIT(ch, door)->to_room);
					}
					else
						send_to_room("The door is opened from the other side.\n\r",
							EXIT(ch, door)->to_room);
				}
			stop_riding(ch);
			char_from_room(ch);
			char_to_room(ch, other_room);
			do_look(ch, "\0", 0, 0, 0);
			act("$n falls in.", TRUE, ch, 0, 0, TO_ROOM);
		}
		GET_POS(ch) = POSITION_SITTING;
		WAIT_STATE_FULL(ch, PULSE_VIOLENCE * 3, CMD_BASH, 2, 80, 0, 0, 0, AFF_WAITING | AFF_BASH, TARGET_NONE);

		break;
	case -1:                 // emergency exit.
	default:
		REMOVE_BIT(ch->specials.affected_by, AFF_BASH);
		abort_delay(ch);
		return;
	}
}


char * rescue_message [MAX_RACES] [2] = {
  {"Fearless, you leap into harms way!\r\n",
   "$n rescues $N.\r\n"},
  {"Shouting 'Elendil!', you leap to the rescue!\r\n",
   "Shouting 'Elendil', $n fearlessly rescues $N.\r\n"},
  {"A harsh guttural warcry escapes your lips as you leap to the rescue!\r\n",
   "Shouting 'Khazad Ai-Menu' $n fearlessly rescues $N.\r\n"},
  {"Shouting 'A Elbereth Githloriel!', you leap to the rescue.\r\n", 
   "Shouting 'A Elbereth Githloriel', $n fearlessly rescues $N.\r\n"},
  {"Shouting 'For the Shire!', you leap to the rescue.\r\n", 
   "Shouting 'For the Shire', $n the brave little halfling rescues $N.\r\n"}
};

ACMD(do_rescue)
{

	int room_message = 1;

	struct char_data *victim, *tmp_ch;
	byte	percent, prob, shadow_flag;

	if (IS_SHADOW(ch)) {
		send_to_char("You are too insubstantial to do that.\n\r", ch);
		return;
	}

	if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_ORC_FRIEND) && MOB_FLAGGED(ch, MOB_PET)) {
		act("$n shrugs at you.", FALSE, ch, 0, ch->master, TO_VICT);
		act("$n shrugs at $N.", FALSE, ch, 0, ch->master, TO_NOTVICT);
		return;
	}

	if (wtl) {
		if ((wtl->targ1.type != TARGET_CHAR) || !char_exists(wtl->targ1.ch_num) || wtl->targ1.ptr.ch->in_room != ch->in_room) {
			send_to_char("Alas! You lost your victim.\n\r", ch);
			return;
		}
		victim = wtl->targ1.ptr.ch;
	}
	else {
		one_argument(argument, arg);

		if (!(victim = get_char_room_vis(ch, arg))) {
			send_to_char("Who do you want to rescue?\n\r", ch);
			return;
		}
	}
	if (victim == ch) {
		send_to_char("What about fleeing instead?\n\r", ch);
		return;
	}

	if (ch->specials.fighting == victim) {
		send_to_char("How can you rescue someone you are trying to kill?\n\r", ch);
		return;
	}

	shadow_flag = IS_SHADOW(ch);

	for (tmp_ch = world[ch->in_room].people; tmp_ch &&
		((tmp_ch->specials.fighting != victim) ||
		(shadow_flag && !IS_SHADOW(tmp_ch)) ||
			(!shadow_flag && IS_NPC(tmp_ch) && IS_SHADOW(tmp_ch)));
		tmp_ch = tmp_ch->next_in_room)
		;

	if (!tmp_ch) {
		if (!shadow_flag)
			act("But no mortal is fighting $M!", FALSE, ch, 0, victim, TO_CHAR);
		else
			act("But no wraith is fighting $M!", FALSE, ch, 0, victim, TO_CHAR);
		return;
	}

	percent = number(1, 101); /* 101% is a complete failure */
	prob = GET_SKILL(ch, SKILL_RESCUE);

	if (percent > prob) {
		send_to_char("You fail the rescue!\n\r", ch);
		return;
	}

	if (GET_RACE(ch) >= 5) 
	{
		send_to_char("You shouldn't have rescue, stop using it!\r\n", ch);
		return;
	}

	send_to_char(rescue_message[GET_RACE(ch)][0], ch);
	act("To your relief $N has rescued you.", FALSE, victim, 0, ch, TO_CHAR);
	act(rescue_message[GET_RACE(ch)][room_message], FALSE, ch, 0, victim, TO_NOTVICT);

	if (victim->specials.fighting == tmp_ch)
		stop_fighting(victim);
	if (tmp_ch->specials.fighting)
		stop_fighting(tmp_ch);
	if (victim->specials.fighting)
		stop_fighting(victim);

	set_fighting(ch, tmp_ch);
	set_fighting(tmp_ch, ch); // Was commented out.  Uncommented by Fingolfin 1/1/02
	tmp_ch->specials.fighting = ch;

	// Defender specialized characters don't have a delay from rescue.
	game_types::player_specs spec = utils::get_specialization(*ch);
	if (spec != game_types::PS_Defender)
	{
		WAIT_STATE(victim, PULSE_VIOLENCE / 3);
		WAIT_STATE(ch, PULSE_VIOLENCE / 2);
	}
}



/*
 * Also handles wild swing.
 */
ACMD(do_kick)
{
	struct char_data *victim;
	struct char_data *t;
	sh_int prob, num, dam;
	int attacktype;

	if (IS_SHADOW(ch)) {
		send_to_char("You are too insubstantial to do that.\n\r", ch);
		return;
	}

	if (subcmd == SCMD_KICK)
		attacktype = SKILL_KICK;
	else if (subcmd == SCMD_SWING)
		attacktype = SKILL_SWING;
	else
		return;

	if (IS_SET(world[ch->in_room].room_flags, PEACEROOM)) {
		send_to_char("A peaceful feeling overwhelms you, "
			"and you cannot bring yourself to attack.\n\r", ch);
		return;
	}

	victim = NULL;
	if (wtl && wtl->targ1.type == TARGET_CHAR &&
		char_exists(wtl->targ1.ch_num)) {
		victim = wtl->targ1.ptr.ch;
	}
	else if (wtl && (wtl->targ1.type == TARGET_TEXT)) {
		victim = get_char_room_vis(ch, wtl->targ1.ptr.text->text);
	}
	else {
		one_argument(argument, arg);
		victim = get_char_room_vis(ch, arg);
	}

	if (!victim) {
		if (ch->specials.fighting) {
			victim = ch->specials.fighting;
		}
		else {
			if (attacktype == SKILL_KICK)
				send_to_char("Kick who?\n\r", ch);
			else
				send_to_char("Swing at who?\n\r", ch);

			return;
		}
	}

	game_rules::big_brother& bb_instance = game_rules::big_brother::instance();
	if (!bb_instance.is_target_valid(ch, victim, attacktype))
	{
		send_to_char("You feel the Gods looking down upon you, and protecting your target.  You freeze momentarily, and reconsider your action.\r\n", ch);
		return;
	}

	if (!check_overkill(victim)) {
		send_to_char("You cannot get close enough!\n\r", ch);
		return;
	}

	if (victim->in_room != ch->in_room) {
		if (attacktype == SKILL_KICK)
			send_to_char("Kick who?\n\r", ch);
		else
			send_to_char("Swing at who?\n\r", ch);

		return;
	}

	if (victim == ch) {
		send_to_char("Aren't we funny today...\n\r", ch);
		return;
	}

	if (IS_SHADOW(victim)) {
		if (attacktype == SKILL_KICK)
			send_to_char("Your kick encounters nothing but thin air.\n\r", ch);
		else
			send_to_char("Your swing encounters nothing but thin air.\n\r", ch);

		return;
	}

	if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_NOBASH)) {
		if (attacktype == SKILL_KICK) {
			act("You kick $N, but only bruise yourself.",
				FALSE, ch, 0, victim, TO_CHAR);
			act("$n kicks $N, but only bruises $mself.",
				FALSE, ch, 0, victim, TO_ROOM);
		}
		else {
			act("You swing at $N, but only bruise yourself.",
				FALSE, ch, 0, victim, TO_CHAR);
			act("$n swings at $N, but only bruises $mself.",
				FALSE, ch, 0, victim, TO_ROOM);
		}

		return;
	}

	if (!GET_SKILL(ch, attacktype)) {
		send_to_char("Learn how to do it first!\n\r", ch);
		return;
	}

	/* %20 chance to swing the wrong person */
	if (attacktype == SKILL_SWING && !number(0, 4)) {
		num = 0;
		for (t = world[ch->in_room].people; t != NULL; t = t->next_in_room)
			if (t != ch && t != victim)
				num++;

		if (!num) {
			damage(ch, victim, 0, attacktype, 0);
			goto delay;
		}

		num = number(1, num);

		for (t = world[ch->in_room].people; t != NULL; t = t->next_in_room)
			if (t != ch && t != victim) {
				--num;
				if (!num)
					break;
			}

		damage(ch, victim, 0, attacktype, 0);

		victim = t;
	}

	prob = GET_SKILL(ch, attacktype);
	prob -= get_real_dodge(victim) / 2;
	prob -= get_real_parry(victim) / 2;
	prob += get_real_OB(ch) / 2;
	prob += number(1, 100);
	prob -= 120;

	dam = (2 + GET_PROF_LEVEL(PROF_WARRIOR, ch)) *
		(100 + prob) / 250;

	if (attacktype == SKILL_SWING)
		dam = dam * 3 / 2;

	if (prob < 0)
		damage(ch, victim, 0, attacktype, 0);
	else
	{
		//TODO(drelidan):  If heavy fighting needs any additional bonuses, start here.
		// Heavy fighters kick 20% harder.
		/*
		if (utils::get_specialization(*ch) == (int)game_types::PS_HeavyFighting)
		{
			dam += dam / 5;
		}
		*/
		damage(ch, victim, dam, attacktype, 0);
	}

delay:
	WAIT_STATE_FULL(ch, PULSE_VIOLENCE * 4 / 3 +
		number(0, PULSE_VIOLENCE),
		0, 0, 59, 0, 0, 0, AFF_WAITING, TARGET_NONE);
}



ACMD(do_disengage){
  
  if(!ch->specials.fighting){
    send_to_char("You are not fighting anybody.\n\r",ch);
    return;
  }

  if(ch->specials.fighting->specials.fighting == ch){
    act("$E won't let you off that easy!",FALSE, ch, 0, ch->specials.fighting,
	TO_CHAR);
    return;
  }

 WAIT_STATE_FULL(ch, PULSE_VIOLENCE * 2, 0, 2, 80, 0,0,0, AFF_WAITING, TARGET_NONE);

  act("$n stops fighting $N.",TRUE, ch, 0, ch->specials.fighting, TO_NOTVICT);
  act("$n stops fighting you.",TRUE, ch, 0, ch->specials.fighting, TO_VICT);
  act("You stop fighting $N.",TRUE, ch, 0, ch->specials.fighting, TO_CHAR);
  stop_fighting(ch);
  
}


ACMD(do_beorning)
{
	return;
}

