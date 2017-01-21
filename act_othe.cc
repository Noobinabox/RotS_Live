/**************************************************************************
*   File: act.other.c                                   Part of CircleMUD *
*  Usage: Miscellaneous player-level commands                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993 by the Trustees of the Johns Hopkins University     *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "platdef.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpre.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "color.h"
#include "limits.h"
#include "profs.h"

/* extern variables */
extern struct descriptor_data *descriptor_list;
extern struct char_data *waiting_list;
extern struct index_data *mob_index;
extern struct skill_data skills[];
extern struct room_data world;
extern byte language_skills[];
extern byte language_number;
extern char *prof_abbrevs[];
extern char *race_abbrevs[];
extern int top_of_world;
extern int rev_dir[];
extern char *dirs[];

/* extern procedures */
extern int old_search_block(char *, int, unsigned int, char **, int);
extern int get_followers_level(struct char_data *);
extern int get_real_stealth(struct char_data *);
extern void check_break_prep(struct char_data *);
SPECIAL(shop_keeper);
ACMD(do_gen_com);
ACMD(do_look);
ACMD(do_gsay);
ACMD(do_say);
ACMD(do_hit); 




ACMD(do_quit)
{
  int tmp;
  void die(struct char_data *, struct char_data *, int);
  
  if(IS_NPC(ch) || !ch->desc || !ch->desc->descriptor)
    return;

  if(subcmd != SCMD_QUIT) {
    send_to_char("You have to type quit - no less - to quit!\n\r", ch);
    return;
  }

  if(GET_POS(ch) == POSITION_FIGHTING) {
    send_to_char("No way!  You're fighting for your life!\n\r", ch);
    return;
  }

  if(affected_by_spell(ch, SPELL_ANGER)){
    send_to_char("You may not quit yet.\n\r",ch);
    return;
  }

  if(GET_POS(ch) < POSITION_STUNNED) {
    send_to_char("You die before your time...\n\r", ch);
    die(ch, 0, 0);
    return;
  }

  if(GET_LEVEL(ch) >= LEVEL_IMMORT)
    act("Goodbye, friend.. Come back soon!", FALSE, ch, 0, 0, TO_CHAR);
  else
    act("As you quit, all your posessions drop to the ground!", 
	FALSE, ch, 0, 0, TO_CHAR);
  
  act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);

  sprintf(buf, "%s has quit the game.", GET_NAME(ch));
  mudlog(buf, NRM, (sh_int)MAX(LEVEL_GOD, GET_INVIS_LEV(ch)), TRUE);

  if(GET_LEVEL(ch) >= LEVEL_IMMORT) {
    Crash_crashsave(ch, RENT_QUIT);
    for(tmp = 0; tmp < MAX_WEAR; tmp++)
      if(ch->equipment[tmp]) 
	extract_obj(unequip_char(ch, tmp));
     while(ch->carrying) 
       extract_obj(ch->carrying);
     /* char is saved in extract_char */
     extract_char(ch);
  }
  else {
    /* same as above */
    extract_char(ch);
    Crash_crashsave(ch);
  }
}



ACMD(do_save)
{
  if(IS_NPC(ch) || !ch->desc)
    return;

  if(cmd) {
    sprintf(buf, "Saving %s.\n\r", GET_NAME(ch));
    send_to_char(buf, ch);
  }
  save_char(ch, NOWHERE,0);
  Crash_crashsave(ch);
}



ACMD(do_not_here)
{
  send_to_char("Sorry, but you can't do that here!\n\r", ch);
}


ACMD(do_recruit)
{
  struct char_data *victim;
  struct affected_type af;
  struct follow_type *j, *k;
  int level_total = 0, num_followers = 0;

  one_argument(argument, arg);

  if(GET_RACE(ch) != RACE_ORC) {
    send_to_char("You can't recruit.\n\r", ch);
    return;
  }

  if(IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("Your superior would not approve of your building an army.\n\r", ch);
    return;
  }

  if(IS_SHADOW(ch)) {
    send_to_char("Who'd take orders from a shadow?\n\r",ch);
    return;
  }

  if(*arg) {
    if(!str_cmp(arg, "all")) {
      send_to_char("Recruiting an army cannot be rushed, "
		   "choose one follower at a time.\r\n", ch);
      return;
    }
  }
  
  for(k = ch->followers; k; k = j) {
    j = k->next;
    if(IS_NPC(k->follower) && MOB_FLAGGED(k->follower, MOB_ORC_FRIEND)) {
      level_total += GET_LEVEL(k->follower);
      num_followers++;
    }
  }

  if(!(victim = get_char_room_vis(ch, arg)) || !CAN_SEE(ch, victim)) {
    send_to_char("Recruit who?\n\r", ch);
    return;
  }

  if(!IS_NPC(victim) || !(MOB_FLAGGED(victim, MOB_ORC_FRIEND))) {
    char buf[1024];

    sprintf(buf, "%s doesn't want to be recruited.\n\r", GET_NAME(victim));
    send_to_char(buf, ch);
    return;
  }

  if(GET_LEVEL(victim) > ((GET_LEVEL(ch) * 2 + 2) / 5)) {
    send_to_char("That target is too high a level to recruit.\n\r", ch);
    return;
  }

  /*
   * Orcs get four followers with a cap of 48 total levels;
   * one follower level per one player level until level 48.
   * We allow orcs such a large force due to the fact that
   * they can't follow or be followed by other players.
   */
  if (level_total + GET_LEVEL(victim) > MIN(GET_LEVEL(ch), 48) ||
      num_followers >= 4) {
    send_to_char("You have already recruited too powerful "
		 "a force for this target to be added.\n\r", ch);
    return;
  }

  /*- change 'your target' to the appropriate message (animal's name) */
  if(GET_POS(victim) == POSITION_FIGHTING){
    send_to_char("Your target is too enraged!\n\r", ch);
    return;
  }

  if(IS_AFFECTED(victim, AFF_CHARM)) {
    send_to_char("Your target has already been charmed.\n\r",ch);
    return;
  }

  if(victim->master) {
    send_to_char("Your target is already following someone else.\n\r", ch);
    return;
  }

  if (victim == ch) {
    send_to_char("Aren't we funny today...\n\r", ch);
    return;
  }

  if(GET_POS(ch) == POSITION_FIGHTING) {
    send_to_char("You are too busy to do that.\n\r",ch);
    return;
  }

  if(circle_follow(victim, ch, FOLLOW_MOVE)) {
    send_to_char("Sorry, following in circles is not allowed.\n\r", ch);
    return;
  }

  if(victim->master)
    stop_follower(victim, FOLLOW_MOVE);
  affect_from_char(victim, SKILL_RECRUIT);
  add_follower(victim, ch, FOLLOW_MOVE);

  act("$n joins the company of $N.", FALSE, victim, 0, ch, TO_ROOM);
  af.type      = SKILL_RECRUIT;
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
  
  victim->specials2.pref = 0; /* remove mob aggressions */
}


