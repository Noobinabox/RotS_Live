/* ************************************************************************
*  File: Ranger.cc                                Part of CircleMUD       *
*  Usage: Handles our ranger coeff skills                                 *
*  Created 27/04/05                                                       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993 by the Trustees of the Johns Hopkins University     *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.
************************************************************************ */

#include <ctype.h>
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
#include "script.h"

#include "char_utils.h"
#include "char_utils_combat.h"

#include <algorithm>
#include <sstream>
#include <iostream>
#include <vector>

typedef char * string;

/*
 * External Variables and structures
 */

extern struct char_data *character_list;
extern struct char_data * waiting_list;
extern struct room_data world;
extern struct skill_data skills[];
extern int  find_door(struct char_data *ch, char *type, char *dir);
extern int get_followers_level(struct char_data *);
extern int old_search_block(char *, int, unsigned int, const char **, int);
extern int get_number(char **name);
extern int get_real_stealth(struct char_data *);
extern int	rev_dir[];
extern int show_tracks(struct char_data *ch, char *name, int mode);
extern int top_of_world;
extern char	*dirs[];
extern void appear(struct char_data *ch);
extern void check_break_prep(struct char_data *);
extern void stop_hiding(struct char_data *ch, char);
extern void update_pos(struct char_data *victim);


ACMD (do_move);
ACMD (do_hit);
ACMD (do_gen_com);



ACMD (do_ride) {

  follow_type * tmpfol;
  char_data * tmpch;
  char_data * mountch;

  if (char_exists(ch->mount_data.mount_number) && ch->mount_data.mount) {
    send_to_char("You are riding already.\n\r", ch);
    return;
  }
  /* only people can ride (bodytype 1) */
  if (GET_BODYTYPE(ch)!=1)
    return;

  if (IS_SHADOW(ch)) {
    send_to_char("You cannot ride whilst inhabiting the shadow world.\n\r", ch);
    return;
  }

  while (*argument && (*argument <= ' ')) argument++;
  mountch = 0;
  if (!*argument) {
    for (tmpfol = ch->followers; tmpfol; tmpfol = tmpfol->next)
      if (char_exists(tmpfol->fol_number) &&
	  tmpfol->follower->in_room == ch->in_room &&
	  IS_NPC(tmpfol->follower) &&
	  IS_SET(tmpfol->follower->specials2.act, MOB_MOUNT))
	break;

    if (tmpfol) {
      mountch = tmpfol->follower;
      stop_follower(mountch, FOLLOW_MOVE);
    } else {
      send_to_char("What do you want to ride?\n\r",ch);
      return;
    }
  } else {
    tmpch = get_char_room_vis(ch, argument);
    if (!tmpch) {
      send_to_char("There is nobody by that name.\n\r",ch);
      return;
    }
    if (IS_NPC(tmpch) && !IS_SET(tmpch->specials2.act, MOB_MOUNT) ||
        !IS_NPC(tmpch)) {
      send_to_char("You can not ride this.\n\r",ch);
      return;
    }
    if (IS_AGGR_TO(tmpch, ch)) {
      act("$N doesn't want you to ride $M.",FALSE, ch, 0, tmpch, TO_CHAR);
      return;
    }
    if (tmpch->mount_data.mount) {
      act("$N is not in a position for you to ride.",FALSE, ch, 0, tmpch, TO_CHAR);
      return;
    }
    mountch = tmpch;
  }

  if (!mountch)
    return;
  if (mountch == ch) {
    send_to_char("You tried to mount yourself, but failed.\n\r",ch);
    return;
  }
  if (IS_RIDING(mountch)) {
    mountch = mountch->mount_data.mount;
  } else
    stop_follower(mountch, FOLLOW_MOVE);

  /* Mounting a horse now */
  ch->mount_data.mount = mountch;
  ch->mount_data.mount_number = mountch->abs_number;
  ch->mount_data.next_rider = 0;
  ch->mount_data.next_rider_number = -1;

  if (!IS_RIDDEN(mountch)) {
    act("You mount $N and start riding $m.", FALSE, ch, 0, mountch, TO_CHAR);
    act("$n mounts $N and starts riding $m.", FALSE, ch, 0, mountch, TO_ROOM);
    mountch->mount_data.rider = ch;
    mountch->mount_data.rider_number = ch->abs_number;
  } else {
    act("You mount $N.", FALSE, ch, 0, mountch, TO_CHAR);
    act("$n mounts $N.", FALSE, ch, 0, mountch, TO_ROOM);

    for (tmpch = mountch->mount_data.rider;
	 tmpch->mount_data.next_rider &&
	   char_exists(tmpch->mount_data.next_rider_number);
	 tmpch = tmpch->mount_data.next_rider);

    tmpch->mount_data.next_rider = ch;
    tmpch->mount_data.next_rider_number = ch->abs_number;
  }
  IS_CARRYING_W(mountch) += GET_WEIGHT(ch) + IS_CARRYING_W(ch);
}

ACMD (do_dismount) {
  struct char_data * mount = 0;
  struct char_data * were_rider;

  if (!ch) {
    sprintf(buf,"Screwed mount %s, no rider, all be wary!", GET_NAME(mount));
    mudlog(buf, NRM, LEVEL_IMMORT, TRUE);
    return;
  }
  if (!IS_RIDING(ch))
    send_to_char("You are not riding anything.\n\r",ch);
  else{
    mount = ch->mount_data.mount;
    if (IS_RIDDEN(mount)) {
      were_rider = mount->mount_data.rider;
    } else {
      sprintf(buf,"Screwed mount %s, all be wary!",GET_NAME(mount));
      mudlog(buf, NRM, LEVEL_IMMORT, TRUE);
      were_rider = 0;
    }
    stop_riding(ch);
    if (IS_NPC(mount) && (were_rider == ch) &&
	(ch->specials.fighting != mount))
      add_follower(mount, ch, FOLLOW_MOVE);
  }
}

ACMD (do_track) {
  int count;

  if (subcmd == -1) {
    abort_delay(ch);
    return;
  }
  if (IS_SHADOW(ch)) {
    send_to_char("You can't see the physical world well enough.\n\r", ch);
    return;
  }

  if (subcmd == 0) {
    if (!CAN_SEE(ch)) {
      send_to_char("You can't see to track!", ch);
      return;
    }
    if (wtl && (wtl->targ1.type == TARGET_TEXT))
      argument = wtl->targ1.ptr.text->text;
    if (IS_WATER(ch->in_room)) {
      send_to_char("You can't track in water!\n\r", ch);
      return;
    }
    send_to_char("You start searching the ground for tracks.\n\r", ch);
    act("$n searches the ground for tracks.",TRUE, ch, 0, 0, TO_ROOM);
    WAIT_STATE_FULL(ch, skills[SKILL_TRACK].beats ,CMD_TRACK, 1, 30,
		    0, 0, get_from_txt_block_pool(argument),
		    AFF_WAITING | AFF_WAITWHEEL, TARGET_TEXT);
    return;
  }
  if (ch->delay.targ1.type == TARGET_TEXT)
    argument = ch->delay.targ1.ptr.text->text;
  count = show_tracks(ch, argument, 1);

  if (ch->delay.targ1.type == TARGET_TEXT)
    ch->delay.targ1.cleanup();

  if (count == 0)
    send_to_char("You found no tracks here.\n\r",ch);
}


/*
 * This is an array of the sector "percentage" bonus values
 * used by gather.
 */
int sector_variables [] = {
  -200, /*Floor*/
  -70,  /*City*/
  20,   /*Field*/
  20,   /*Forest*/
  0,    /*Hills*/
  -20,  /*Mountains*/
  -200, /*Water*/
  -200, /*No_Swim Water*/
  -200, /*UnderWater*/
  -30,  /*Road*/
  -200, /*Crack*/
  20,   /*Dense Forest*/
  20    /*Swamp*/
};


/*
 * This function checks the conditions needed to gather
 * with success.
 */
int check_gather_conditions (struct char_data *ch, int percent, int gather_type) {

  /*
   * The way gather was originally implemented was terrible,
   * because of this i have to but in a second checker for
   * argument here to stop a bug. Nested switch statements
   * using the same variable to switch is a bad idea. . .
   */
  if (gather_type == 0) {
    send_to_char("You can gather food, healing, energy or light.\n\r", ch);
    return FALSE;
  }
  if (affected_by_spell(ch, SKILL_GATHER_FOOD) && gather_type > 2) {
    send_to_char("You would gain no benefit from this right now.\n\r", ch);
    return FALSE;
  }
  if (gather_type == 1 && (GET_RACE(ch) > 9)) {
    send_to_char("The fruits of Arda are as poison to you.\n\r", ch);
    return FALSE;
  }

  /*
   * Checks our sector conditions (if any)
   */
  switch(world[ch->in_room].sector_type) {
  case SECT_INSIDE:
    send_to_char ("You can not gather herbs inside!\n\r", ch);
    return FALSE;
  case SECT_CITY:
    send_to_char ("You might have better luck "
		  "outside of city\n\r", ch);
    return FALSE;
  case SECT_FIELD:
    send_to_char("Learning how to gather herbs better or,"
		 " perhaps, going to the forest could help.\r\n", ch);
    return FALSE;
  case SECT_SWAMP:
    if (gather_type == 2) {
      send_to_char("Trying to gather dry kindle-wood in a swamp is"
		   " an exercise in futility.\r\n", ch);
      return FALSE;
    }
    if (GET_RACE(ch) < 9) {
      send_to_char("There are no plants or herbs of any value"
		   " to you here.\r\n", ch);
      return FALSE;
    }
    else
      return TRUE;
  default:
    if (percent <= 10) {
      send_to_char("You are unable to find anything useful here.\r\n", ch);
      return FALSE;
    }
    else
      return TRUE;
  }
  return TRUE;
}



