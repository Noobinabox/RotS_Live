#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "platdef.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpre.h"
#include "db.h"
#include "spells.h"
#include "limits.h"
#include "color.h"

#include "char_utils.h"
#include "object_utils.h"
#include "big_brother.h"
#include "char_utils_combat.h"
#include <algorithm>

const int MIN_SAFE_STAT = 3;

extern void remember(struct char_data *ch, struct char_data *victim);
extern void check_break_prep(struct char_data *);
extern struct room_data world;
extern struct message_list fight_messages[MAX_MESSAGES];
extern struct obj_data *object_list;
extern struct char_data * waiting_list; /* in db.cpp */
extern int immort_start_room;
extern int mortal_start_room[];
extern int r_mortal_start_room[];
extern int r_immort_start_room;
extern struct index_data *mob_index;
extern int armor_absorb(struct obj_data *obj);

void group_gain(struct char_data *, struct char_data *);
char saves_power(struct char_data *, sh_int power, sh_int bonus);
int check_overkill(struct char_data *);
int check_hallucinate(struct char_data *, struct char_data *);

const char* const stat_word[] = {
  "strength",
  "intelligence",
  "will",
  "dexterity",
  "constitution",
  "learning",
  "concentration"
};

namespace
{
	//============================================================================
	bool is_mental_stat(int stat_num)
	{
		return stat_num == 1 || stat_num == 2 || stat_num > 4;
	}
} // end anonymous helper namespace

//============================================================================
bool check_mind_block(char_data* character, char_data* attacker, int amount, int stat_num)
{
	affected_type* mind_block_affect = affected_by_spell(character, SPELL_MIND_BLOCK);
	if (is_mental_stat(stat_num) && affected_by_spell(character, SPELL_MIND_BLOCK))
	{
		if(0.20 > number())
		{
			mind_block_affect = affected_by_spell(character, SPELL_MIND_BLOCK);
			mind_block_affect->duration -= amount;
			if (mind_block_affect->duration <= 0) 
			{
				affect_from_char(character, SPELL_MIND_BLOCK);
				send_to_char("Your mind block resists for one final time, but is broken!\n\r", character);
				act("You break $N's mind block!\n\r", FALSE, attacker, 0, character, TO_CHAR);
			}
			else 
			{
				send_to_char("Your attempts are blocked by a powerful spell.\n\r", attacker);
				send_to_char("Your mind block is holding strong.\n\r", character);
			}
			return false;
		}
	}

	return true;
}



ACMD(do_flee);
void report_char_mentals(struct char_data *, char *, int);
void appear(struct char_data *);
int damage_stat(struct char_data *, struct char_data *, int ,int);