ACMD(do_title)
{
  for(; isspace(*argument); argument++);

  if(IS_NPC(ch))
    send_to_char("Your title is fine... go away.\n\r", ch);
  else if(GET_LEVEL(ch) < 20)
    send_to_char("You can't set your title yet.\n\r", ch);
  else if(PLR_FLAGGED(ch, PLR_NOTITLE))
    send_to_char("You can't title yourself - you shouldn't have abused it!\n\r", ch);
  else if(!*argument) {
    sprintf(buf,"Your present title is: %s\n\r",GET_TITLE(ch));
    send_to_char(buf, ch);
  }
  else if(strstr(argument, "(") || strstr(argument, ")"))
    send_to_char("Titles can't contain the ( or ) characters.\n\r", ch);
  else if (strlen(argument) > 80)
    send_to_char("Sorry, titles can't be longer than 80 characters.\n\r", ch);
  else {
    if(GET_TITLE(ch))
      RELEASE(GET_TITLE(ch));
    CREATE(GET_TITLE(ch), char, strlen(argument) + 1);
    strcpy(GET_TITLE(ch), argument);

    sprintf(buf, "OK, you're now %s %s.\n\r", GET_NAME(ch), GET_TITLE(ch));
    send_to_char(buf, ch);
  }
}



ACMD(do_group)
{
  int tmp1, tmp2, tmp3;
  char found;
  struct char_data *victim, *k;
  struct follow_type *f;

  extern struct prompt_type prompt_hit[];
  extern struct prompt_type prompt_mana[];
  extern struct prompt_type prompt_move[];

  one_argument(argument, buf);
  
  if(IS_SHADOW(ch)){
    send_to_char("You cannot group whilst inhabiting the shadow world.\n\r", ch);
    return;
  }

  if(!*buf) {
    if(!ch->group_leader && !ch->group)
      send_to_char("But you are not the member of a group!\n\r", ch);
    else {
      send_to_char("Your group consists of:\n\r", ch);
      
      /* first, display the group leader's status */
      k = (ch->group_leader ? ch->group_leader : ch);
      for(tmp1=0; (1000*GET_HIT(k))/GET_MAX_HIT(k) > prompt_hit[tmp1].value; tmp1++);
      for(tmp2=0; (1000*GET_MANA(k))/GET_MAX_MANA(k) > prompt_mana[tmp2].value; tmp2++);
      for(tmp3=0; (1000*GET_MOVE(k))/GET_MAX_MOVE(k) > prompt_move[tmp3].value; tmp3++);
         
      sprintf(buf, "HP:%9s,%11s,%13s -- $N (Head of group)",
	      prompt_hit[tmp1].message,                                        
	      *prompt_mana[tmp2].message == '\0' ? 
	      "S:Full" : prompt_mana[tmp2].message,
	      *prompt_move[tmp3].message == '\0' ? 
	      "MV:Energetic" : prompt_move[tmp3].message);
      act(buf, FALSE, ch, 0, k, TO_CHAR);


      /* now, we display the group members */

      for(f = k->group; f ; f = f->next) {
	for(tmp1=0; (1000*GET_HIT(f->follower))/
	      (GET_MAX_HIT(f->follower)==0?1:GET_MAX_HIT(f->follower)) > prompt_hit[tmp1].value; tmp1++);
	for(tmp2=0; (1000*GET_MANA(f->follower))/
	      (GET_MAX_MANA(f->follower)==0?1:GET_MAX_MANA(f->follower)) > prompt_mana[tmp2].value; tmp2++);
	for(tmp3=0; (1000*GET_MOVE(f->follower))/
	      (GET_MAX_MOVE(f->follower)==0?1:GET_MAX_MOVE(f->follower)) > prompt_move[tmp3].value; tmp3++);
	if(MOB_FLAGGED(f->follower, MOB_ORC_FRIEND))
	  sprintf(buf, "HP:%9s,%11s,%13s -- $N (Lvl:%2d)",
		  prompt_hit[tmp1].message,
		  *prompt_mana[tmp2].message == '\0' ? 
		  "S:Full" : prompt_mana[tmp2].message,
		  *prompt_move[tmp3].message == '\0' ? 
		  "MV:Energetic":prompt_move[tmp3].message,
		  GET_LEVEL(f->follower));
	else
	  sprintf(buf, "HP:%9s,%11s,%13s -- $N",
		  prompt_hit[tmp1].message,
		  *prompt_mana[tmp2].message == '\0' ? 
		  "S:Full":prompt_mana[tmp2].message,
		  *prompt_move[tmp3].message == '\0' ? 
		  "MV:Energetic":prompt_move[tmp3].message);
	act(buf, FALSE, ch, 0, f->follower, TO_CHAR);
      }    
    }  
    return;
  }
  
  if(ch->group_leader) {
    act("You can not enroll group members without being head of a group.",
	FALSE, ch, 0, 0, TO_CHAR);
    return;
  }

  if(!str_cmp(buf, "all")) {
    victim = 0;
    for(f = ch->followers; f; f = f->next) {
      victim = f->follower;
      if(!char_exists(f->fol_number)) 
	f->follower = victim = 0;
      if(victim && !victim->group_leader && !other_side(ch, victim))
	add_follower(victim, ch, FOLLOW_GROUP);
    }  
    return;
  }

  if(!(victim = get_char_room_vis(ch, buf))) {
    send_to_char("No one here by that name.\n\r", ch);
    return;
  } 
  else {
    found = FALSE;

    if(victim == ch)
      found = TRUE;
    else
      for(f = ch->group; f; f = f->next)
	if(f->follower == victim) {
	  found = TRUE;
	  break;
	}

    if(found) {
      act("You have been kicked out of $n's group!",
	  FALSE, ch, 0, victim, TO_VICT);
      if(victim == ch)
	send_to_char("You are already grouped with yourself.\n\r",ch);
      else
	stop_follower(victim, FOLLOW_GROUP);
    }
    else {
      if(other_side(ch, victim)) {
	sprintf(buf,"You wouldn't group with that ugly %s!\n\r",
		pc_races[GET_RACE(victim)]);
	send_to_char(buf, ch);
	return;
      }
      if(victim == ch)
	send_to_char("You are already grouped with yourself.\n\r",ch);
      else if(victim->group_leader)
	act("$N is busy somewhere else already.",
	    FALSE, ch, 0, victim, TO_CHAR);
      else
	add_follower(victim, ch, FOLLOW_GROUP);  
    }
  }
}