ACMD (do_gather_food) {
  struct obj_data *obj;
  struct affected_type af;
  sh_int percent, move_use, GatherType;
  int affects_last, ranger_bonus;


  /*
   * Replaced a series of if/else statements which used to determine
   * what subcommand of gather you were using. Replaced it with
   * a neat little array, and used search_block to get the desired
   * values.
   */
  static char *gather_type [] = {
    "",
    "food",
    "light",
    "healing",
    "energy",
    "\n"
  };

  if (IS_SHADOW(ch)) {
    send_to_char("You are too insubstantial to do that.\n\r", ch);
    return;
  }
  if (IS_NPC(ch) && (MOB_FLAGGED(ch, MOB_ORC_FRIEND))) {
    send_to_char("Leave that to your leader.\n\r", ch);
    return;
  }

  percent = GET_SKILL(ch,SKILL_GATHER_FOOD);
  /* This is where we get our sector bonus */
  percent += sector_variables[world[ch->in_room].sector_type];

  switch (subcmd) {
  case -1:
    abort_delay(ch);
    return;
  case 0:
        move_use = 25 - percent / 5;
    if (GET_MOVE(ch) < move_use) {
      send_to_char("You are too tired for this right now\n\r", ch);
      return;
    }
    one_argument(argument, arg);
    GatherType = search_block(arg, gather_type, 0);
    if (GatherType == -1) { /*If we can't find an argument */
      send_to_char("You can gather food, healing, energy or light.\n\r", ch);
      return;
    }
    if (!check_gather_conditions(ch, percent, GatherType)) /*Checks sector and race conditions */
       return;

    /*And we've passed our conditions so we're going on now to try and gather */
    WAIT_STATE_FULL(ch, MIN(25, 40 - GET_SKILL(ch, SKILL_GATHER_FOOD) / 5), CMD_GATHER_FOOD, GatherType,
		    30, 0, 0, 0, AFF_WAITING | AFF_WAITWHEEL, TARGET_NONE);
    GET_MOVE(ch) -= move_use;
    break;
  default:
    if (number(0, 100) > (subcmd < 3 ? percent:percent - 10)) {
      send_to_char("You tried to gather some herbs, but could not find"
		   " anything useful.\n\r", ch);
      return;
    }
    else {
      /*
       * This portion of the code was relatively untouched
       * and stays in its original format. A few lines concerning
       * objects and darkie races were removed.
       * This is where we load our objects for gather light and food
       * and do our calculations for healing and energy.
       */
      switch(subcmd) {
      case 1:
	if ((obj = read_object(7218, VIRT)) != NULL) {
	  send_to_char("You look around, and manage to find some edible"
		       " roots and berries.\n\r", ch);
	  obj_to_char(obj, ch);
	} else
	  send_to_char("Minor problem in gather food. Could not create item."
		       " Please notify imps.\n\r", ch);
	break;
      case 2:
	if ((obj = read_object(7007, VIRT)) != NULL) {
	  send_to_char("You gather some wood and fashion it into a"
		       " crude torch.\n\r", ch);
	    obj_to_char(obj, ch);
	} else
	  send_to_char("Problem in gather light.  Could not create item."
		       "Please notify imps.\n\r", ch);
	break;
      case 3:
	GET_HIT(ch) = MIN(GET_HIT(ch) + GET_SKILL(ch, SKILL_GATHER_FOOD) / 3
			  + GET_PROF_LEVEL(PROF_RANGER, ch), GET_MAX_HIT(ch));
	send_to_char("You look around, and manage to find some healing"
		     " herbs.\n\r", ch);
	break;
      case 4:
	GET_MOVE(ch) = MIN(GET_MOVE(ch) + GET_SKILL(ch, SKILL_GATHER_FOOD) * 2 / 3 +
			   GET_PROF_LEVEL(PROF_RANGER, ch), GET_MAX_MOVE(ch));

	send_to_char("You manage to find some herbs which have given you energy."
		     "\n\r", ch);
	break;
      }

      /*
       * The affects of gather herbs now is based on the characters
       * ranger level. Its 25 - ranger level / 2, it was static at
       * 18, so i wanted to keep the values relatively (big relatively)
       * close to that mark.
       */

      ranger_bonus = number(GET_PROF_LEVEL(PROF_RANGER, ch) / 4,
			    GET_PROF_LEVEL(PROF_RANGER, ch) / 3);
      affects_last = 22 - ranger_bonus;
      if (subcmd > 2) {
	af.type      = SKILL_GATHER_FOOD;
	af.duration  = affects_last;
	af.modifier  = 0;
	af.location  = APPLY_NONE;
	af.bitvector = 0;
	affect_to_char(ch, &af);
      }
    }
    abort_delay(ch);
  } /*subcommand switch statement*/
}


ACMD(do_pick)
{
  byte percent;
  int	door, other_room;
  char	type[MAX_INPUT_LENGTH], dir[MAX_INPUT_LENGTH];
  struct room_direction_data *back;
  struct obj_data *obj;
  struct char_data *victim;


  if (IS_SHADOW(ch)) {
    send_to_char("You are too insubstantial to do that.\n\r", ch);
    return;
  }

  if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_ORC_FRIEND)) {
    send_to_char("Sorry, best leave this to your master.\n\r", ch);
    return;
  }

  argument_interpreter(argument, type, dir);
  percent = number(1, 101);

  if (!*type)
    send_to_char("Pick what?\n\r", ch);
  else if (generic_find(argument, FIND_OBJ_INV | FIND_OBJ_ROOM,
			ch, &victim, &obj))

    if (obj->obj_flags.type_flag != ITEM_CONTAINER)
      send_to_char("That's not a container.\n\r", ch);
    else if (!IS_SET(obj->obj_flags.value[1], CONT_CLOSED))
      send_to_char("Silly - it isn't even closed!\n\r", ch);
    else if (obj->obj_flags.value[2] < 0)
      send_to_char("Odd - you can't seem to find a keyhole.\n\r", ch);
    else if (!IS_SET(obj->obj_flags.value[1], CONT_LOCKED))
      send_to_char("Oho! This thing is NOT locked!\n\r", ch);
    else if (IS_SET(obj->obj_flags.value[1], CONT_PICKPROOF))
      send_to_char("It resists your attempts at picking it.\n\r", ch);
    else {
      if (percent > GET_SKILL(ch, SKILL_PICK_LOCK)) {
	send_to_char("You failed to pick the lock.\n\r", ch);
        act("$n fumbles with the lock.", TRUE, ch, 0, 0, TO_ROOM);
        return;
      }
      REMOVE_BIT(obj->obj_flags.value[1], CONT_LOCKED);
      send_to_char("*Click*\n\r", ch);
      act("$n fiddles with $p.", FALSE, ch, obj, 0, TO_ROOM);
    }

  else if ((door = find_door(ch, type, dir)) >= 0)
    if (!IS_SET(EXIT(ch, door)->exit_info, EX_ISDOOR))
      send_to_char("That's absurd.\n\r", ch);
    else if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED))
      send_to_char("You realize that the door is already open.\n\r", ch);
    else if (EXIT(ch, door)->key < 0)
      send_to_char("You can't seem to spot any lock to pick.\n\r", ch);
    else if (!IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED))
      send_to_char("Oh.. it wasn't locked at all.\n\r", ch);
    else if (IS_SET(EXIT(ch, door)->exit_info, EX_PICKPROOF))
      send_to_char("You seem to be unable to pick this lock.\n\r", ch);
    else {
      if (percent > GET_SKILL(ch, SKILL_PICK_LOCK)) {
	send_to_char("You failed to pick the lock.\n\r", ch);
	act("$n fumbles with the lock.", TRUE, ch, 0, 0, TO_ROOM);
	return;
      }
      REMOVE_BIT(EXIT(ch, door)->exit_info, EX_LOCKED);
      if (EXIT(ch, door)->keyword)
	act("$n skillfully picks the lock of the $F.", 0, ch, 0,
	    EXIT(ch, door)->keyword, TO_ROOM);
      else
	act("$n picks the lock of the door.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("The lock quickly yields to your skills.\n\r", ch);
      /*
       *This piece now unlocks the other side
       * of the door also
       */
      if ((other_room = EXIT(ch, door)->to_room) != NOWHERE)
	if ((back = world[other_room].dir_option[rev_dir[door]]) != NULL)
	  if (back->to_room == ch->in_room)
	    REMOVE_BIT(back->exit_info, EX_LOCKED);
    }
}



void stop_hiding (struct char_data *ch, char mode) {
  /*
   *if mode is FALSE, then we don't send the "step" message
   */
  if (IS_SET(ch->specials.affected_by, AFF_HIDE) && mode)
     send_to_char("You step out of your cover.\r\n", ch);

  REMOVE_BIT(ch->specials.affected_by, AFF_HIDE);
  REMOVE_BIT(ch->specials2.hide_flags, HIDING_SNUCK_IN);
  GET_HIDING(ch) = 0;
}


ACMD (do_hide) {
  sh_int hide_skill, hide_chance;
  char first_argument[260];

  one_argument(argument, first_argument);

  /*
   * We can't use an in-function `well' flag for this.  why?  because then
   * we charge the player 5* as many beats for hide, then return.  we're
   * then called again to perform the actual hide, but our well variable
   * has been lost.  making `well' static well also not work, because then
   * if two people hid on the same tick, we could confuse who hid well and
   * who did not
   */

  if (!strcmp(first_argument, "well"))
    SET_BIT(ch->specials2.hide_flags, HIDING_WELL);
  else {
    if (subcmd != 1)
      REMOVE_BIT(ch->specials2.hide_flags, HIDING_WELL);
  }
  if (IS_RIDING(ch)) {
    send_to_char("Hide while riding? Surely you jest.\n\r", ch);
    return;
  }

  if (ch->specials.fighting) {
    send_to_char("You can not hide from your opponent!\n\r", ch);
    return;
  }
  check_break_prep(ch);
  if (!subcmd) {
    if (GET_KNOWLEDGE(ch, SKILL_HIDE) <= 0)
      send_to_char("You cover your eyes with your hands and "
		   "hope you can't be seen.\n\r", ch);
    /*
     * The code below determines the beats taken to perform
     * a hide, based on whether or not people have snuck into
     * the room, are looking to hide or hide well. If you've snuck
     * into the room, hiding times will be slightly faster
     */
    else {
      if (IS_SET(ch->specials2.hide_flags, HIDING_SNUCK_IN))
	if (IS_SET(ch->specials2.hide_flags, HIDING_WELL))
	  /* Hiding well after sneaking in -> 7 beats*/
	  WAIT_STATE_BRIEF(ch, skills[SKILL_HIDE].beats *
			   (IS_SET(ch->specials2.hide_flags, HIDING_WELL) * 2) -1,
			   CMD_HIDE, 1, 30, AFF_WAITING | AFF_WAITWHEEL);
	else
	  /* Hiding after sneaking in -> 3 beats*/
	  WAIT_STATE_BRIEF(ch, skills[SKILL_HIDE].beats -1,
			   CMD_HIDE, 1, 30, AFF_WAITING | AFF_WAITWHEEL);
      else
	/*Hiding well with no sneak -> 9 beats, with four being default for normal hide*/
	WAIT_STATE_BRIEF(ch, skills[SKILL_HIDE].beats *
			 (IS_SET(ch->specials2.hide_flags, HIDING_WELL) *2 +1),
			 CMD_HIDE, 1, 30, AFF_WAITING | AFF_WAITWHEEL);

      if (IS_SET(ch->specials2.hide_flags, HIDING_WELL))
	send_to_char("You carefully choose a place to hide.\n\r", ch);
      else
	send_to_char("You quickly look for somewhere safe to hide.\n\r", ch);
    }
    return;
  }

  if (subcmd != 1) {
    send_to_char("You decide not to hide.\n\r", ch);
    return;
  }

  if (IS_AFFECTED(ch, AFF_HIDE))
    REMOVE_BIT(ch->specials.affected_by, AFF_HIDE);

  hide_skill = hide_prof(ch);
  if (IS_SET(ch->specials2.hide_flags, HIDING_WELL)) {
    send_to_char("You carefully hide yourself.\r\n", ch);
    hide_chance = number(hide_skill * 3 / 4, hide_skill);
    REMOVE_BIT(ch->specials2.hide_flags, HIDING_WELL);
  } else {
    send_to_char("You hide yourself.\r\n", ch);
    hide_chance = number(hide_skill / 2, hide_skill * 3 / 4);
  }
  GET_HIDING(ch) = hide_chance;

  if (hide_chance)
    SET_BIT(ch->specials.affected_by, AFF_HIDE);
}