void do_mental(struct char_data *ch, char *argument, struct waiting_type *wtl, int cmd, int subcmd)
{
  char_data * victim;
  int result, tmp, damg, not_ready;

  /* Is this a peace room? */
  if(IS_SET(world[ch->in_room].room_flags, PEACEROOM)) {
    send_to_char("A peaceful feeling overwhelms you, and you "
		 "cannot bring yourself to attack.\n\r", ch);
    return;
  }

  if(GET_MENTAL_DELAY(ch) > 1)
    not_ready = 1;
  else
    not_ready = 0;
  
  victim = 0;
  if(argument) 
    while(*argument && (*argument <= ' ')) 
      argument++;

  if(wtl) {
    if((wtl->targ1.type == TARGET_CHAR) && char_exists(wtl->targ1.ch_num))
      victim = wtl->targ1.ptr.ch;
    else if(wtl->targ1.type == TARGET_TEXT)
      victim = get_char_room_vis(ch, wtl->targ1.ptr.text->text);
    else if(wtl->targ1.type == TARGET_NONE)
      victim = ch->specials.fighting;
  }
  else if(argument && *argument)
    victim = get_char_room_vis(ch, argument);
  else
    victim = ch->specials.fighting;

  if(!victim) {
    send_to_char("Whom do you want to press?\n\r",ch);
    return;
  }
  if(victim == ch) {
    send_to_char("You contemplate yourself for a little while.\n\r",ch);
    return;
  }

  // This isn't technically a curse, but it's close enough.
  game_rules::big_brother& bb_instance = game_rules::big_brother::instance();
  if (!bb_instance.is_target_valid(ch, victim))
  {
	  send_to_char("You feel the Gods looking down upon you, and protecting your target.  Your mind falters.\r\n", ch);
	  return;
  }

  /* Level check, level 9 and below can't kill each other */
  if(!IS_NPC(ch) && GET_LEVEL(ch) < 10 && !IS_NPC(victim) && 
      RACE_GOOD(ch) && RACE_GOOD(victim)) {
    act("The will of the Valar holds your attack in check.\n\r",
	FALSE, ch, 0, victim, TO_CHAR);
    return;
  }

  if(ch->specials.fighting && (ch->specials.fighting != victim) &&
     (ch != victim->specials.fighting)) {
    send_to_char("Your attention is elsewhere already.\n\r", ch);
    return;    
  }
  
  if(victim->in_room != ch->in_room) {
    send_to_char("Your victim disappeared!\n\r", ch);
    stop_fighting(ch);
    return;
  }

  if(not_ready) {
    send_to_char("Your mind is not ready yet.\n\r",ch);
    return;
  }
  
  if(!ch->specials.fighting && IS_MENTAL(ch)){
    set_fighting(ch, victim);
    WAIT_STATE(ch, 2); /* HVORFOR DET?? */
  }

  set_mental_delay(ch, PULSE_MENTAL_FIGHT);

  if(!(IS_SHADOW(victim)) && GET_BODYTYPE(victim) != 1) {
    act("You cannot fathom $N's mind.\n\r",FALSE, ch, 0, victim, TO_CHAR);
    return;
  }

  /* Check hallucination */
  if(!check_hallucinate(ch, victim))
    return;

  /* checking the perception first */
  if(GET_PERCEPTION(ch) * GET_PERCEPTION(victim) < number(1, 10000)){
    act("You couldn't reach $S mind.",FALSE, ch, 0, victim, TO_CHAR);
    return;
  }
  
  /* mind_block = less effective mental fighting */
  if(affected_by_spell(ch, SPELL_MIND_BLOCK))
    if(number(0, 100) < 75){
      act("Your mind block prevents you from damaging $N.\n\r",
	  FALSE, ch, 0, victim, TO_CHAR);
      return;
    }

  /* rolling the Will now */
  result = 0;
  damg = 0;
  if(!saves_power(victim, (GET_WILLPOWER(ch) +
			   ((IS_AFFECTED(ch, AFF_CONCENTRATION)) ?
			    GET_PROF_LEVEL(PROF_CLERIC, ch) / 2 : 0)),
		  ((IS_AFFECTED(victim, AFF_CONCENTRATION)) ?
		   GET_PROF_LEVEL(PROF_CLERIC, victim)/2 : 0))) damg++;
  if(!saves_power(victim, (GET_WILLPOWER(ch) +
                           ((IS_AFFECTED(ch, AFF_CONCENTRATION)) ?
                            GET_PROF_LEVEL(PROF_CLERIC, ch) / 2 : 0)),
                  ((IS_AFFECTED(victim, AFF_CONCENTRATION)) ?
                   GET_PROF_LEVEL(PROF_CLERIC, victim)/2 : 0))) damg++;
  
  /* Successful hit */
  if(damg) {
    tmp = number(0, 6);
    if(tmp == 6)  /* Hitting concentration */
      GET_SPIRIT(ch) += damg;
    
    if(!(check_mind_block(victim, ch, damg, tmp))) 
      return;
    
    sprintf(buf, "$CHYou force your Will against $N's %s!", stat_word[tmp]);
    act(buf, FALSE, ch, NULL, victim, TO_CHAR);

    sprintf(buf, "$CD$n forces $s Will against your %s!", stat_word[tmp]);
    act(buf, FALSE, ch, NULL, victim, TO_VICT);
    
    sprintf(buf,"$n forces $s Will against $N's %s!", stat_word[tmp]);
    act(buf, TRUE, ch, 0, victim, TO_NOTVICT);
    
    result = damage_stat(ch, victim, tmp, damg);
    if(!result && ((IS_AFFECTED(victim, AFF_WAITWHEEL) && 
		    (GET_WAIT_PRIORITY(victim) <= 40))))
      break_spell(victim);
    
    if(!result && victim->delay.cmd == CMD_PREPARE &&
       (victim->delay.targ1.type==TARGET_IGNORE)) {
      send_to_char("Your preparations were ruined.\n\r", victim);
      victim->delay.targ1.cleanup();
      abort_delay(victim);
    }
    
    /* Check to see if player attacking memory mob - Errent */
    if(IS_NPC(victim) && IS_SET(victim->specials2.act, MOB_MEMORY) &&
       !IS_NPC(ch) && (GET_LEVEL(ch) < LEVEL_IMMORT))
      remember(victim, ch);
  }
  else {  /* failure */
    if(!number(0, 3)) {  /* Did he damage himself? */
      act("You try your Will against $N's mind, but lose control!",
	  FALSE, ch, 0, victim, TO_CHAR);
      act("$n tries $s Will against your mind, but loses control!", 
	  FALSE, ch, 0, victim, TO_VICT);
      act("$n tries $s Will against $N's mind, but loses control!", 
	  TRUE, ch, 0, victim, TO_NOTVICT);
      result = damage_stat((ch->specials.fighting) ? ch->specials.fighting :
			   ch, ch, number(0, 6), 2);

	  if(!result)
	    check_break_prep(ch);

    }
    else{
      act("You try your Will against $N's mind, but $E resists!",
	  FALSE, ch, 0, victim, TO_CHAR);
      act("$n tries $s Will against your mind, but you resist!", 
	  FALSE, ch, 0, victim, TO_VICT);
      act("$n tries $s Will against $N's mind, but $E resists!", 
	  TRUE, ch, 0, victim, TO_NOTVICT);
    }
    if(!result && !victim->specials.fighting && IS_MENTAL(victim))
      set_fighting(victim, ch);
  }
}