ACMD(do_ungroup)
{
  struct follow_type *f, *next_fol;
  struct char_data *tch;

  one_argument(argument, buf);
  
  if(!*buf) {
    if(ch->group_leader) {
      stop_follower(ch, FOLLOW_GROUP);
      return;
    }
    else if(!ch->group) {
      send_to_char("You have no group to leave.\n\r",ch);
      return;
    }
    else {
      sprintf(buf2, "%s has disbanded the group.\n\r", GET_NAME(ch));
      for(f = ch->group; f; f = next_fol) {
	next_fol = f->next;
	send_to_char(buf2, f->follower);
	stop_follower(f->follower, FOLLOW_GROUP);
      }
      send_to_char("You have disbanded the group.\n\r", ch);
      return;
    }
  }
  
  if(!(tch = get_char_room_vis(ch, buf))) {
    send_to_char("There is no such person!\n\r", ch);
    return;
  }
  
  if(tch->group_leader != ch) {
    send_to_char("That person is not in your group!\n\r", ch);
    return;
  }

  act("You have been kicked out of $n's group!", FALSE, ch, 0, tch, TO_VICT);
  stop_follower(tch, FOLLOW_GROUP);
}



ACMD(do_report)
{
  char str[255];
  int tmp1, tmp2, tmp3;
  char *tmpchar;

  extern struct prompt_type prompt_hit[];
  extern struct prompt_type prompt_mana[];
  extern struct prompt_type prompt_move[];
  
  if(IS_NPC(ch) && MOB_FLAGGED(ch, MOB_PET) && 
     !(MOB_FLAGGED(ch, MOB_ORC_FRIEND))) {
    send_to_char("Sorry, tamed mobiles can't report.\n\r", ch);
    return;
  }

  for(tmp1=0; (1000*GET_HIT(ch))/GET_MAX_HIT(ch) > prompt_hit[tmp1].value; tmp1++);
  for(tmp2=0; (1000*GET_MANA(ch))/GET_MAX_MANA(ch) > prompt_mana[tmp2].value; tmp2++);
  for(tmp3=0; (1000*GET_MOVE(ch))/GET_MAX_MOVE(ch) > prompt_move[tmp3].value; tmp3++);

  sprintf(str, "I am %s, my stamina is %s, and I am %s.",
	  prompt_hit[tmp1].message,                     // No need to add
	  *prompt_mana[tmp2].message == '\0' ? 
	  "full" : prompt_mana[tmp2].message + 3,       // Add 3 because of M:
	  *prompt_move[tmp3].message == '\0' ? 
	  "energetic" : prompt_move[tmp3].message + 4); // Add 4 because of MV:

  for(tmpchar = &str[1]; *tmpchar != '\0'; tmpchar++)
    *tmpchar = tolower(*tmpchar);

  if(ch->group || ch->group_leader)
    do_gsay(ch, str, 0, 0, 0);
  else
    do_say(ch, str, 0, 0, 0);
}



ACMD(do_split)
{
  int amount, num, share;
  char *current_arg;
  struct char_data *k;
  struct follow_type *f;

  if(IS_NPC(ch))
    return;

  current_arg = one_argument(argument, buf);

  if(is_number(buf)) {
    amount = atoi(buf);
    if(amount <= 0) {
      send_to_char("Sorry, you can't do that.\n\r", ch);
      return;
    }

    one_argument(current_arg, buf);
    if(!strcmp(buf, "gold") || !strcmp(buf, "silver") || 
       !strcmp(buf, "copper")) {
      
      /* save some strcmp'ing time since only the previous three
       * arguments could possibly have made it here */
      switch(*buf) {
      case 'g':
	amount *= 1000;
	break;
      case 's':
	amount *= 100;
	break;
      case 'c':
	amount *= 1;
	break;
      }

      if(amount > GET_GOLD(ch)) {
	send_to_char("You don't have that much coin to split.\n\r", ch);
	return;
      }

      if(ch->group_leader)
	k = ch->group_leader;
      else
	k = ch;
      
      /* num starts at 1, because the group leader is never
       * listed in the group */
      for(num = 1, f = k->group; f; f = f->next)
	if((!IS_NPC(f->follower)) && 
	   (f->follower->in_room == ch->in_room))
	  num++;
      
      if(num > 1)
	share = amount / num;
      else {
	send_to_char("There is no one here for you to share with.\r\n", ch);
	return;
      }

      GET_GOLD(ch) -= share * (num - 1);
      
      /* special check for `k' -- group leader, because the leader
       * is not listed in the group */
      if((k->in_room == ch->in_room)
	 && !(IS_NPC(k)) &&  k != ch) {
	GET_GOLD(k) += share;
	sprintf(buf, "%s splits some money among the group; you receive %s.\r\n", 
		GET_NAME(ch), money_message(share, 0));
	send_to_char(buf, k);
      }
      
      for(f = k->group; f; f = f->next) {
	if((!IS_NPC(f->follower)) && 
	   (f->follower->in_room == ch->in_room) && 
	   f->follower != ch) {
	  GET_GOLD(f->follower) += share;
	  sprintf(buf, "%s splits some money among the group; you receive %s.\r\n",
		  GET_NAME(ch), money_message(share, 0));
	  send_to_char(buf, f->follower);
	}
      }
      sprintf(buf, "You give %s to each member of your group.\n\r",
	      money_message(share, 0));
      send_to_char(buf, ch);
    }
    else
      send_to_char("You must specify what type of coin to split.\r\n", ch);
  } 
  else
    send_to_char("You must specify how much you wish to split.\r\n", 
		 ch);
}



ACMD(do_use)
{
  int bits;
  struct char_data *tmp_char;
  struct obj_data *tmp_object, *stick;

  argument = one_argument(argument, buf);

  if(ch->equipment[HOLD] == 0 || !isname(buf, ch->equipment[HOLD]->name)) {
    act("You do not hold that item in your hand.", FALSE, ch, 0, 0, TO_CHAR);
    return;
  }

  stick = ch->equipment[HOLD];
  
  if(stick->obj_flags.type_flag == ITEM_STAFF) {
    act("$n taps $p three times on the ground.", TRUE, ch, stick, 0, TO_ROOM);
    act("You tap $p three times on the ground.", FALSE, ch, stick, 0, TO_CHAR);

    if(stick->obj_flags.value[2] > 0) {  /* Is there any charges left? */
      stick->obj_flags.value[2]--;
      if(*skills[stick->obj_flags.value[3]].spell_pointer)
	((*skills[stick->obj_flags.value[3]].spell_pointer)
	 ((byte) stick->obj_flags.value[0], ch, "", SPELL_TYPE_STAFF, 
	  0, 0, 0, 0));
      
    } 
    else
      send_to_char("The staff seems powerless.\n\r", ch);
  } 
  else if(stick->obj_flags.type_flag == ITEM_WAND) {
    bits = generic_find(argument, FIND_CHAR_ROOM | FIND_OBJ_INV | 
			FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, 
			&tmp_object);
    if(bits) {
      if(bits == FIND_CHAR_ROOM) {
	act("$n points $p at $N.", TRUE, ch, stick, tmp_char, TO_ROOM);
	act("You point $p at $N.", FALSE, ch, stick, tmp_char, TO_CHAR);
      }
      else {
	act("$n points $p at $P.", TRUE, ch, stick, tmp_object, TO_ROOM);
	act("You point $p at $P.", FALSE, ch, stick, tmp_object, TO_CHAR);
      }

      if(stick->obj_flags.value[2] > 0) { /* Is there any charges left? */
	stick->obj_flags.value[2]--;
	if(*skills[stick->obj_flags.value[3]].spell_pointer)
	  ((*skills[stick->obj_flags.value[3]].spell_pointer)
	   ((byte) stick->obj_flags.value[0], ch, "", SPELL_TYPE_WAND, 
	    tmp_char, tmp_object, 0, 0));
      } 
      else
	send_to_char("The wand seems powerless.\n\r", ch);
    }
    else
      send_to_char("What should the wand be pointed at?\n\r", ch);
  } 
  else
    send_to_char("Use is normally only for wands and staffs.\n\r", ch);
}