ACMD (do_sneak) {
  if (IS_NPC(ch)) {
    send_to_char("Either you sneak or you don't.\r\n", ch);
    return;
  }

  if (!IS_AFFECTED(ch,AFF_SNEAK)) {
    send_to_char("Ok, you'll try to move silently.\r\n", ch);
    SET_BIT(ch->specials.affected_by, AFF_SNEAK);
  } else {
    send_to_char("You stop sneaking.\r\n", ch);
    REMOVE_BIT(ch->specials.affected_by, AFF_SNEAK);
  }
}

/*
 * Return a pointer to the intended victim of an ambush.  If
 * the intended victim is not a VALID victim, then we return
 * NULL.
 */
struct char_data *
ambush_get_valid_victim(struct char_data *ch, struct waiting_type *target)
{
	struct char_data *victim;

	if (target->targ1.type == TARGET_TEXT)
		victim = get_char_room_vis(ch, target->targ1.ptr.text->text);
	else if (target->targ1.type == TARGET_CHAR) {
		if (char_exists(target->targ1.ch_num))
			victim = target->targ1.ptr.ch;
	}

	if (victim == NULL) {
		send_to_char("Ambush who?\r\n", ch);
		return NULL;
	}

	/* Can happen for TARGET_CHAR */
	if (ch->in_room != victim->in_room) {
		send_to_char("Your victim is no longer here.\r\n", ch);
		return NULL;
	}

	if (victim->specials.fighting) {
		send_to_char("Your target is too alert!\n\r", ch);
		return NULL;
	}
	if (victim == ch) {
		send_to_char("How can you sneak up on yourself?\n\r", ch);
		return NULL;
	}
	if (!CAN_SEE(ch, victim)) {
		send_to_char("Ambush who?\r\n", ch);
		return NULL;
	}

	return victim;
}

/*
 * Decide whether an ambush succeeds or not.  If it does
 * succeed, then the return value is greater than 0.  Also,
 * the return value denotes how WELL the ambush succeeded.
 * Higher return values denote more successful ambushes.
 */
int
ambush_calculate_success(struct char_data *ch, struct char_data *victim)
{
	int percent;

	percent = number(-100, 0);
	percent += number(-20, 20);
	percent -= GET_LEVELA(victim);
	percent -= IS_NPC(victim) ? 0 : GET_SKILL(victim, SKILL_AWARENESS) / 2;
	percent += GET_PROF_LEVEL(PROF_RANGER, ch) + 15;
	percent += GET_SKILL(ch, SKILL_AMBUSH) + get_real_stealth(ch);
	if (GET_POSITION(victim) <= POSITION_RESTING)
		percent += 25 * (POSITION_FIGHTING - GET_POSITION(victim));
	percent -= GET_AMBUSHED(victim);
	percent -= GET_ENCUMB(ch);

	return percent;
}

/*
 * Calculate ambush damage.  Keep in mind that the modifier can
 * be anywhere from 0 to 200.  On average, for a 30r with stealth
 * against an opponent with no awareness, the modifier should be
 * on the order of 60.
 */
int
ambush_calculate_damage(struct char_data *ch, struct char_data *victim,
                        int modifier)
{
	int dmg;
        int weapon_bulk, weapon_dmg;

	if (modifier <= 0)
		return 0;

	if (ch->equipment[WIELD] != NULL) {
		weapon_bulk = ch->equipment[WIELD]->obj_flags.value[2];
		weapon_dmg = get_weapon_damage(ch->equipment[WIELD]);
	} else {
		weapon_bulk = 0;
		weapon_dmg = 0;
	}

	dmg = 120 + modifier;

	/* Scale by the amount of hp the victim has */
	dmg *= MIN(GET_HIT(victim), GET_PROF_LEVEL(PROF_RANGER, ch) * 20);

	/* Penalize for gear encumbrance and weapon encumbrance */
	dmg /= 400 + 5 * (GET_ENCUMB(ch) + GET_LEG_ENCUMB(ch) +
	        weapon_bulk*weapon_bulk);

	/* Add a small constant amount of damage */
	dmg += GET_PROF_LEVEL(PROF_RANGER, ch) - GET_LEVELA(victim) + 15;

	/* Add damage based on weapon */
	dmg += (weapon_dmg*weapon_dmg/100) * GET_LEVELA(ch)/30;

	/* Ambushes are capped at 275 damage */
	dmg = MIN(dmg, 275);

	return dmg;
}

ACMD(do_ambush)
{
  struct char_data *victim;
  int success;
  int dmg;

  if (IS_AFFECTED(ch, AFF_SANCTUARY)) {
    appear(ch);
    send_to_char("You cast off your sanctuary!\r\n", ch);
    act("$n renouces $s sanctuary!",FALSE, ch, 0, 0, TO_ROOM);
  }

  if (IS_SHADOW(ch)) {
    send_to_char("Hmm, perhaps you've spent too much time in"
		 " mortal lands.\r\n", ch);
    return;
  }

  if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_ORC_FRIEND)) {
    send_to_char("Leave that to your leader.\r\n", ch);
    return;
  }

  if (IS_SET(world[ch->in_room].room_flags, PEACEROOM)) {
    send_to_char("A peaceful feeling overwhelms you, and you cannot bring"
                 " yourself to attack.\n\r", ch);
    return;
  }

  if (!ch->equipment[WIELD]) {
    send_to_char("You must be wielding a weapon to ambush.\r\n", ch);
    return;
  }

  if (ch->equipment[WIELD]->obj_flags.value[2] > 2) {
    send_to_char("You need to wield a smaller weapon to surprise your victim"
      ".\r\n", ch);
    return;
  }

  if (!GET_SKILL(ch,SKILL_AMBUSH)) {
    send_to_char("Learning how to ambush would help!\r\n", ch);
    return;
  }

  /* Subcmd 0 is the first time ambush is called.  We require a target */
  if (subcmd == 0 && wtl == NULL) {
    send_to_char("Ambush who?\r\n", ch);
    return;
  }

  switch (subcmd) {
  case -1:
    abort_delay(ch);
    ch->delay.targ1.cleanup();
    ch->delay.targ2.cleanup();
    ch->delay.cmd = ch->delay.subcmd = 0;
    if(ch->specials.store_prog_number == 16)
      ch->specials.store_prog_number = 0;
    return;
  case 0:
    victim = ambush_get_valid_victim(ch, wtl);
    if (victim == NULL)
      return;

    /* TARGET_CHAR stores the victim, TARGET_TEXT stores the keyword */
    if (wtl->targ1.type == TARGET_CHAR)
      WAIT_STATE_FULL(ch, skills[SKILL_AMBUSH].beats, CMD_AMBUSH, 1, 30, 0,
                      GET_ABS_NUM(victim), victim,
                      AFF_WAITING | AFF_WAITWHEEL, TARGET_CHAR);
    else
      WAIT_STATE_FULL(ch, skills[SKILL_AMBUSH].beats, CMD_AMBUSH, 1, 30, 0,
                      0, get_from_txt_block_pool(wtl->targ1.ptr.text->text),
                      AFF_WAITING | AFF_WAITWHEEL, TARGET_TEXT);

    return;

  case 1:
    if (wtl == NULL) {
      vmudlog(BRF, "ERROR: ambush callback with no context");
      send_to_char("Error: ambush callback with no callback context.\r\n"
                   "Please report this to an immortal.\r\n", ch);
      return;
    }

    victim = ambush_get_valid_victim(ch, wtl);
    if (victim == NULL)
      return;

    success = ambush_calculate_success(ch, victim);
    if (success > 0) {
      /* Ambushed victims gain awareness and lose energy and parry */
      GET_ENERGY(victim) = GET_ENERGY(victim)/2 - success*3;

      SET_CURRENT_PARRY(victim) = 0;

      GET_AMBUSHED(victim) = GET_AMBUSHED(victim) * 3/4 + 30;
      if (IS_NPC(victim))
	GET_AMBUSHED(victim) += 20;
      if (IS_NPC(victim) && !IS_SET(ch->specials2.act, MOB_AGGRESSIVE))
	GET_AMBUSHED(victim) += 15;
    }

    dmg = ambush_calculate_damage(ch, victim, success);
    damage(ch, victim, dmg, SKILL_AMBUSH, 0);

    return;

  default:
    abort_delay(ch);
  }
}



/*
 * Check whether target->targ2.ptr.ch is matched by the
 * optional trap target keyword, which is stored in
 * target->targ1.ptr.text->text.
 */
struct char_data *
trap_get_valid_victim(struct char_data *ch, struct waiting_type *target)
{
	char *keyword;
	struct char_data *victim;

	if (target->targ1.type == TARGET_NONE)
		victim = target->targ2.ptr.ch;
	else {
		/*
		 * Two BIG distinctions: if the keyword begins with a
		 * digit, then it's 1.uruk or 2.bear, and thus the keyword
		 * is UNIQUE.  There is at most one 1.uruk and one 2.bear
		 * in any given room.  If the keyword does not begin with
		 * a digit, then the keyword may not be unique; i.e., if
		 * the keyword is "uruk" and there are 3 uruks in the room,
		 * then this could be any one of them.
		 */
		keyword = target->targ1.ptr.text->text;
		if (isdigit(*keyword))
			victim = get_char_room_vis(ch, keyword, 0);
		else if (keyword_matches_char(ch, target->targ2.ptr.ch, keyword))
			victim = target->targ2.ptr.ch;

		/* The victim didn't match the keyword */
		if (victim != target->targ2.ptr.ch)
			return NULL;
	}

	if (victim == NULL)
		return NULL;

	if (victim->specials.fighting) {
		send_to_char("Your target is too alert!\n\r", ch);
		return NULL;

	}
	if (victim == ch) {
		send_to_char("You attempt to trap yourself.\r\n", ch);
		return NULL;
	}

	return victim;
}

void
trap_cleanup_quiet(struct char_data *ch)
{
	abort_delay(ch);
	ch->delay.targ1.cleanup();
	ch->delay.targ2.cleanup();
	ch->delay.cmd = ch->delay.subcmd = 0;
	if (ch->specials.store_prog_number == 16)
		ch->specials.store_prog_number = 0;
}

bool can_do_trap(char_data& character)
{
	const int max_trap_weapon_bulk = 2;

	if (IS_SHADOW(&character))
	{
		send_to_char("Shadows can't trap!\n\r", &character);
		return false;
	}

	if (IS_NPC(&character) && MOB_FLAGGED(&character, MOB_ORC_FRIEND))
	{
		send_to_char("Leave that to your leader.\r\n", &character);
		return false;
	}

	if (!GET_SKILL(&character, SKILL_AMBUSH))
	{
		send_to_char("You must learn how to ambush to set an effective trap.\r\n", &character);
		return false;
	}

	const obj_data* weapon = character.equipment[WIELD];
	if (!weapon)
	{
		send_to_char("You cannot trap without equipping a weapon.\r\n", &character);
		return false;
	}

	int weapon_bulk = weapon->obj_flags.value[2];
	if (weapon_bulk > max_trap_weapon_bulk)
	{
		send_to_char("You must be using a lighter weapon to set a trap.\r\n", &character);
		return false;
	}

	return true;
}