/*
 * returns 1 if the victim dies, else 0 
 */
void die(struct char_data *ch, struct char_data *killer, int attacktype);

int damage_stat(struct char_data *killer, struct char_data *ch, int stat_num, int amount)
{
  int result, i, must_flee, would_flee;
  struct waiting_type tmpwtl;
  
  if(ch->specials.fighting != killer) {
    tmpwtl.targ1.ptr.ch = ch;
    tmpwtl.targ1.type = TARGET_CHAR;
    tmpwtl.targ2.ptr.other = 0;
    tmpwtl.targ2.type = TARGET_NONE;
    i = special(killer, 0, "", SPECIAL_DAMAGE, &tmpwtl);
    if(i) {
      if(killer->specials.fighting == ch) 
	stop_fighting(killer);
      return 0;
    }
  }

  // This isn't technically a curse, but it's close enough.
  game_rules::big_brother& bb_instance = game_rules::big_brother::instance();
  if (!bb_instance.is_target_valid(killer, ch))
  {
	  send_to_char("You feel the Gods looking down upon you, and protecting your target.  Your mind falters.\r\n", ch);
	  return 0;
  }

  if(IS_AFFECTED(killer, AFF_SANCTUARY))
    appear(killer);

  if(!(check_mind_block(ch, killer, amount, stat_num)))
    return 0;
  
  if(!check_sanctuary(ch, killer))
    return 0;
  
  /* Give anger */
  utils::on_attacked_character(killer, ch);
  
  if(IS_NPC(ch) && (ch->specials.attacked_level < GET_LEVELB(killer)))
    ch->specials.attacked_level = GET_LEVELB(killer);
  
  if(!(killer->specials.fighting))
    set_fighting(killer, ch);
  
  /*
   * would_flee should not be dependant on a wimpy level, because that is for
   * physical attacks.  ugh.  -- Seether  
   */
  would_flee = (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_WIMPY)) || !IS_NPC(ch);
  must_flee = 0;

  switch(stat_num) {
  case 0: 
    SET_STR(ch, GET_STR(ch) - amount);
    if(GET_STR(ch) <= 0) 
      result = 1;
    else 
      result = 0;
    break;

  case 1:
    GET_INT(ch) -= amount;
    if(GET_INT(ch) <= 0) 
      result = 1;
    else
      result = 0;
    if(GET_INT(ch) < MIN_SAFE_STAT)
      must_flee = 1;
    break;

  case 2: 
    GET_WILL(ch) -= amount;
    if(GET_WILL(ch) <= 0) 
      result = 1;
    else
      result = 0;
    if(GET_WILL(ch) < MIN_SAFE_STAT) 
      must_flee = 1;
    break;

  case 3:
    GET_DEX(ch) -= amount;
    if(GET_DEX(ch) <= 0) 
      result = 1;
    else
      result = 0;
    if(GET_DEX(ch) < MIN_SAFE_STAT)
      must_flee = 1;
    break;

  case 4:
    GET_CON(ch) -= amount;
    if(GET_CON(ch) <= 0) 
      result = 1;
    else
      result = 0;
    if(GET_CON(ch) < MIN_SAFE_STAT) 
      must_flee = 1;
    break;

  case 5:
    GET_LEA(ch) -= amount;
    if(GET_LEA(ch) <= 0) 
      result = 1;
    else
      result = 0;
    if(GET_LEA(ch) < MIN_SAFE_STAT) 
      must_flee = 1;
    break;

  case 6:
    if(GET_MENTAL_DELAY(ch) >= 0) 
      set_mental_delay(ch, 2);
    else
      set_mental_delay(ch,-1 - GET_MENTAL_DELAY(ch));
    result = 0;
    break;

  default:
    send_to_char("A non-existent stat was attacked; please report this.\n\r",
		 ch);
    return 0;
  }
  if(result)
    die(ch, killer, 0);
  else {
    affect_total(ch);
    
    if(!ch->specials.fighting)
      set_fighting(ch, killer);
    else if((ch->specials.fighting != killer) && IS_NPC(ch) && 
	    !number(0, 15)) {
      ch->specials.fighting = killer;
      act("You turn to fight $N.", FALSE, ch, 0, killer, TO_CHAR);
    }
    if(would_flee) {
      must_flee = 0;
      if(GET_STR(ch) < MIN_SAFE_STAT)
	must_flee = 1;
      if(GET_INT(ch) < MIN_SAFE_STAT)
	must_flee = 1;
      if(GET_WILL(ch) < MIN_SAFE_STAT)
	must_flee = 1;
      if(GET_DEX(ch) < MIN_SAFE_STAT)
	must_flee = 1;
      if(GET_CON(ch) < MIN_SAFE_STAT)
	must_flee = 1;
      if(GET_LEA(ch) < MIN_SAFE_STAT)
	must_flee = 1;

      if(must_flee)
	do_flee(ch, "", 0, 0, 0);
    }
    if(IS_AFFECTED(ch, AFF_WAITWHEEL) && (GET_WAIT_PRIORITY(ch) <= 40))
      break_spell(ch);
  }
  
  return result;
}