ACMD(do_wimpy)
{
  int wimp_lev;

  one_argument(argument, arg);

  if(!*arg) {
    if(WIMP_LEVEL(ch)) {
      sprintf(buf, "You will flee if your hit points drop below %d.\n\r",
	      ch->specials2.wimp_level);
      send_to_char(buf, ch);
      return;
    } 
    else {
      send_to_char("You will not flee from combat.\n\r", ch);
      return;
    }
  }
  
  if(isdigit(*arg)) {
    if((wimp_lev = atoi(arg))) {
      if(wimp_lev < 0)
	wimp_lev = 0;
      if(wimp_lev > GET_MAX_HIT(ch))
	wimp_lev = GET_MAX_HIT(ch);
      sprintf(buf, "OK, you'll flee if you drop below %d hit points.\n\r",
	      wimp_lev);
      send_to_char(buf, ch);
      WIMP_LEVEL(ch) = wimp_lev;
    } 
    else {
      send_to_char("OK, you'll tough out fights to the bitter end.\n\r", ch);
      WIMP_LEVEL(ch) = 0;
    }
  } 
  else
    send_to_char("At how many hit points do you wish to flee?\n\r", ch);
}



char *logtypes[] = {
  "off", 
  "brief", 
  "normal", 
  "spell", 
  "complete", 
  "\n" 
};

ACMD(do_syslog)
{
  int tp;
  
  if(IS_NPC(ch))
    return;
  
  one_argument (argument, arg);
  
  if(!*arg) {
    tp = ((PRF_FLAGGED(ch, PRF_LOG1) ? 1 : 0) +
	  (PRF_FLAGGED(ch, PRF_LOG2) ? 2 : 0) +
	  (PRF_FLAGGED(ch, PRF_LOG3) ? 4 : 0));
    sprintf(buf, "Your syslog is currently %s.\n\r", logtypes[tp]);
    send_to_char(buf, ch);
    return;
  }
  
  if(((tp = search_block(arg, logtypes, FALSE)) == -1)) {
    send_to_char("Usage: syslog { Off | Brief | Normal | Spell | Complete }\n\r", ch);
    return;
  }
  
  REMOVE_BIT(PRF_FLAGS(ch), PRF_LOG1 | PRF_LOG2 | PRF_LOG3);
  SET_BIT(PRF_FLAGS(ch), (PRF_LOG1 * (tp & 1)) | (PRF_LOG2 * (tp & 2) >> 1) |
	  (PRF_LOG3 * (tp & 4) >> 2));
  
  sprintf(buf, "Your syslog is now %s.\n\r", logtypes[tp]);
  send_to_char(buf, ch);
}







#define TOG_OFF 0
#define TOG_ON  1

#define PRF_TOG_CHK(ch,flag) ((TOGGLE_BIT(PRF_FLAGS(ch), (flag))) & (flag))

int 
flag_on(struct char_data *ch, int flag, char **message, int which)
{
  if(!which)
    SET_BIT(PRF_FLAGS(ch),(flag));
  else
    SET_BIT(PLR_FLAGS(ch),(flag));
    
  send_to_char(message[0],ch);
  return 1;
}



int 
flag_off(struct char_data *ch, int flag, char **message, int which)
{
  if(!which)
    REMOVE_BIT(PRF_FLAGS(ch),(flag));
  else
    REMOVE_BIT(PLR_FLAGS(ch),(flag));
  send_to_char(message[1],ch);
  return 0;
}



int 
flag_toggle(struct char_data *ch, int flag, char **message, int which)
{
  int i;
  if(!which) {
    TOGGLE_BIT(PRF_FLAGS(ch),(flag));
    i = IS_SET(PRF_FLAGS(ch),(flag));
  }
  else {
    TOGGLE_BIT(PLR_FLAGS(ch),(flag));
    i = IS_SET(PLR_FLAGS(ch),(flag));
  }
  if(i)
    send_to_char(message[0],ch);
  else
    send_to_char(message[1],ch);
  return i;
}



int 
flag_void(struct char_data *ch, int flag, char **message, int which)
{
  int i;
  if(!which)
    i = IS_SET(PRF_FLAGS(ch),(flag));
  else 
    i = IS_SET(PLR_FLAGS(ch),(flag));

  if(i)
    send_to_char(message[2],ch);
  else
    send_to_char(message[3],ch);

  return i;  
}