bool is_valid_subcommand(char_data& character, int sub_command, const waiting_type* wtl)
{
	/* Sanity check ... All subcmds past 0 are callbacks and require a context */
	if (sub_command > 0 && wtl == NULL)
	{
		vmudlog(BRF, "do_trap: subcmd=%d, but the context is NULL!", sub_command);
		vsend_to_char(&character, "ERROR: trap subcommand is %d, but the context is null.\r\n"
			"Please report this message to an immortal.\r\n", sub_command);

		return false;
	}

	return true;
}

// drelidan:  Copied ACMD macro here so I could see the arguments.
//void do_trap(struct char_data *ch, char *argument, struct waiting_type * wtl, int cmd, int subcmd)
ACMD(do_trap)
{
	static int ignore_recursion = 0;
	struct char_data *victim;
	int dmg;
	int success;

	// Early out if some preconditions aren't met.
	if (!can_do_trap(*ch))
		return;

	// Perform a command sanity check.
	if (!is_valid_subcommand(*ch, subcmd, wtl))
		return;

	/*
	 * Subcommand callbacks:
	 *  -1   Cancel the current trap.  See the SUPER HACK note in case 2 to
	 *       see what the purpose of ignore_recursion is.
	 *   0   Command issued by a player or mob.  Has either a text keyword
	 *       as an argument or has no argument.
	 *   1   Callback: trap setup complete.  We cheat a little and store
	 *       important target data on the character's waiting structure.
	 *       This may cause problems if some other wait activity happens,
	 *       because then the target data for our trap will be cleared.
	 *   2   do_trap was called from react_trap in spec_pro.cc.  This means
	 *       that someone has entered the room with ch.  If the person who
	 *       entered matches ch's target data, then ch will attempt to trap
	 *       them.  We delay ch's trap for 1 game tick before letting it go
	 *       off.
	 *   3   The trap is actually occurring.  Damage and bash the victim if
	 *       the trap is successful.  Damage and success percent are heavily
	 *       based on ambush success and damage.
	 */
	switch (subcmd)
	{
	case -1:
		/* XXX: SUPER HACK */
		if (ignore_recursion)
		{
			ignore_recursion = 0;
			return;
		}
		send_to_char("You abandoned your trap.\r\n", ch);
		trap_cleanup_quiet(ch);
		break;

	case 0:
		send_to_char("You begin setting up your trap...\r\n", ch);

		/* If there's a target keyword, then store it */
		if (wtl->targ1.type == TARGET_TEXT)
		{
			WAIT_STATE_FULL(ch, skills[SKILL_AMBUSH].beats * 2, CMD_TRAP, 1, 30, 0,
				0, get_from_txt_block_pool(wtl->targ1.ptr.text->text),
				AFF_WAITING | AFF_WAITWHEEL, TARGET_TEXT);
		}
		else
		{
			WAIT_STATE_FULL(ch, skills[SKILL_AMBUSH].beats * 2, CMD_TRAP, 1, 30, 0,
				0, NULL,
				AFF_WAITING | AFF_WAITWHEEL, TARGET_NONE);
		}
		break;

	case 1:
		send_to_char("You begin to wait patiently for your victim.\r\n", ch);

		/* Use the wait state to store the target data */
		if (wtl->targ1.type == TARGET_TEXT)
		{
			WAIT_STATE_FULL(ch, -1, CMD_TRAP, 2, 30, 0,
				0, get_from_txt_block_pool(wtl->targ1.ptr.text->text),
				0, TARGET_TEXT);
		}
		else
		{
			WAIT_STATE_FULL(ch, -1, CMD_TRAP, 2, 30, 0,
				0, NULL,
				0, TARGET_NONE);
		}

		/* We use spec_prog 16 for people with trap */
		ch->specials.store_prog_number = 16;
		return;

	case 2:
		victim = trap_get_valid_victim(ch, wtl);
		if (victim == NULL)
			return;

		ch->specials.store_prog_number = 0;

		/*
		 * XXX: SUPER HACK.  If we are here, then case 1 has happened already.
		 * In case 1, we store trap's target information in the character's
		 * delay variable.  However, when we call WAIT_STATE_FULL, it will call
		 * complete_delay, which will then call do_trap with subcmd -1 to clear
		 * the data we stored there.  Once those targets are deleted, we're
		 * screwed.
		 *
		 * So what we have here is a static variable that we set to 1 when we
		 * know this screwball scenario is going to happen.  When ignore_recursion
		 * is 1, then case -1 above reacts accordingly and does not clear the
		 * target data.
		 */
		ignore_recursion = 1;
		if (wtl->targ1.type == TARGET_TEXT)
		{
			WAIT_STATE_FULL(ch, 1, CMD_TRAP, 3, 40, 0,
				0, get_from_txt_block_pool(wtl->targ1.ptr.text->text),
				AFF_WAITING, TARGET_TEXT);
		}
		else
		{
			WAIT_STATE_FULL(ch, 1, CMD_TRAP, 3, 40, 0,
				0, NULL,
				AFF_WAITING, TARGET_NONE);
		}

		/* WAIT_STATE_FULL clobbers targ2 unconditionally, so we refill it */
		ch->delay.targ2 = wtl->targ2;
		break;

	case 3:
		victim = trap_get_valid_victim(ch, wtl);
		if (victim == NULL)
		{
			/* Reset the trap.  do_trap subcmd=1 does exactly this. */
			do_trap(ch, "", wtl, CMD_TRAP, 1);
			return;
		}

		trap_cleanup_quiet(ch);  /* Removes the spec prog */
		success = ambush_calculate_success(ch, victim);

		if (success < 0)
		{
			damage(ch, victim, 0, SKILL_TRAP, 0);
		}
		else
		{
			dmg = ambush_calculate_damage(ch, victim, success);
			dmg = dmg >> 1; // Cut the damage in half?  Easier ways to do this.  dmg = dmg >> 1;

			// Set a bash affection on the victim
			WAIT_STATE_FULL(victim, 5,
					CMD_BASH, 2, 80, 0, 0, 0, AFF_WAITING | AFF_BASH,
					TARGET_IGNORE);

			damage(ch, victim, dmg, SKILL_TRAP, 0);
		}
		break;

	default:
	{
		abort_delay(ch);
		break;
	}
	}
}


ACMD (do_calm) {
  int calm_skill;
  struct char_data *victim;
  struct waiting_type tmpwtl;
  struct affected_type af;

  if (IS_SHADOW(ch)) {
    send_to_char("You are too airy for that.\r\n", ch);
    return;
  }

  calm_skill = GET_SKILL(ch, SKILL_CALM) +
    GET_RAW_SKILL(ch, SKILL_ANIMALS) / 2;

  if (GET_SPEC(ch) == PLRSPEC_PETS)
    calm_skill += 30;
  /*
   * Set up the victim pointer
   * Case -1 Cancelled a calm in progess
   * Case 0 Entered "Calm Target"
   * Case 1 Finished Calm delay
   * Default should never happen
   */
  switch(subcmd) {
  case -1:
    abort_delay(ch);
    return;
  case 0:
    one_argument(argument, arg);
    if (!(victim = get_char_room_vis(ch, arg))) {
      send_to_char("Calm who?\r\n", ch);
      return;
    }
    break;
  case 1:
    if (wtl->targ1.type != TARGET_CHAR ||
	!char_exists(wtl->targ1.ch_num)) {
      send_to_char("Your victim is no longer among us.\r\n", ch);
      return;
    }
    victim = (struct char_data *) wtl->targ1.ptr.ch;
    break;
  default:
    sprintf(buf2, "do_calm: illegal subcommand '%d'.\r\n",
	    subcmd);
    mudlog(buf2, NRM, LEVEL_IMMORT, TRUE);
    return;
  }

  /* Validate victim */
  if (!CAN_SEE(ch, victim)) {
    send_to_char("Calm who?\r\n", ch);
    return;
  } else if (victim == ch) {
    send_to_char("You try to calm yourself.\r\n", ch);
    return;
  } else if (!IS_SET(MOB_FLAGS(victim), MOB_AGGRESSIVE)) {
    send_to_char("Your target is already calm.\r\n", ch);
    return;
  } else if (!IS_NPC(victim) ||
	    (GET_BODYTYPE(victim) != BODYTYPE_ANIMAL)) {
    send_to_char("You can only calm animals.\r\n", ch);
    return;
  } else if (GET_POS(victim) == POSITION_FIGHTING) {
    sprintf(buf, "%s is too enraged!\r\n", GET_NAME(victim));
    send_to_char(buf, ch);
    return;
  } else if (GET_POS(victim) < POSITION_FIGHTING) {
    send_to_char("Your target needs to be standing.\r\n", ch);

  }
  switch(subcmd) {
  case 0:
    if (!GET_SKILL(ch, SKILL_CALM)) {
      send_to_char("Learn how to calm properly first!\n\r",
		   ch);
      return;
    }
    /* #define is here for readability only. */

#define CALM_WAIT_BEATS                                  \
    skills[SKILL_CALM].beats * 2 * GET_LEVEL(victim) /   \
    (GET_PROF_LEVEL(PROF_RANGER, ch) + calm_skill / 15)

    WAIT_STATE_FULL(ch, CALM_WAIT_BEATS, CMD_CALM, 1, 50,
		    0, GET_ABS_NUM(victim), victim,
		    AFF_WAITING | AFF_WAITWHEEL,
		    TARGET_CHAR);
#undef CALM_WAIT_BEATS

    break;
  case 1:
    if (ch->in_room != victim->in_room) {
      send_to_char("You target is not here any longer.\r\n",
		   ch);
      return;
    }
    if (calm_skill > number(0, 150)) {  /* success */
      if (!affected_by_spell(victim, SKILL_CALM)) {
	act("$n seems calmed.", FALSE, victim, 0, 0, TO_ROOM);
	af.type = SKILL_CALM;
	af.duration = -1;
	af.modifier = 0;
	af.location = APPLY_NONE;
	af.bitvector = 0;
	affect_to_char(victim, &af);
      }
      REMOVE_BIT(MOB_FLAGS(victim), MOB_AGGRESSIVE);
    } else {
      send_to_char("You fail to calm your target.\r\n", ch);
      sscanf(ch->player.name, "%s", buf);
      tmpwtl.targ1.type = TARGET_CHAR;
      tmpwtl.targ1.ptr.ch = ch;
      tmpwtl.targ1.ch_num = ch->abs_number;
      tmpwtl.cmd = CMD_HIT;
      do_hit(victim, buf, &tmpwtl, 0, 0);
    }
    break;

  default:  /* Shouldn't ever happen */
    sprintf(buf2, "do_calm: illegal subcommand '%d'.\r\n",
	    subcmd);
    mudlog(buf2, NRM, LEVEL_IMMORT, TRUE);
    abort_delay(ch);
    return;
  }
}