//============================================================================
// Returns the amount of stats actually restored 
//============================================================================
int restore_stat(char_data* character, int stat_num, int amount)
{
	switch (stat_num) 
	{
	case 0:
	{
		int max_amount = character->abilities.str - character->tmpabilities.str;
		amount = std::min(amount, max_amount);
		character->tmpabilities.str += amount;
		return amount;
	}
	case 1:
	{
		int max_amount = character->abilities.intel - character->tmpabilities.intel;
		amount = std::min(amount, max_amount);
		character->tmpabilities.intel += amount;
		return amount;
	}
	case 2:
	{
		int max_amount = character->abilities.wil - character->tmpabilities.wil;
		amount = std::min(amount, max_amount);
		character->tmpabilities.wil += amount;
		return amount;
	}
	case 3:
	{
		int max_amount = character->abilities.dex - character->tmpabilities.dex;
		amount = std::min(amount, max_amount);
		character->tmpabilities.dex += amount;
		return amount;
	}
	case 4:
	{
		int max_amount = character->abilities.con - character->tmpabilities.con;
		amount = std::min(amount, max_amount);
		character->tmpabilities.con += amount;
		return amount;
	}
	case 5:
	{
		int max_amount = character->abilities.lea - character->tmpabilities.lea;
		amount = std::min(amount, max_amount);
		character->tmpabilities.lea += amount;
		return amount;
	}
	default:
	{
		return 0;
	}
	}

	return 0;
}