int 
(* flag_modify)(struct char_data *, int, char **, int);
char *tog_messages[][4] = {
  {
    "You are now safe from summoning by other players.\n\r",
    "You may now be summoned by other players.\n\r",
    "You are safe from summoning by other players.\n\r",
    "You may be summoned by other players.\n\r"
  },
  { 
    "You will now have your communication repeated.\r\n",
    "You will no longer have your communication repeated.\r\n",
    "You have your communication repeated.\r\n",
    "You don't have your communication repeated.\r\n",
  },
  { 
    "Brief mode now on.\n\r", 
    "Brief mode now off.\n\r",
    "Brief mode on.\n\r", 
    "Brief mode off.\n\r"
  },
  { 
    "Spam mode now on.\n\r", 
    "Spam mode now off.\n\r",
    "Spam mode on.\n\r", 
    "Spam mode off.\n\r" 
  },
  {
    "Compact mode now on.\n\r", 
    "Compact mode now off.\n\r",
    "Compact mode on.\n\r", 
    "Compact mode off.\n\r" 
  },
  {
    "You are now deaf to tells.\n\r",
    "You can now hear tells.\n\r", 
    "You are deaf to tells.\n\r",
    "You can hear tells.\n\r"
  }, 
  { 
    "You can now hear narrates.\n\r", 
    "You are now deaf to narrates.\n\r",
    "You can hear narrates.\n\r", 
    "You are deaf to narrates.\n\r" 
  },
  { 
    "You can now hear chat.\n\r", 
    "You are now deaf to chat.\n\r",
    "You can hear chat.\n\r", 
    "You are deaf to chat.\n\r" 
  },
  { 
    "You can now hear the wiz-channel.\n\r", 
    "You are now deaf to the wiz-channel.\n\r",
    "You can hear the wiz-channel.\n\r", 
    "You are deaf to the wiz-channel.\n\r" 
  },
  {
    "You will now see the room flags.\n\r",
    "You will no longer see the room flags.\n\r", 
    "You see the room flags.\n\r", 
    "You don't see the room flags.\n\r" 
  },
  { 
    "Nohassle now enabled.\n\r", 
    "Nohassle now disabled.\n\r",
    "Nohassle enabled.\n\r", 
    "Nohassle disabled.\n\r" 
  },
  { 
    "Holylight mode is now on.\n\r",
    "Holylight mode is now off.\n\r",
    "Holylight mode is on.\n\r",
    "Holylight mode is off.\n\r" 
  },
  { 
    "Nameserver_is_slow changed to NO; IP addresses will now be resolved.\n\r",
    "Nameserver_is_slow changed to YES; sitenames will no longer be resolved.\n\r",
    "Nameserver_is_slow is set to NO; IP addresses will be resolved.\n\r",
    "Nameserver_is_slow is set to YES; sitenames will not be resolved.\n\r"
  },
  { 
    "Line wrapping is now on.\n\r",
    "Line wrapping is now off.\n\r",
    "Line wrapping is on.\n\r",
    "Line wrapping is off.\n\r"
  },
  {
    "Autoexit mode is now on.\n\r",
    "Autoexit mode is now off.\n\r",
    "Autoexit mode is on.\n\r",
    "Autoexit mode is off.\n\r" 
  },
  { 
    "AutoMental mode is now on.\n\r",
    "AutoMental mode is now off.\n\r",
    "AutoMental mode is on.\n\r",
    "AutoMental mode is off.\n\r" 
  },
  {
    "Incognito mode is now on.\n\r",
    "Incognito mode is now off.\n\r",
    "Incognito mode is on.\n\r",
    "Incognito mode is off.\n\r" 
  },
  {
    "You can now hear singing.\n\r", 
    "You are now deaf to singing.\n\r",
    "You can hear singing.\n\r", 
    "You are deaf to singing.\n\r" 
  },
  {
    "Prompt is now on.\n\r",
    "Prompt is now off.\n\r",
    "Prompt is on.\n\r",
    "Prompt is off.\n\r" 
  },
  { 
    "You will now attempt to swim if needed.\r\n",
    "You will no longer attempt to swim.\r\n",
    "You swim when needed.\r\n",
    "You refuse to swim.\r\n"
  },
  {
    "You will now use the latin-1 character set.\r\n",
    "You will no longer use the latin-1 character set.\r\n",
    "You are currently using the latin-1 character set.\r\n",
    "You are currently using the standard character set.\r\n"
  },
  {
    "You will now see a spinner during skill and spell delays.\r\n",
    "You will no longer see a spinner during skill and spell delays.\r\n",
    "You currently see a spinner during skill and spell delays.\r\n",
    "You currently do not see a spinner during skill and spell delays.\r\n"
  }
};

ACMD(do_gen_tog)
{
  long result;
  char str[10];
  int len, mod, tmp;
  
  extern int nameserver_is_slow;

  if(IS_NPC(ch)) 
    return;
  
  len = strlen(argument);
  mod = 0;
  
  if(len) {
    for(tmp = 0; tmp < len && tmp < 9; tmp++)
      str[tmp]=tolower(argument[tmp]);
    str[tmp] = 0;
    len = tmp;
    if(!strncmp("on", str, len)) {
      mod = 1;
      flag_modify = flag_on;
    }
    if(!strncmp("off", str, len)) {
      mod = 2;
      flag_modify = flag_off;
    }
    if(!strncmp("toggle", str, len)) {
      mod = 3;
      flag_modify = flag_toggle;
    }
    if(!mod) {
      send_to_char("Options are on | off | toggle.\n\r",ch);
      return; 
    }
  }
  else {
    mod = 0;
    flag_modify = flag_void;
  }

  switch(subcmd) {
  case SCMD_SPINNER:
    result = flag_modify(ch, PRF_SPINNER, tog_messages[21], 0);
    break;

  case SCMD_LATIN1:
    result = flag_modify(ch, PRF_LATIN1, tog_messages[20], 0);
    break;

  case SCMD_NOSUMMON:
    result = flag_modify(ch, PRF_SUMMONABLE, tog_messages[0], 0); 
    break;

  case SCMD_ECHO: 
    result = flag_modify(ch, PRF_ECHO, tog_messages[1], 0); 
    break;

  case SCMD_BRIEF:
    result = flag_modify(ch, PRF_BRIEF, tog_messages[2], 0); 
    break;

  case SCMD_SPAM:
    result = flag_modify(ch, PRF_SPAM, tog_messages[3], 0); 
    break;

  case SCMD_COMPACT:
    result = flag_modify(ch, PRF_COMPACT, tog_messages[4], 0); 
    break;

  case SCMD_NOTELL:
    result = flag_modify(ch, PRF_NOTELL, tog_messages[5], 0); 
    break;

  case SCMD_NARRATE:
    result = flag_modify(ch, PRF_NARRATE, tog_messages[6], 0);
    break;

  case SCMD_CHAT:
    result = flag_modify(ch, PRF_CHAT, tog_messages[7], 0); 
    break;

  case SCMD_INCOGNITO:
    result = flag_modify(ch, PLR_INCOGNITO, tog_messages[16], 1); 
    break;

   case SCMD_SING:
     result = flag_modify(ch, PRF_SING, tog_messages[17], 0); 
     break;

  case SCMD_SETPROMPT:
    result = flag_modify(ch, PRF_PROMPT, tog_messages[18], 0); 
    break;

  case SCMD_WIZ:
    result = flag_modify(ch, PRF_WIZ, tog_messages[8], 0);
    break;

  case SCMD_SWIM: 
    result = flag_modify(ch, PRF_SWIM, tog_messages[19], 0); 
    break;

  case SCMD_ROOMFLAGS: 
    result = flag_modify(ch, PRF_ROOMFLAGS, tog_messages[9], 0); 
    break;

  case SCMD_NOHASSLE:
    result = flag_modify(ch, PRF_NOHASSLE, tog_messages[10], 0); 
    break;

  case SCMD_HOLYLIGHT:
    result = flag_modify(ch, PRF_HOLYLIGHT, tog_messages[11], 0); 
    break;

  case SCMD_SLOWNS:
    result = (nameserver_is_slow = !nameserver_is_slow); 
    if(!result)  
      send_to_char(tog_messages[12][2],ch);
    else 
      send_to_char(tog_messages[12][3],ch); 
    break;

  case SCMD_WRAP:
    result = flag_modify(ch, PRF_WRAP, tog_messages[13], 0); 
    break;

  case SCMD_AUTOEXIT:
    result = flag_modify(ch, PRF_AUTOEX, tog_messages[14], 0); 
    break;

  case SCMD_MENTAL:
    result = flag_modify(ch, PRF_MENTAL, tog_messages[15], 0); 
    break;

  default:
    log("SYSERR: Unknown subcmd in do_gen_toggle");
    return;
    break;
  }
}