ACMD (do_tame) {
  int tame_skill, levels_over_required;
  struct char_data *victim = NULL;
  struct waiting_type tmpwtl;
  struct affected_type af;

  if (IS_SHADOW(ch)) {
    send_to_char("You are too insubstantial to do that.\r\n", ch);
    return;
  }

  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("Your superior would not approve of your building"
		 " an army.\n\r", ch);
    return;
  }
  tame_skill = GET_SKILL(ch, SKILL_TAME) + GET_RAW_SKILL(ch, SKILL_ANIMALS) / 2;
  one_argument(argument, arg);
  if (GET_MOVE(ch) < 60) {
    send_to_char("You are too exhausted.\r\n", ch);
    return;
  }

  if (subcmd == -1) {
    abort_delay(ch);
    return;
  }

  if (!subcmd)
    if (!(victim = get_char_room_vis(ch, arg))) {
      send_to_char("Tame who?\r\n", ch);
      return;
    }

  if (subcmd == 1) {
    if (wtl->targ1.type != TARGET_CHAR || !char_exists(wtl->targ1.ch_num)) {
      send_to_char("Your victim is no longer among us.\r\n", ch);
      return;
    }
	victim = (struct char_data *) wtl->targ1.ptr.ch;
	}

    levels_over_required = (GET_PROF_LEVEL(PROF_RANGER, ch) / 3 + tame_skill /
			    30 - GET_LEVEL(victim) - get_followers_level(ch));

    if (affected_by_spell(victim, SKILL_CALM))
      levels_over_required += 1;

    if (GET_SPEC(ch) == PLRSPEC_PETS)
      levels_over_required += 1;

    if (IS_SHADOW(ch)) {
      send_to_char("You are too airy for that.\r\n", ch);
      return;
    }

    one_argument(argument, arg);

    if (GET_PERCEPTION(victim) || !IS_NPC(victim) ||
	GET_BODYTYPE(victim) != BODYTYPE_ANIMAL) {
      send_to_char("You can only tame animals.\r\n", ch);
      return;
    }
    /*
     * change the above to check for animal flag
     * change 'your target' to the appropriate message (animal's name)
     */
    if (GET_POS(victim) == POSITION_FIGHTING) {
      send_to_char("Your target is too enraged!\r\n", ch);
      return;
    }

    if (GET_POS(victim) < POSITION_FIGHTING) {
      send_to_char("Your target needs to be standing.\r\n", ch);
      return;
    }

    if (victim->master) {
      send_to_char("Your target is already following someone.\r\n", ch);
      return;
    }

    if (levels_over_required < 0) {
      send_to_char("Your target is too powerful for you to tame.\r\n", ch);
      return;
    }

    switch (subcmd) {
    case 0:
      if (victim == ch) {
	send_to_char("Aren't we funny today...\r\n", ch);
	return;
      }

      if (!GET_SKILL(ch, SKILL_TAME)) {
	send_to_char("Learn how to tame properly first!\r\n ", ch);
	return;
      }

      WAIT_STATE_FULL(ch, skills[SKILL_TAME].beats * GET_LEVEL(victim) * 2 /
		      (GET_PROF_LEVEL(PROF_RANGER,ch)+ 1), CMD_TAME, 1 , 50, 0,
		      GET_ABS_NUM(victim), victim, AFF_WAITING | AFF_WAITWHEEL,
		      TARGET_CHAR);
      return;


    case 1:
      if (GET_POS(ch) == POSITION_FIGHTING) {
	send_to_char("You are too busy to do that.\r\n", ch);
	return;
      }

      if (ch->in_room != victim->in_room) {
	send_to_char("You target is not here any longer.\r\n", ch);
	return;
      }

      if (tame_skill * (levels_over_required + 1) / 5 >
	  number(0, 100)) {
	if (circle_follow(victim, ch, FOLLOW_MOVE)) {
	  send_to_char("Sorry, following in circles is not allowed.\r\n", ch);
	  return;
	}

	if (victim->master)
	  stop_follower(victim, FOLLOW_MOVE);
	affect_from_char(victim, SKILL_TAME);
	add_follower(victim, ch, FOLLOW_MOVE);

	act("$n seems tamed.",FALSE, victim, 0, 0, TO_ROOM);
	af.type      = SKILL_TAME;
	af.duration  = -1;
	af.modifier  = 0;
	af.location  = APPLY_NONE;
	af.bitvector = AFF_CHARM;

	affect_to_char(victim, &af);

	REMOVE_BIT(MOB_FLAGS(victim), MOB_SPEC);
	victim->specials.store_prog_number = 0;
	REMOVE_BIT(MOB_FLAGS(victim), MOB_AGGRESSIVE);
	REMOVE_BIT(MOB_FLAGS(victim), MOB_STAY_ZONE);
	SET_BIT(MOB_FLAGS(victim), MOB_PET);

	victim->specials2.pref = 0;
	/*
	 * Removal of mob aggressions
	 * Addition of move bonus
	 */
	GET_MOVE(ch) -= 60;
	GET_MOVE(victim) = GET_MAX_MOVE(victim) += 50;
      } else {
	send_to_char("You fail to tame your target.\r\n", ch);
	sscanf(ch->player.name,"%s", buf);
	tmpwtl.targ1.type = TARGET_CHAR;
	tmpwtl.targ1.ptr.ch = ch;
	tmpwtl.targ1.ch_num = ch->abs_number;
	tmpwtl.cmd = CMD_HIT;
	do_hit(victim, buf, &tmpwtl, 0, 0);
      }

    default:
      abort_delay(ch);
      return;
    }
}


ACMD (do_whistle) {
  int rm_num, zone_num, cur_room_num;
  struct room_data *rm;
  struct char_data *tmpch;

  cur_room_num = ch->in_room;
  zone_num = world[cur_room_num].zone;

  if (IS_SHADOW(ch)) {
    send_to_char("You need to breathe to whistle!\r\n", ch);
    return;
  }

  if (!subcmd) {  /* setting a delay */
    send_to_char("You gather your breath.\r\n", ch);
    WAIT_STATE_FULL(ch, skills[SKILL_WHISTLE].beats, CMD_WHISTLE, 1 , 50, 0,
		    0, 0, AFF_WAITING | AFF_WAITWHEEL, TARGET_NONE);
    return;
  } else if (subcmd == 1) {
    if ((GET_SKILL(ch, SKILL_WHISTLE) + (GET_RACE(ch) == 13 ? 99 : 0))
	< number(0, 99)) {
      send_to_char("You whistle, but can barely hear yourself.\r\n", ch);
      act("$n whistles softly.", FALSE, ch, 0, 0, TO_ROOM);

      return;
    }
    send_to_char("You whistle powerfully.\r\n", ch);
    for (rm_num = 0; rm_num <= top_of_world; rm_num++) {
      rm = &world[rm_num];

      if (rm->zone != zone_num)
	continue;

      ch->in_room = rm_num;

      if (rm_num == cur_room_num)
	act("$n whistles powerfully.", FALSE, ch, 0, 0, TO_ROOM);
      else
	act("You hear a powerful whistle nearby.",FALSE, ch, 0, 0, TO_ROOM);

      for (tmpch = rm->people; tmpch; tmpch = tmpch->next_in_room) {
	if (MOB_FLAGGED(tmpch, MOB_PET) && (tmpch->master == ch)) {
	  /*
	   * this is the guy we need to come to ch
	   * Make him stand up
	   */
	  GET_POS(tmpch) = POSITION_STANDING;
	  update_pos(tmpch);
	  affected_type newaf;
	  newaf.type = SPELL_ACTIVITY;
	  newaf.duration = 5;
	  newaf.modifier = 1;
	  newaf.location = APPLY_SPEED;
	  newaf.bitvector = AFF_HUNT;
	  affect_to_char(tmpch, &newaf);
	  forget(tmpch, ch);
	  remember(tmpch, ch);
	}
      }
    }
    ch->in_room = cur_room_num;
  }
}

ACMD (do_stalk) {
  int dir;

  if (IS_SHADOW(ch)) {
    send_to_char("You don't leave tracks anyway!.\n\r", ch);
    return;
  }

  if(!wtl || wtl->targ1.type != TARGET_DIR) {
    send_to_char("Improper command target - please notify an immortal.\n\r",
		 ch);
    return;
  }

  dir = wtl->targ1.ch_num;

  if(special(ch, dir + 1, "", SPECIAL_COMMAND, 0))
    return;


  if(GET_KNOWLEDGE(ch, SKILL_STALK) <= 0) {
    do_move(ch, argument, wtl, dir+1, 0);
    return;
  }

  if(!CAN_GO(ch, dir)) {
    send_to_char("You cannot go that way.\n\r", ch);
    return;
  }

  if(IS_RIDING(ch)) {
    send_to_char("You cannot control tracks of your mount.\n\r", ch);
    return;
  }

  if(world[ch->in_room].sector_type == SECT_WATER_NOSWIM) {
    send_to_char("Water keeps no tracks.\n\r", ch);
    return;
  }

  WAIT_STATE_BRIEF(ch, skills[SKILL_STALK].beats, dir + 1, SCMD_STALK,
		   30, AFF_WAITING | AFF_WAITWHEEL);
  ch->delay.targ1.type = TARGET_DIR;
  ch->delay.targ1.ch_num = dir;

  act("$n looks carefully at the ground.", TRUE, ch, 0, 0, TO_ROOM);
  sprintf(buf, "You look for a discreet way %s.\n\r", dirs[dir]);
  send_to_char(buf, ch);
  return;
}



ACMD (do_cover) {
  room_data * tmproom;
  int tmp, dir, dt, tr_time;

  if (IS_SHADOW(ch)) {
    send_to_char("You are too insubstantial to do that.\r\n", ch);
    return;
  }

  if (subcmd < 0) {
    send_to_char("You abandoned your track covering.\r\n", ch);
    return;
  }

  if (subcmd == 0) {

    if (GET_KNOWLEDGE(ch, SKILL_STALK) <= 0) {
      send_to_char("You fumble around, trying to tidy up the place.\r\n", ch);
    } else {
      WAIT_STATE_BRIEF(ch, skills[SKILL_STALK].beats, CMD_COVER, 1, 30, AFF_WAITING | AFF_WAITWHEEL);
      send_to_char("You start covering up the tracks.\r\n", ch);
    }
    act("$n moves around, making small adjustments.", TRUE, ch, 0, 0, TO_ROOM);
    return;
  }

  if (subcmd != 1) {
    send_to_char("Wrong subcmd in the track covering. Aborting.\r\n", ch);
    return;
  }

  tmproom = &world[ch->in_room];

  for (tmp = 0; tmp < NUM_OF_TRACKS; tmp++) {
    if (tmproom->room_track[tmp].char_number != 0 &&
	GET_KNOWLEDGE(ch, SKILL_STALK) * 2 > number(1,100)) {
      dt = number(1, 20);
      tr_time = tmproom->room_track[tmp].condition;
      dir = tmproom->room_track[tmp].data & 7;
      if (dt + tr_time >= 24) {
	/* Tracks being removed in this if*/
	tmproom->room_track[tmp].char_number = 0;
	tmproom->room_track[tmp].data = 0;
        tmproom->room_track[tmp].condition = 0;
      } else {
        tmproom->room_track[tmp].condition += dt;

      }
    }
  }

  send_to_char("You sweep over the room, getting rid of the tracks.\r\n", ch);
}