ACMD(do_concentrate)
{
  char str[255];
  int i, tmp, extra;
  struct char_data *victim;
  int stat_damage[6];
  
  if(subcmd == -1) {
    tmp = GET_MENTAL_DELAY(ch);
    REMOVE_BIT(ch->specials.affected_by, AFF_CONCENTRATION);
    
    if(ch->specials.fighting) {
      victim = ch->specials.fighting;
      extra = (-GET_MENTAL_DELAY(ch) + number(0, PULSE_MENTAL_FIGHT-1)) /
	PULSE_MENTAL_FIGHT;
      sprintf(str, "You attack with extra will power! (%d)\n\r", extra);
      send_to_char(str,ch);
      act("$n lashes out his power!", FALSE, ch, 0, 0, TO_ROOM);
      
      extra = extra * GET_PERCEPTION(victim) / 100;
      
      for(i = 0; i < 6; i++)
	stat_damage[i] = 0;
      while(extra > 0 && !saves_power(victim, GET_WILLPOWER(ch) + extra, 0)) {
	stat_damage[number(0,5)]++;
	extra -=2;
      }
      for(i = 0; i < 6; i++)
	if(stat_damage[i])
	  if(damage_stat(ch, victim, i, stat_damage[i]))
	    break;
    }
    else {
      send_to_char("You release your concentration.\n\r",ch);
      act("$n stops concentrating.",FALSE, ch, 0, 0, TO_ROOM);
    }
    set_mental_delay(ch, 1 - tmp);
    return;
  }
  else {
    if(GET_MENTAL_DELAY(ch) > PULSE_MENTAL_FIGHT) {
      send_to_char("Your mind is not ready yet.\n\r", ch);
      return;
    }
    set_mental_delay(ch, -1-GET_MENTAL_DELAY(ch));
    ch->delay.targ1.type = TARGET_NONE;
    ch->delay.targ2.type = TARGET_NONE;
    WAIT_STATE_BRIEF(ch, -1, CMD_CONCENTRATE, -1, 50, 
		     AFF_CONCENTRATION | AFF_WAITING | AFF_WAITWHEEL);
    send_to_char("You start focusing your mind.\n\r", ch);
    act("$n starts concentrating.", FALSE, ch, 0, 0, TO_ROOM);
  }
}

//============================================================================
// Deals willpower based damage to the victim's stat.  If the attack was successful,
// this function returns true.  Otherwise it weapons false.
//============================================================================
bool weapon_willpower_damage(char_data* attacker, char_data* victim)
{
	obj_data* weapon = attacker->equipment[WIELD];
	if (!weapon)
		return false;

	if (!utils::is_object_stat(*weapon, ITEM_WILLPOWER))
		return false;

	if (utils::is_shadow(*attacker))
		return false;
	
	int combined_percep = utils::get_perception(*attacker) * utils::get_perception(*victim);
	if (combined_percep < number(1, 10000))
	{
		//TODO(drelidan):  Add nifty message here.
		return false;
	}

	// Weapon level scales on the mystic level of the defender.  Interesting.
	int victim_cleric_level = utils::get_prof_level(PROF_CLERIC, *victim);
	int weapon_level = weapon->obj_flags.level + victim_cleric_level;
	int save_bonus = 0;
	if (utils::is_affected_by(*victim, AFF_CONCENTRATION))
	{
		save_bonus = victim_cleric_level / 2;
	}

	if (!saves_power(victim, weapon_level, save_bonus)) 
	{
		act("$CD$n's weapon burns you to your soul!", FALSE, attacker, NULL, victim, TO_VICT);
		act("$CHYour weapon burns $N to $S soul!\r\n", FALSE, attacker, NULL, victim, TO_CHAR);

		int stat_targeted = number(0, 5);
		int rolled_damage = std::min(number(0, (int)attacker->points.willpower / 10), 4);
		int stat_damage = std::max(rolled_damage, 1);
		damage_stat(attacker, victim, stat_targeted, stat_damage);

		return true;
	}

	return false;
}