extern char *tactics[];

ACMD(do_tactics)
{
  char *s1 = "You are presently employing";
  char *s2 = "You are now employing";
  char *s;
  int tmp, len;
  
  if(!*argument)
    s = s1;
  else {
    s = s2;
    len = strlen(argument);
    
    for(tmp=0; tactics[tmp][0]!='\n'; tmp ++)
      if(!strncmp(tactics[tmp],argument,len)) 
	break;
    
    if(tactics[tmp][0] == '\n') {
      sprintf(buf, "Possible tactics are:\n\r   ");
      for(tmp=0; tactics[tmp][0]!='\n'; tmp ++) {
	strcat(buf, tactics[tmp]);
        strcat(buf, " tactics.");
	strcat(buf, "\n\r    ");
      }
      send_to_char(buf,ch);
      return;
    }
    if((GET_TACTICS(ch) == TACTICS_BERSERK))
      if(number(-20,100) > GET_RAW_SKILL(ch,SKILL_BERSERK)) {
	send_to_char("You failed to cool down.\n\r",ch);
	return;
      }
    
    switch(tmp) {
    case 0: 
      SET_TACTICS(ch,TACTICS_DEFENSIVE);
      break;

    case 1:
      SET_TACTICS(ch,TACTICS_CAREFUL);
      break;

    case 2:
      SET_TACTICS(ch,TACTICS_NORMAL);
      break;

    case 3:
      SET_TACTICS(ch,TACTICS_AGGRESSIVE);
      break;

    case 4: 
      SET_TACTICS(ch,TACTICS_BERSERK);
      break;

    default:
      SET_TACTICS(ch,99);
      break;
    }
  }

  switch(GET_TACTICS(ch)) {
  case TACTICS_DEFENSIVE:
    sprintf(buf,"%s %s tactics.\n\r", s, tactics[0]);
    break;
    
  case TACTICS_CAREFUL: 
    sprintf(buf,"%s %s tactics.\n\r",s,tactics[1]);
    break;
    
  case TACTICS_NORMAL:
    sprintf(buf,"%s %s tactics.\n\r",s,tactics[2]);
    break;
    
  case TACTICS_AGGRESSIVE:
    sprintf(buf,"%s %s tactics.\n\r",s,tactics[3]);
    break;
    
  case TACTICS_BERSERK:
    sprintf(buf,"%s %s tactics.\n\r",s,tactics[4]);
    break;
    
  default: sprintf(buf,"%s weird.\n\r",s);
    break;
  }
  send_to_char(buf, ch);
}



ACMD(do_language)
{
  char str[200];
  int len, tmp;

  if(!*argument) {
    sprintf(str,"You are using %s.\n\r",
	    (ch->player.language)? skills[ch->player.language].name :
	    "common language");
    send_to_char(str,ch);
    return;
  }
  len = strlen(argument);

  if(!strncmp(argument,"common language",len)) {
    if(GET_LEVEL(ch) < LEVEL_IMMORT)
      send_to_char("You can't speak in the common tongue.\n\r", ch);
    else {
      ch->player.language = 0;
      send_to_char("You will now use common language.\n\r",ch); 
    }
    return;
  }

  for(tmp = 0; tmp < language_number; tmp++)
    if(!strncmp(skills[language_skills[tmp]].name, argument, len))
      break;

  if(tmp == language_number) {
    strcpy(str, "Possible languages are:\n\r");
    for(tmp = 0; tmp < language_number; tmp++) {
      strcat(str, skills[language_skills[tmp]].name);
      strcat(str, "\n\r");
    }
    send_to_char(str, ch);
    return;
  }

  ch->player.language = language_skills[tmp];
  sprintf(str, "You will now use %s.\n\r", skills[language_skills[tmp]].name);
  send_to_char(str,ch);
  return;
}



char *change_comm[]={
  "prompt",            /* 0 */
  "tactics",
  "nosummon",
  "echo",
  "brief",            
  "spam",              /* 5 */
  "compact",
  "notell",
  "narrate",
  "chat",
  "title",            /* 10 */
  "wimpy",
  "wrap",
  "autoexit",
  "language",
  "description",      /* 15 */
  "incognito",
  "time",
  "sing",
  "mental",         
  "swim",             /* 20 */
  "latin1",
  "spinner",
  "wiz",
  "roomflags",
  "nohassle",
  "holylight",        /* 25 */
  "slowns",
  "\n"
};

ACMD(do_toggle);
ACMD(do_title);
ACMD(do_wimpy);
ACMD(do_spam);