ACMD (do_hunt) {
  if (IS_SHADOW(ch)) {
    send_to_char("You can't notice details like that.\r\n", ch);
    return;
  }

  if (!IS_AFFECTED(ch,AFF_HUNT)) {
    send_to_char("Ok, you'll try to notice fresh tracks.\r\n", ch);
    SET_BIT(ch->specials.affected_by, AFF_HUNT);
  } else {
    send_to_char("You stop looking for fresh tracks.\r\n", ch);
    REMOVE_BIT(ch->specials.affected_by, AFF_HUNT);
  }
}



/*
 * Used when a character successfully sneaks into a room
 * without anyone noticing.  As the name implies, `snuck_in'
 * assumes that `ch' has snuck into his new room successfully;
 * this, of course, assumes that `ch' is at least sneaking!
 * Note also that the small hide value awarded by snuck_in is
 * a normal hide value, and will vanish should the character
 * perform any revealing action; because of the HIDE_SNUCK_IN
 * bit, stop_hiding needs to unset this bit in addition to
 * removing the GET_HIDING level.  snuck_in also facilitates
 * the small delay associated with using sneak.
 * The purpose is to:
 *   - notify the player that he moved without catching attention,
 *     assuming that there is at least one visible character visible
 *     to `ch' in the room he is entering.
 *   - cause a regular hide (this is so that sneak can actually be
 *     used vs. mobs.)
 *   - set the SNUCK_IN bit in specials2, so that get_real_stealth
 *     calls can determine whether or not we should bonus this
 *     character for his sneaking
 */
void
snuck_in(struct char_data *ch)
{
  int wait;
  struct char_data *t;

  GET_HIDING(ch) = number(hide_prof(ch) / 3, hide_prof(ch) * 4 / 5);
  if(GET_HIDING(ch) > 0)
    SET_BIT(ch->specials.affected_by, AFF_HIDE);
  SET_BIT(ch->specials2.hide_flags, HIDING_SNUCK_IN);

  for(t = world[ch->in_room].people; t; t = t->next_in_room)
    if(t != ch && CAN_SEE(ch, t))
      break;
  if(t)
    send_to_char("You managed to enter without anyone noticing.\r\n", ch);

  /*
   * If you're hunting, we don't want sneak to make your hunt delay
   * non-existent, so we add the hunt delay to the sneak delay if
   * you're hunting.
   *
   * For those confused as to why we use the variable `wait': since
   * macro expansion simply replaces the text in the macro with the
   * text we send it as arguments, sending 'ch->delay.wait_value + 2'
   * doesn't actually work.  WAIT_STATE is called, it calls
   * WAIT_STATE_FULL, which will call abort_delay (in the case of
   * hunt), which will set ch->delay.wait_value to 0.  Your wait time
   * will still be using the simple ch->delay.wait_value + 2 syntax
   * when it actually performs the wait_value assignment in
   * WAIT_STATE_FULL, but the value in ch->delay.wait_value will
   * already be 0.
   *
   * Another worthy thing to note is that the WAIT_STATE macros seem
   * to go through alot of angst to circumvent the buzz-worthy 'double
   * delays'; a double delay is just what is happening here: the hunt
   * delay is still active, and we're throwing another on top of it.
   * I don't really see what the problem with this is, assuming both
   * delays were set with WAIT_STATE (and they are, for this case).
   * Notice that if anyone ever mudlles any sort of delay, sneak won't
   * destroy the delay's value, but it'll destroy its other info.. not
   * quite sure what we should do about this.
   */
  wait = ch->delay.wait_value + 2;
  if (GET_PROF_LEVEL(PROF_RANGER, ch) > number(0, 60))
    wait = wait - 1;

  WAIT_STATE(ch, wait);
}



/*
 * snuck_out checks to see how many characters in the room (with
 * the exception of `ch' itself) `ch' is leaving are visible to
 * `ch'.  If this number is non-zero, then we send `ch' a message
 * confirming a successful sneak.  Do note that snuck_in does
 * assume that the sneak was performed successfully.
 */
void
snuck_out(struct char_data *ch)
{
  struct char_data *t;

  for(t = world[ch->in_room].people; t; t = t->next_in_room)
    if(t != ch && CAN_SEE(ch, t))
      break;
  if(t)
    send_to_char("You seem to have left without raising any alarm.\r\n", ch);
}




/*
 * hide_prof calculates the level of GET_HIDING that `hider'
 * should be able to hide himsef at, using mainly the number
 * generated by get_real_stealth and `hider's hide skill.
 * See get_real_stealth to see what's involved in this.
 *
 * Numbers for non-30 ranger below are absolete
 * hide_chance for a level 30 ranger with 100% hide and 100%
 * stealth in the an average stealth-bonus area operates in the
 * range of [75, 125].  A level 36 ranger hobbit with 100% hide/
 * stealth in the highest stealth area pulls a hide_chance in the
 * range of [135, 180].  In all generality, it's pretty safe to
 * target the [110, 140] range as the "legend hide" range.
 *
 * Note: the low end of the ranges above assume the hider is
 * performing a normal hide; the high end assumes the hider is
 * performing a hide well.
 */
int
hide_prof(struct char_data *hider)
{
  int hide_value;
  int get_real_stealth(struct char_data *);

  hide_value = GET_SKILL(hider, SKILL_HIDE) + get_real_stealth(hider) +
    GET_PROF_LEVEL(PROF_RANGER, hider) - 30;

  return hide_value;
}



/*
 * see_hiding calculates the level of GET_HIDING that
 * `seeker' should be able to see, taking into account:
 *  - the ranger level of `seeker'
 *  - the amount of awareness `seeker' has practiced
 *  - the intelligence of `seeker'
 *  - the specialization of `seeker' (stealth spec gets a bonus)
 *  - the race of `seeker' (elves get a bonus)
 */
int
see_hiding(struct char_data *seeker)
{
  int can_see, awareness;

  if(IS_NPC(seeker))
    awareness = std::min(100, 40 + GET_INT(seeker) + GET_LEVEL(seeker));
  else
    awareness = GET_SKILL(seeker, SKILL_AWARENESS) + GET_INT(seeker);

  can_see = awareness * GET_PROF_LEVEL(PROF_RANGER, seeker) / 30;

  if(GET_SPEC(seeker) == PLRSPEC_STLH)
    can_see += 5;

  if(GET_RACE(seeker) == RACE_WOOD)
    can_see += 5;

  return can_see;
}

/*
 * Assuming 'archer' is shooting 'victim', does 'archer' perform
 * an 'accurate' hit (via the accuracy skill)?
 *
 * This is similar to the accuracy check in 'fight.cc', but it does not
 * take tactics into consideration, since they are not used for archery.
 *
 * Returns: true if the hit should not be accurate, false if the
 * hit should be accurate.
 */
bool check_archery_accuracy(const char_data& archer, const char_data& victim)
{
	using namespace utils;

	double probability = get_prof_level(PROF_RANGER, archer) * 0.01; // 30% chance at 30r
	probability -= get_skill_penalty(archer) * 0.0001; // minus any skill penalty
	probability -= get_dodge_penalty(archer) * 0.0001; // minus any dodge penalty
	probability *= get_skill(archer, SKILL_ACCURACY) * 0.01; // scaled by skill - 100% gives us the above
	
	double roll = number();

	return roll < probability;
}

/*
 * shoot_calculate_success determines the success rate of the
 * archer versus the victim, taking into account:
 * - the ranger level of the attacker
 * - the amount of archery practiced by attacker
 * - the ob of the attacker
 * - the dex of the attacker
 *
 * NOTE: This should probably be changed after the first test of attacking.
 * XXX The skill accuracy should be taken into mind here
 * --------------------------- Change Log --------------------------------
 * slyon: Jan 24,2017 - Created function
 * drelidan: Jan 31, 2017 - Base implementation as an example.
 * slyon: Feb 3, 2017 - Added arrow tohit into the equation
 */

int shoot_calculate_success(const char_data* archer, const char_data* victim, const obj_data* arrow)
{
	using namespace utils;

	int archery_skill = get_skill(*archer, SKILL_ARCHERY);
	int accuracy_skill = get_skill(*archer, SKILL_ACCURACY);
	//This obj_flag is defined in act_info.cc
	int arrow_tohit = arrow->obj_flags.value[0];

	int ranger_level = get_prof_level(PROF_RANGER, *archer);
	int ranger_dex = archer->get_cur_dex();

	// Calculate success is currently not taking 'OB' into account.  This is intentional.
	// This can return over 100 currently.  Check out scaling for different factors to
	// see how we want to adjust this.

	// TODO(drelidan):  When 'shooting modes' are implemented, give a penalty
	// here for shooting quickly and a bonus for shooting slowly.
	int success_chance = ranger_level + (ranger_dex / 2) + (archery_skill / 3) + (accuracy_skill / 3) + arrow_tohit;

	return success_chance;
}

/*
 * Returns the part of the body on the victim that is getting hit.
 *
 **/
int get_hit_location(const char_data& victim)
{
	int hit_location = 0;

	int body_type = victim.player.bodytype;

	const race_bodypart_data& body_data = bodyparts[body_type];
	if (body_data.bodyparts != 0)
	{
		int roll = number(1, 100);
		while (roll > 0 && hit_location < MAX_BODYPARTS)
		{
			roll -= body_data.percent[hit_location++];
		}
	}

	if (hit_location > 0)
		--hit_location;

	return hit_location;
}

/*
* Given a hit location and a damage, calculate how much damage
* should be done after the victim's armor is factored in.  The
* modified amount is returned.
*
* This used to be (tmp >= 0); this implied that torches
* could parry. - Tuh
*/
int apply_armor_to_arrow_damage(char_data& archer, char_data& victim, int damage, int location)
{
	/* Bogus hit location */
	if (location < 0 || location > MAX_WEAR)
		return 0;

	/* If they've got armor, let's let it do its thing */
	obj_data* armor = victim.equipment[location];
	if (armor)
	{
		// The target has armor, but we made an accurate shot.
		if (check_archery_accuracy(archer, victim))
		{
			act("You manage to find a weakness in $N's armor!", TRUE, &archer, NULL, &victim, TO_CHAR);
			act("$n manages to find a weakness in $N's armor!", TRUE, &archer, NULL, &victim, TO_NOTVICT);
			act("$n notices a weakness in your armor!", TRUE, &archer, NULL, &victim, TO_VICT);
			return damage;
		}

		const obj_flag_data& obj_flags = armor->obj_flags;

		/* First, remove minimum absorb */
		int damage_reduction = armor->get_base_damage_reduction();
		
		/* Then apply the armor_absorb factor */
		damage_reduction += (damage * armor_absorb(armor) + 50) / 100;

		if (obj_flags.is_leather() || obj_flags.is_cloth())
		{
			damage_reduction = 0;
		}
		else if (obj_flags.is_chain())
		{
			// Chain is half-effective against shooting.
			damage_reduction = damage_reduction / 2;
		}

		// Reduce damage here, but not below 1.
		damage -= damage_reduction;
		damage = std::max(damage, 1);
	}

	return damage;
}