ACMD(do_set)
{
  char command[50];
  char arg[250];
 
  int tmp, tmp2, len;

  if(IS_NPC(ch)) {
    send_to_char("Sorry, NPCs can't do that.\n\r", ch);
    return;
  }

  half_chop(argument, command, arg);

  len = strlen(command);

  if(!*command) {
    do_toggle(ch, "", wtl, 0, 0);
    return;
  }

  for(tmp = 0; change_comm[tmp][0] != '\n'; tmp++)
    if(!strncmp(change_comm[tmp], command, len)) 
      break;
  
  if(change_comm[tmp][0] == '\n') {
  no_set:
    sprintf(buf, "Possible arguments are:\n\r");
    /*
     * Such. a. fucking. hack.  This doesn't even really work: slowns
     * is implementor-only, as may be the case for other future
     * options.  We really need a generic type of list that can deal
     * with permissions based on level.
     */
    tmp2 = (GET_LEVEL(ch) >= LEVEL_IMMORT) ? 27 : 23;
    for(tmp = 0; tmp < tmp2; tmp++) {
      strcat(buf, change_comm[tmp]);
      strcat(buf, ", ");
    }
    buf[strlen(buf)-2] = 0;
    strcat(buf, ".\n\r");
    send_to_char(buf, ch);
    return;
  }

  switch(tmp) {
  case 0: 
    do_gen_tog(ch, arg, wtl, 0, SCMD_SETPROMPT);
    break;

  case 1: 
    do_tactics(ch, arg, wtl, 0, 0);
    break;

  case 2: 
    do_gen_tog(ch, arg, wtl, 0, SCMD_NOSUMMON);
    break;

  case 3:
    do_gen_tog(ch, arg, wtl, 0, SCMD_ECHO);
    break;

  case 4:
    do_gen_tog(ch, arg, wtl, 0, SCMD_BRIEF);       
    break;

  case 5:
    do_gen_tog(ch, arg, wtl, 0, SCMD_SPAM);
    break;

  case 6:
    do_gen_tog(ch, arg, wtl, 0, SCMD_COMPACT);
    break;

  case 7:
    do_gen_tog(ch, arg, wtl, 0, SCMD_NOTELL);
    break;

  case 8:
    do_gen_tog(ch, arg, wtl, 0, SCMD_NARRATE);
    break;

  case 9:
    do_gen_tog(ch, arg, wtl, 0, SCMD_CHAT);
    break;

  case 10: 
    do_title(ch, arg, wtl, 0, 0);
    break;

  case 11:
    do_wimpy(ch, arg, wtl, 0, 0);
    break;

  case 12: 
    do_gen_tog(ch, arg, wtl, 0, SCMD_WRAP);
    break;

  case 13: 
    do_gen_tog(ch, arg, wtl, 0, SCMD_AUTOEXIT);
    break;

  case 14:
    do_language(ch, arg, wtl, 0, 0);
    break;

  case 15: 
    string_add_init(ch->desc, &(ch->player.description));
    break;

  case 16: 
    if(GET_LEVEL(ch) < LEVEL_IMMORT)
      do_gen_tog(ch, arg, wtl, 0, SCMD_INCOGNITO);
    /* if the immortal is incog, then they can use incog to set it off */
    else if(IS_SET(PLR_FLAGS(ch), PLR_INCOGNITO))
      do_gen_tog(ch, arg, wtl, 0, SCMD_INCOGNITO);
    else
      send_to_char("Immortals cannot be incognito.\n\r", ch);
    break;

  case 17: 
    do_gen_tog(ch, arg, wtl, 0, SCMD_TIME);
    break;

  case 18:
    do_gen_tog(ch, arg, wtl, 0, SCMD_SING);
    break;

  case 19:
    do_gen_tog(ch, arg, wtl, 0, SCMD_MENTAL);
    break;

  case 20:
    do_gen_tog(ch, arg, wtl, 0, SCMD_SWIM);
    break;

  case 21:
    do_gen_tog(ch, arg, wtl, 0, SCMD_LATIN1);
    break;

  case 22:
    do_gen_tog(ch, arg, wtl, 0, SCMD_SPINNER);
    break;

  case 23:
    if(GET_LEVEL(ch) >= LEVEL_IMMORT)
      do_gen_tog(ch, arg, wtl, 0, SCMD_WIZ);
    else 
      goto no_set;
    break;

  case 24:
    if(GET_LEVEL(ch) >= LEVEL_IMMORT)
      do_gen_tog(ch, arg, wtl, 0, SCMD_ROOMFLAGS);
    else 
      goto no_set;
    break;

  case 25:
    if(GET_LEVEL(ch) >= LEVEL_IMMORT)
      do_gen_tog(ch, arg, wtl, 0, SCMD_NOHASSLE);
    else
      goto no_set;
    break;

  case 26: 
    if(GET_LEVEL(ch) >= LEVEL_IMMORT) 
      do_gen_tog(ch, arg, wtl, 0, SCMD_HOLYLIGHT);
    else 
      goto no_set;
    break;

  case 27: 
    if(GET_LEVEL(ch) == LEVEL_IMPL)
      do_gen_tog(ch, arg, wtl, 0, SCMD_SLOWNS);
    else if(GET_LEVEL(ch) >= LEVEL_IMMORT)
      send_to_char("This is for implementors to decide.\n\r",ch);
    else
      goto no_set;
    break;

  default:
    send_to_char("Undefined response to this argument.\n\r",ch);
    return;
  }
}



ACMD(do_next)
{
  send_to_char("You see no boards here.\n\r",ch);
  return;
}



ACMD(do_knock)
{
  int tmp, room, oldroom;
  char str[200];
  
  if (IS_SHADOW(ch)){
	send_to_char("You are too insubstantial to do that.\n\r", ch);
	return;
  }

  while((*argument <= ' ') && (*argument))
    argument++;

  if(!*argument) {
    send_to_char("You knocked yourself on your head.\n\r", ch);
    act("$n knocked $mself on $s head.\n\r", FALSE, ch, 0, 0, TO_ROOM);
    return;
  }

  for(tmp = 0; tmp < NUM_OF_DIRS; tmp++) {
    if(EXIT(ch, tmp))
      if(!strcmp(argument, EXIT(ch, tmp)->keyword)) {
	if(IS_SET(EXIT(ch, tmp)->exit_info, EX_ISDOOR)) {
	  sprintf(str, "You knocked on the %s.\n", EXIT(ch, tmp)->keyword);
	  send_to_char(str, ch);
	  sprintf(str, "$n knocked on the %s.\n", EXIT(ch, tmp)->keyword);
	  act(str, FALSE, ch, 0, 0, TO_ROOM);

	  room = EXIT(ch, tmp)->to_room;
	  if(room != NOWHERE) {
	    oldroom = ch->in_room;
	    ch->in_room = room;
	    if(EXIT(ch, rev_dir[tmp])) {
	      if(IS_SET(EXIT(ch,rev_dir[tmp])->exit_info,EX_ISDOOR) &&
		 EXIT(ch,rev_dir[tmp])->keyword) {
		sprintf(str,"You hear a knock on the %s.\n",
			EXIT(ch, rev_dir[tmp])->keyword);
		act(str, FALSE, ch, 0, 0, TO_ROOM);
	      }
	      else {
		sprintf(str,"You hear a knock sounding %sward.\n\r",
			dirs[rev_dir[tmp]]);
		act(str, FALSE, ch, 0, 0, TO_ROOM);
	      }
	    }
	    else
	      act("You hear a distinct knocking sound.\n\r",
		  FALSE, ch, 0, 0, TO_ROOM);

	    ch->delay.targ1.type = TARGET_OTHER;
	    ch->delay.targ1.ch_num = rev_dir[tmp];
	    special(ch, CMD_KNOCK, argument, SPECIAL_NONE, 0);
	    ch->in_room = oldroom;
	  }

	  return;
	}
	else {
	  sprintf(str,"You waved your hand %sward.\n", dirs[tmp]);
	  send_to_char(str, ch);
	  sprintf(str,"$n waved $s hand %sward.\n", dirs[tmp]);
	  act(str, FALSE, ch, 0, 0, TO_ROOM);
	}	  
      }
  }
  send_to_char("There is nothing like that here.\n", ch);
}



ACMD(do_jig)
{
  if(IS_NPC(ch) && MOB_FLAGGED(ch, MOB_SPEC)) {
    act("$n tries to jig, but decides against it.", TRUE, ch, 0, 0, TO_ROOM);
    return;
  }
  if(ch->specials.store_prog_number == 7) {
    act("$n stops dancing.", TRUE, ch, 0, 0, TO_ROOM);
    send_to_char("You stop dancing.\n\r",ch);
    ch->specials.store_prog_number = 0;
    return;
  }
  act("$n cheers and starts dancing a merry jig.", TRUE, ch, 0, 0, TO_ROOM);
  send_to_char("To celebrate, you dance a merry jig.\n\r",ch);
  ch->specials.store_prog_number = 7;
}



#define FIRST_BLOCK_PROC 8;

ACMD(do_block)
{
  int tmp, len;
  char str[255];
  
  if(IS_SHADOW(ch)) {
    send_to_char("You are too insubstantial to do that.\n\r", ch);
    return;
  }

  if(IS_NPC(ch) && mob_index[ch->nr].func) {
    send_to_char("You are too busy to do that.\n\r",ch);
    return;
  }

  if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_ORC_FRIEND)) {
    send_to_char("Sorry, you're not big enough for that.\n\r", ch);
    return;
  }

  while((*argument <=' ') && (*argument)) 
    argument++;

  if(!*argument) {
    tmp = ch->specials.store_prog_number - FIRST_BLOCK_PROC;
    
    if((tmp < 0) || (tmp >= NUM_OF_DIRS)) {
      send_to_char("Which way do you want to block?\n\r", ch);
      return;
    }
    else {
      sprintf(buf,"You are blocking the way %s.\n\r", dirs[tmp]);
      send_to_char(buf, ch);
      return;
    }
  }
  
  one_argument(argument, str);
  len = strlen(str);
  
  for(tmp = 0; tmp < NUM_OF_DIRS; tmp++)
    if(!strncmp(dirs[tmp],str, len)) 
      break;

  if(tmp < NUM_OF_DIRS) { 
    ch->specials.store_prog_number = tmp + FIRST_BLOCK_PROC;

    sprintf(buf,"You start blocking the way %s.\n\r",dirs[tmp]);
    send_to_char(buf, ch);
    sprintf(buf,"$n starts blocking the way %s.",dirs[tmp]);
    act(buf, TRUE, ch, 0, 0, TO_ROOM);
    return;
  }
  
  if(!strncmp(str, "none", len)) {
    tmp = ch->specials.store_prog_number - FIRST_BLOCK_PROC;
    if((tmp >= 0) & (tmp < NUM_OF_DIRS)){
      ch->specials.store_prog_number = 0;    
      sprintf(buf,"You stop blocking the way %s.\n\r",dirs[tmp]);
      send_to_char(buf, ch);
      sprintf(buf,"$n stops blocking the way %s.",dirs[tmp]);
      act(buf, TRUE, ch, 0, 0, TO_ROOM);
    }
    else send_to_char("You are not blocking anything.\n\r",ch);
  }
  else
    send_to_char("You would want to block some direction, or none.\n\r",ch);

  return;
}



ACMD(do_specialize)
{
  int tmp, len;

  extern char *specialize_name[];
  extern int num_of_specializations;


  if(GET_LEVEL(ch) < 20) {
    send_to_char("You are too young to specialize.\n\r", ch);
    return;
  }
  
  if(wtl && (wtl->targ1.type == TARGET_TEXT)) {
    tmp = GET_SPEC(ch);
    if(tmp && (tmp < num_of_specializations)) {
      sprintf(buf, "You are already specialized in %s.\n\r",
	      specialize_name[tmp]);
      send_to_char(buf, ch);
      return;
    }

    if(!(len = strlen(wtl->targ1.ptr.text->text)))
      return;

    for(tmp = 1; tmp < num_of_specializations; tmp++)
      if(!strncmp(specialize_name[tmp], wtl->targ1.ptr.text->text, len)) 
	break;

    if(GET_RACE(ch) == RACE_ORC && tmp == PLRSPEC_GRDN) {
		send_to_char("Snagas can't specialize in guardian!\n\r", ch);
		return;
	}
	
    if(tmp < num_of_specializations) {
      SET_SPEC(ch, tmp);
      sprintf(buf, "You are now specialized in %s.\n\r", specialize_name[tmp]);
      send_to_char(buf, ch);
      return;
    }
  }

  tmp = GET_SPEC(ch);
  if(!tmp || (tmp >= num_of_specializations)) {
    strcpy(buf, "You can specialize in ");
    for(tmp = 1; tmp < num_of_specializations; tmp++) {
      strcat(buf, specialize_name[tmp]);
      strcat(buf, ", ");
    }
    buf[strlen(buf)-2] = 0;
    strcat(buf,".\n\r");
    send_to_char(buf, ch);
  }
  else {
    sprintf(buf, "You are specialized in %s.\n\r", specialize_name[tmp]);
    send_to_char(buf, ch);
    return;
  }
}



ACMD(do_fish) 
{
  int fish = 7230;
  struct obj_data *tmpobj;
  struct room_data *cur_room;

  if (IS_SHADOW(ch)){
	send_to_char("You are too insubstantial to do that.\n\r", ch);
	return;
  }

  sh_int percent, move_use;
  percent = GET_SKILL(ch, SKILL_GATHER_FOOD);
  move_use = 25 - percent/5;

  cur_room = &world[ch->in_room];

  fish = real_object(fish);
  tmpobj = read_object(fish, REAL);

  switch(subcmd) {
  case -1:
    abort_delay(ch);
    return;

  case 0:
    if(GET_MOVE(ch) < move_use) {
      send_to_char("You are too tired to fish right now.\n\r",ch);
      return;
    }

    if(cur_room->sector_type != SECT_WATER_NOSWIM) {
      send_to_char("You can't fish here.\n\r", ch);
      return;
    }
    
    if(!ch->equipment[HOLD] || !isname("fishing", ch->equipment[HOLD]->name)) {
      send_to_char("You need to hold a fishing pole to fish.\n\r", ch);
      return;
    }

    send_to_char("You cast your line into the water and begin to wait.\n\r",
		 ch);

    WAIT_STATE_FULL(ch, MIN(20,44 - GET_SKILL(ch,SKILL_GATHER_FOOD)/5),
		    CMD_FISH, 1, 30, 0, 0, 0, AFF_WAITING | AFF_WAITWHEEL,
		    TARGET_NONE);
    break;

  case 1:
    move_use = 25 - percent/5;
    if(GET_MOVE(ch) < move_use) {
	  send_to_char("You are too tired to fish right now.\n\r",ch);
	  return; }

    GET_MOVE(ch) -= move_use;

    if(number(0, 115) > percent)
      send_to_char ("You give up, having failed to catch anything.\n\r", ch); 
    
    else if(number(0, 125) <= percent) {
      obj_to_char(tmpobj, ch);
      act("$n catches $p.", TRUE, ch, tmpobj, 0, TO_ROOM);
      act("You catch $p.", TRUE, ch, tmpobj, 0, TO_CHAR); 
    }

    abort_delay(ch);
  }
}
    
  
 