//============================================================================
// Returns a multiplier to archery damage based on ranger level.
//============================================================================ 
double get_ranger_level_multiplier(int ranger_level)
{
	const double base_multiplier = 0.8;

	if (ranger_level <= 20)
		return base_multiplier;
	
	// ranger level > 20
	int num_steps = ranger_level - 20;
	return base_multiplier + num_steps * 0.02; // ranger level 20 is 80% damage, 25 is 90% damage, 30 is base damage
}

/*
 * shoot_calculate_damage
 *
 * Damage should be based on the archer's ranger level, dexterity modifier,
 *  the arrows that they are using, and the bow that they are using.
 *
 * If the archer performs an accurate shot, the target's armor is ignored.
 * --------------------------- Change Log --------------------------------
 * slyon: Jan 24, 2017 - Created function
 * drelidan: Jan 31, 2017 - First pass implementation.  Doesn't take weapon or
 *   arrow type into account.
 * slyon: Feb 3, 2017 - Added arrowto_dam into the equation
 * drelidan: Feburary 7, 2017 - Add weapon damage into the equation.
 */
int shoot_calculate_damage(char_data* archer, char_data* victim, const obj_data* arrow, int& hit_location)
{
	using namespace utils;

	int ranger_level = get_prof_level(PROF_RANGER, *archer);
	double ranger_level_factor = (ranger_level * 0.5) * number_d(0.5, 1.0);
	double strength_factor = (archer->get_cur_str() - 10) * 0.75;
	
	int arrow_todam = arrow->obj_flags.value[1];

	obj_data* bow = archer->equipment[WIELD];
	double weapon_damage = get_weapon_damage(bow);
	double random_cap = arrow_todam + weapon_damage; // should be between ~4 and 30 at the ABSOLUTE max
	
	double random_factor_1 = number(random_cap);
	double random_factor_2 = number(random_cap);
	double random_factor_3 = number(random_cap);

	double bow_factor = random_factor_1 + random_factor_2 + random_factor_3 + strength_factor;

	double multipler = get_ranger_level_multiplier(ranger_level);
	int damage = int(ranger_level_factor + (bow_factor * multipler));

	int arrow_hit_location = get_hit_location(*victim);

	int body_type = victim->player.bodytype;
	const race_bodypart_data& body_data = bodyparts[body_type];

	// Apply damage reduction.
	int armor_location = body_data.armor_location[arrow_hit_location];
	damage = apply_armor_to_arrow_damage(*archer, *victim, damage, armor_location);


	hit_location = arrow_hit_location;
	return damage;
}

/*
 * shoot_calculate_wait will determine how long the shoot will be affected
 * by WAIT_STATE_FULL and it will take into account the following:
 * -- WEAPON SPEED
 * -- SKILL_ARCHERY
 * -- RANGER_LEVEL
 * -- RACE;
 * --------------------------- Change Log --------------------------------
 * slyon: Jan 24, 2017 - Created function
 */
int shoot_calculate_wait(const char_data* archer)
{
	const int base_beats = 16;
	const int beats_modifier = 12;
	const int min_beats = 3;

	int total_beats = base_beats - ((archer->points.ENE_regen / beats_modifier) - beats_modifier);
	total_beats = total_beats - (utils::get_prof_level(PROF_RANGER, *archer) / beats_modifier);

	if (archer->player.race == RACE_WOOD)
	{
		total_beats = total_beats - 1;
	}

	total_beats = std::max(total_beats, min_beats);
	return total_beats;

}

/*
 * This function will determine if an arrow breaks based on the
 * victims armor and percentage on arrows themself.
 * --------------------------- Change Log --------------------------------
 * drelidan: Jan 26, 2017 - Created function
 */
bool does_arrow_break(const char_data* victim, const obj_data* arrow)
{
	//TODO(drelidan):  Add logic for calculating breaking here.
	const int breakpercentage = arrow->obj_flags.value[3];
	if (victim)
	{
		// factor in victim contribution here - but victim is optional.
	}

	const int rolledNumber = number(1, 100);
	if (rolledNumber < breakpercentage)
	{
		return true;
	}
	return false;
}

/*
 * move_arrow_to_victim will take the arrow out of the shooters quiver and
 * tag the arrow with their character_id. After that it will move the arrow
 * into the victims inventory. We tag the arrow so the shooter can type recover
 * after the kill and it will return all arrows with his character_id on it.
 *
 * This function will also handle the breaking of arrows based on the
 * victims armor and percentage on arrows themself.
 *
 * Returns true if the arrow was moved to the victim, false if it was destroyed.
 * --------------------------- Change Log --------------------------------
 * slyon: Jan 25, 2017 - Created function
 * drelidan: Jan 26, 2017 - Implemented function logic.
 */
bool move_arrow_to_victim(char_data* archer, char_data* victim, obj_data* arrow)
{
	// Remove object from the character.
	obj_from_obj(arrow);
	obj_to_char(arrow, archer); // Move it into his inventory.
	if (does_arrow_break(victim, arrow))
	{
		// Destroy the arrow and exit.
		extract_obj(arrow);
		return false;
	}
	//tag arrow in value slot 2 of the shooter
	arrow->obj_flags.value[2] = (int)archer->specials2.idnum;

	// Move the arrow to the victim.
	obj_from_char(arrow);
	obj_to_char(arrow, victim);

	return true;
}

/*
* move_arrow_to_room will take the arrow out of the shooters quiver and
* tag the arrow with their character_id. After that it will deposit the arrow
* on the ground. We tag the arrow so the shooter can type recover
* after the kill and it will return all arrows with his character_id on it.
*
* This function will also handle the breaking of arrows based on the
* victims armor and percentage on arrows themself.
*
* Returns true if the arrow was moved to the victim, false if it was destroyed.
* --------------------------- Change Log --------------------------------
* drelidan: Feb 07, 2017 - Created function
*/
bool move_arrow_to_room(char_data* archer, obj_data* arrow, int room_num)
{
	// Remove object from the character.
	obj_from_obj(arrow);
	obj_to_char(arrow, archer); // Move it into his inventory.
	if (does_arrow_break(NULL, arrow))
	{
		// Destroy the arrow and exit.
		extract_obj(arrow);
		return false;
	}
	//tag arrow in value slot 2 of the shooter
	arrow->obj_flags.value[2] = (int)archer->specials2.idnum;

	// Move the arrow to the room.
	obj_from_char(arrow);
	obj_to_room(arrow, room_num);

	return true;
}

/*
 * can_ch_shoot will check all the prereqs for shooting a bow
 *  -- Does the ch have the correct equipment
 *  -- Does the ch have SKILL_ARCHERY prac'd
 * --------------------------- Change Log --------------------------------
 * slyon: Jan 24, 2017 - Created function
 * slyon: Jan 25, 2017 - Renamed function to better reflect what it's doing
 */
bool can_ch_shoot(char_data* archer)
{
	using namespace utils;

	if (is_shadow(*archer)) {
		send_to_char("Hmm, perhaps you've spent to much time in the "
			" mortal lands.\r\n", archer);
		return false;
	}

	if (is_npc(*archer) && is_mob_flagged(*archer, MOB_ORC_FRIEND)) {
		send_to_char("Leave that to your leader.\r\n", archer);
		return false;
	}

	const room_data& room = world[archer->in_room];
	if (is_set(room.room_flags, (long)PEACEROOM)) {
		send_to_char("A peaceful feeling overwhelms you, and you cannot bring"
			" yourself to attack.\r\n", archer);
		return false;
	}

	const obj_data* weapon = archer->equipment[WIELD];
	if (!weapon || !isname("bow", weapon->name)) {
		send_to_char("You must be wielding a bow to shoot.\r\n", archer);
		return false;
	}
	/* slyon:
	 * We need to check if they have something equiped on their back
	 * before we check for the name or it will crash the game.
	 */
	const obj_data* quiver = archer->equipment[WEAR_BACK];
	if (!quiver || !isname("quiver", quiver->name))
	{
		send_to_char("You must be wearing a quiver on your back.\r\n", archer);
		return false;
	}

	if (!quiver->contains)
	{
		send_to_char("Your quiver is empty!  Find some more arrows.\r\n", archer);
		return false;
	}

	if (!is_twohanded(*archer)) {
		send_to_char("You must be wielding your bow with two hands.\r\n", archer);
		return false;
	}

	if (get_skill(*archer, SKILL_ARCHERY) == 0) {
		send_to_char("Learn how to shoot a bow first.\r\n", archer);
		return false;
	}


	return true;
}

/*
 * is_targ_valid will check to see if the target the shoot is a valid
 * one and return true if so and false if not.
 * --------------------------- Change Log --------------------------------
 * slyon: Jan 25, 2017 - Created function
 */
char_data* is_targ_valid(char_data* archer, waiting_type* target)
{
	char_data* victim = NULL;

	if (target->targ1.type == TARGET_TEXT)
	{
		victim = get_char_room_vis(archer, target->targ1.ptr.text->text);
	}
	else if (target->targ1.type == TARGET_CHAR)
	{
		if (char_exists(target->targ1.ch_num))
		{
			victim = target->targ1.ptr.ch;
		}
	}

	if (victim == NULL)
	{
		if (archer->specials.fighting)
		{
			victim = archer->specials.fighting;
		}
		else
		{
			send_to_char("Shoot who?\r\n", archer);
			return NULL;
		}
	}

	if (archer->in_room != victim->in_room)
	{
		send_to_char("Your victim is no longer here.\r\n", archer);
		return NULL;
	}
	if (!CAN_SEE(archer, victim))
	{
		send_to_char("Shoot who?\r\n", archer);
		return NULL;
	}

	return victim;
}

//============================================================================
// Moves the arrow to the room specified.
//============================================================================
void do_move_arrow_to_room(char_data* archer, obj_data* arrow, int room_num)
{
	send_to_char("Your arrow harmlessly flies past your target.\r\n", archer);
	move_arrow_to_room(archer, arrow, room_num);
}

//============================================================================
// Handles an arrow hitting a target - calculating and applying damage, and
// moving the arrow to that target.
//============================================================================
void on_arrow_hit(char_data* archer, char_data* victim, obj_data* arrow)
{
	int hit_location = 0;
	int damage_dealt = shoot_calculate_damage(archer, victim, arrow, hit_location);
	move_arrow_to_victim(archer, victim, arrow);
	damage(archer, victim, damage_dealt, SKILL_ARCHERY, hit_location);
}

//============================================================================
// Handles the arrow hitting another target.  Currently only considers characters
// in combat with the victim as potential targets.
//============================================================================
void change_arrow_target(char_data* archer, char_data* victim, obj_data* arrow)
{
	const room_data& room = world[archer->in_room];

	// Get the list of people that are in-combat with the victim, and
	// ensure that the archer isn't in the list of potential targets.

	typedef std::vector<char_data*>::iterator iter;

	std::vector<char_data*> potential_targets = utils::get_engaged_characters(victim, room);

	iter archer_iter = std::remove(potential_targets.begin(), potential_targets.end(), archer);
	if (archer_iter != potential_targets.end())
	{
		potential_targets.erase(archer_iter);
	}

	iter victim_iter = std::remove(potential_targets.begin(), potential_targets.end(), victim);
	if (victim_iter != potential_targets.end())
	{
		potential_targets.erase(victim_iter);
	}

	// If there aren't any targets, have the arrow fall into the room.
	if (potential_targets.empty())
	{
		do_move_arrow_to_room(archer, arrow, archer->in_room);
	}
	else
	{
		int target_roll = number(0, potential_targets.size() - 1);
		char_data* new_victim = potential_targets.at(target_roll);

		send_to_char("Your shot misses your target and flies into someone else!\r\n", archer);
		on_arrow_hit(archer, new_victim, arrow);
	}
}

//============================================================================
// Gets the room that the arrow will land in.
//============================================================================
int get_arrow_landing_location(const room_data& room)
{
	// The arrow flies into an adjacent room.
	// Build the possible exit list...
	int valid_dirs = 0;
	int exit_indices[NUM_OF_DIRS] = { -1 };
	for (int i = 0; i < NUM_OF_DIRS; ++i)
	{
		// The room exit exits and goes somewhere.
		room_direction_data* dir = room.dir_option[i];
		if (dir && dir->to_room != NOWHERE)
		{
			// The exit has a door...
			if (utils::is_set(dir->exit_info, EX_ISDOOR))
			{
				// But it's open, so the arrow can fly that way.
				if (!utils::is_set(dir->exit_info, EX_CLOSED))
				{
					exit_indices[valid_dirs++] = i;
				}
			}
			// There's no door - this is a valid location for the arrow to land.
			else
			{
				exit_indices[valid_dirs++] = i;
			}
		}
	}

	if (valid_dirs == 0)
	{
		// The arrow has to fall in the room passed in.
		return room.number;
	}
	else
	{
		int random_exit = number(0, valid_dirs - 1);
		int exit_index = exit_indices[random_exit];

		// The arrow flies into a nearby room.
		return room.dir_option[exit_index]->to_room;
	}
}

//============================================================================
// Handles an arrow missing the target.
// The arrow may impact into someone else, go into a separate room, or land
// harmlessly on the ground (if it doesn't break).
//============================================================================
void on_arrow_miss(char_data* archer, char_data* victim, obj_data* arrow)
{
	const room_data& room = world[archer->in_room];

	double roll = number();
	roll -= 0.20;
	if (roll <= 0)
	{
		change_arrow_target(archer, victim, arrow);
		return;
	}

	int arrow_landing_location = archer->in_room;

	roll -= 0.01;
	if (roll <= 0)
	{
		arrow_landing_location = get_arrow_landing_location(room);
	}

	// The arrow falls into this or a nearby room.
	send_to_char("Your arrow harmlessly flies past your target.\r\n", archer);
	move_arrow_to_room(archer, arrow, arrow_landing_location);
}

/*
 * do_shoot will attempt to shoot the victim with a bow or crossbow
 * There is a lot of things going on with this ACMD so just read through it.
 * --------------------------- Change Log --------------------------------
 * slyon: Jan 25, 2017 - Created ACMD
 */
ACMD(do_shoot)
{
	struct char_data *victim = NULL;
	int success, dmg;

	one_argument(argument, arg);

	if (subcmd == -1)
	{
		send_to_char("You could not concentrate on shooting anymore!\r\n", ch);
		ch->specials.ENERGY = std::min(ch->specials.ENERGY, (sh_int)0); // reset swing timer after interruption.
		return;
	}

	if (!can_ch_shoot(ch))
		return;

	if (utils::is_affected_by(*ch, AFF_SANCTUARY))
	{
		appear(ch);
		send_to_char("You cast off your sanctuary!\r\n", ch);
		act("$n renounces $s sanctuary!", FALSE, ch, 0, 0, TO_ROOM);
	}

	switch (subcmd)
	{
	case 0:
	{
		victim = is_targ_valid(ch, wtl);
		if (victim == NULL)
		{
			return;
		}

		send_to_char("You draw back your bow and prepare to fire...\r\n", ch);
		if (GET_SEX(ch) == SEX_MALE)
		{
			act("$n draws back his bow and prepares to fire...\r\n", FALSE, ch, 0, 0, TO_ROOM);
		}
		else if (GET_SEX(ch) == SEX_FEMALE)
		{
			act("$n draws back her bow and prepares to fire...\r\n", FALSE, ch, 0, 0, TO_ROOM);
		}
		else
		{
			act("$n draws back their bow and prepares to fire...\r\n", FALSE, ch, 0, 0, TO_ROOM);
		}
		int wait_delay = shoot_calculate_wait(ch);
		WAIT_STATE_FULL(ch, wait_delay, CMD_SHOOT, 1, 30, 0, 0, victim, AFF_WAITING | AFF_WAITWHEEL, TARGET_CHAR);
	}
		break;
	case 1:
	{
		if ((wtl->targ1.type != TARGET_CHAR) || !char_exists(wtl->targ1.ch_num))
		{
			send_to_char("Your victim is no longer among us.\r\n", ch);
			return;
		}

		victim = reinterpret_cast<char_data*>(wtl->targ1.ptr.ch);
		if (!CAN_SEE(ch, victim))
		{
			send_to_char("Shoot who?\r\n", ch);
			return;
		}

		if (ch->in_room != victim->in_room)
		{
			send_to_char("Your target is not here any longer\r\n", ch);
			return;
		}

		// Get the arrow.
		obj_data* quiver = ch->equipment[WEAR_BACK];
		obj_data* arrow = quiver->contains;
		send_to_char("You release your arrow and it goes flying!\r\n", ch);

		if (GET_SEX(ch) == SEX_MALE)
		{
			act("$n releases his arrow and it goes flying!\r\n", FALSE, ch, 0, 0, TO_ROOM);
		}
		else if (GET_SEX(ch) == SEX_FEMALE)
		{
			act("$n releases her arrow and it goes flying!\r\n", FALSE, ch, 0, 0, TO_ROOM);
		}
		else
		{
			act("$n releases their arrow and it goes flying!\r\n", FALSE, ch, 0, 0, TO_ROOM);
		}

		int roll = number(0, 99);
		int target_number = shoot_calculate_success(ch, victim, arrow);
		if (roll < target_number)
		{
			on_arrow_hit(ch, victim, arrow);
		}
		else
		{
			on_arrow_miss(ch, victim, arrow);
		}

		ch->specials.ENERGY = std::min(ch->specials.ENERGY, (sh_int)0); // reset swing timer after loosing an arrow.
	}
		break;
	default:
		sprintf(buf2, "do_shoot: illegal subcommand '%d'.\r\n", subcmd);
		mudlog(buf2, NRM, LEVEL_IMMORT, TRUE);
		abort_delay(ch);
		return;
	}
}

//============================================================================
// Gets all arrows in the object list that are tagged to the character.  These
// arrows are placed in the 'arrows' vector.
//============================================================================
void get_tagged_arrows(const char_data* character, obj_data* obj_list, std::vector<obj_data*>& arrows)
{
	const int arrow_type = 7;

	// Iterate through items in the list.
	for (obj_data* item = obj_list; item; item = item->next_content)
	{
		// TODO(drelidan): See if there's another way to determine that these items are ammo.
		if (item->obj_flags.type_flag == arrow_type)
		{
			if (item->obj_flags.value[2] == character->specials2.idnum)
			{
				arrows.push_back(item);
			}
		}
	}
}

//============================================================================
// Gets all arrows in the room that are tagged to the character passed in.
//============================================================================
void get_room_tagged_arrows(const char_data* character, std::vector<obj_data*>& arrows)
{
	const room_data& room = world[character->in_room];
	obj_data* obj_list = room.contents;

	return get_tagged_arrows(character, obj_list, arrows);
}

//============================================================================
// Gets all arrows from the corpses in the room that are tagged to the 
// character passed in.
//============================================================================
void get_corpse_tagged_arrows(const char_data* character, std::vector<obj_data*>& arrows)
{
	const room_data& room = world[character->in_room];
	obj_data* obj_list = room.contents;

	// Iterate through items in the list.
	for (obj_data* item = obj_list; item; item = item->next_content)
	{
		if (strstr(item->name, "corpse") != NULL)
		{
			get_tagged_arrows(character, item->contains, arrows);
		}
	}
}

//============================================================================
// Recovers arrows that have been tagged by a player from the room that the
// player is in, and any corpses within the room.
//============================================================================
void do_recover(char_data* character, char* argument, waiting_type* wait_list, int command, int sub_command)
{
	if (character == NULL)
		return;

	// Characters cannot recover arrows if they are blind.
	if (!CAN_SEE(character))
	{
		send_to_char("You can't see anything in this darkness!", character);
		return;
	}

	// Characters cannot recover arrows if they are a shadow.
	if (utils::is_shadow(*character))
	{
		send_to_char("Try rejoining the corporal world first...", character);
		return;
	}

	int max_inventory = utils::get_carry_item_limit(*character);
	int max_carry_weight = utils::get_carry_weight_limit(*character);

	std::vector<obj_data*> arrows_to_get;
	get_room_tagged_arrows(character, arrows_to_get);
	get_corpse_tagged_arrows(character, arrows_to_get);
	
	if (arrows_to_get.empty())
	{
		send_to_char("You have no expended arrows here.", character);
		return;
	}

	int num_recovered = 0;

	typedef std::vector<obj_data*>::iterator iter;
	for (iter arrow_iter = arrows_to_get.begin(); arrow_iter != arrows_to_get.end(); ++arrow_iter)
	{
		obj_data* arrow = *arrow_iter;
		if (character->specials.carry_items < max_inventory)
		{
			if (character->specials.carry_weight + arrow->get_weight() < max_carry_weight)
			{
				++num_recovered;
				obj_data* arrow_container = arrow->in_obj;
				if (arrow_container)
				{
					obj_from_obj(arrow);
				}
				if (arrow->in_room >= 0)
				{
					obj_from_room(arrow);
				}
				obj_to_char(arrow, character);
			}
			else
			{
				send_to_char("You can't carry that many items!", character);
				break;
			}
		}
		else
		{
			send_to_char("You can't that much weight!", character);
			break;
		}
	}

	std::ostringstream message_writer;
	message_writer << "You recover " << num_recovered << (num_recovered > 1 ? " arrows." : " arrow.") << std::endl;
	std::string message = message_writer.str();
	send_to_char(message.c_str(), character);

	act("$n recovers some arrows.\r\n", FALSE, character, 0, 0, TO_ROOM);
}