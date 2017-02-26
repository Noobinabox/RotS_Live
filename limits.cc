/* ************************************************************************
*   File: limits.c                                      Part of CircleMUD *
*  Usage: limits & gain funcs for HMV, exp, hunger/thirst, idle time      *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993 by the Trustees of the Johns Hopkins University     *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "platdef.h"
#include <assert.h>
#include "structs.h"
#include "utils.h"
#include "spells.h"
#include "comm.h"
#include "interpre.h"
#include "db.h"
#include "limits.h"
#include "handler.h"
#include "profs.h"

#include "char_utils.h"
#include "big_brother.h"

extern char *pc_race_types[];

#define READ_TITLE(ch) pc_race_types[GET_RACE(ch)]


extern struct char_data *character_list;
extern struct obj_data *object_list;
extern struct room_data world;
extern int	top_of_world;
extern struct time_info_data time_info;	
extern struct skill_data skills[];
extern char	*spell_wear_off_msg[];
extern struct char_data * fast_update_list;
extern int circle_shutdown;
extern struct char_data * death_waiting_list;

void recalc_skills(struct char_data * ch);

extern void raw_kill(char_data* ch, char_data* killer, int attacktype);
void one_mobile_activity(char_data * ch);

ACMD(do_flee);
char saves_spell(struct char_data *ch, sh_int level, int bonus);
void stop_riding(struct char_data *ch);
char char_perception_check(struct char_data *ch);
int get_sun_level(int room);

/* When age < 15 return the value p0 */
/* When age in 15..29 calculate the line between p1 & p2 */
/* When age in 30..44 calculate the line between p2 & p3 */
/* When age in 45..59 calculate the line between p3 & p4 */
/* When age in 60..79 calculate the line between p4 & p5 */
/* When age >= 80 return the value p6 */
int	graf(int age, int p0, int p1, int p2, int p3, int p4, int p5, int p6)
{

   if (age < 15)
      return(p0);						/* < 15   */
   else if (age <= 29)
      return (int) (p1 + (((age - 15) * (p2 - p1)) / 15));	/* 15..29 */
   else if (age <= 44)
      return (int) (p2 + (((age - 30) * (p3 - p2)) / 15));	/* 30..44 */
   else if (age <= 59)
      return (int) (p3 + (((age - 45) * (p4 - p3)) / 15));	/* 45..59 */
   else if (age <= 79)
      return (int) (p4 + (((age - 60) * (p5 - p4)) / 20));	/* 60..79 */
   else
      return(p6);						/* >= 80 */
}


int xp_to_level(int lvl)
{
	return lvl * lvl * 1500;
}

inline void advance_mini_level(struct char_data *ch)
{
	int i, n;
	i = ++GET_MINI_LEVEL(ch);
	/* add on average 2 HP/level */
	if (GET_MAX_MINI_LEVEL(ch) < GET_MINI_LEVEL(ch))
	{
		if (number(0, 50) == 0)
		{
			ch->constabilities.hit++;
		}
	}

	if ((xp_to_level(GET_LEVEL(ch) + 1) <= GET_EXP(ch)))
	{
		GET_LEVEL(ch)++;
		advance_level(ch);
	}

	for (n = 1; n <= MAX_PROFS; n++)
	{
		if ((i)*GET_PROF_COOF(n, ch) >= 100000 * (GET_PROF_LEVEL(n, ch) + 1))
		{
			if (GET_PROF_COOF(n, ch) * LEVEL_MAX >= 1000 * (GET_PROF_LEVEL(n, ch) + 1))
			{
				advance_level_prof(n, ch);
			}
		}
	}

	GET_MAX_MINI_LEVEL(ch) = std::max(GET_MAX_MINI_LEVEL(ch), GET_MINI_LEVEL(ch));
}

double adjust_regen_for_level(int character_level, double regen_amount)
{
	// No extra regen for characters over level 10.
	if (character_level > 10)
		return regen_amount;

	const double flat_multiplier = 3.0;
	const double level_penalty = 0.2;

	double adjusted_amount = regen_amount * (flat_multiplier - (character_level * level_penalty));
	return adjusted_amount;
}

/* manapoint gain pr. game hour */
int	mana_gain(const char_data* character)
{
	using namespace utils;

	double gain(character->player.level);
	if (is_pc(*character))
	{
		const char_ability_data& abils = character->tmpabilities;
		gain = 8.0 + abils.intel / 2.0 + abils.wil / 5.0;
		gain = gain + get_prof_level(PROF_MAGE, *character) / 5.0 + get_prof_level(PROF_CLERIC, *character) / 5.0;

		switch (character->specials.position) 
		{
		case POSITION_SLEEPING:
			gain *= 2;
			break;
		case POSITION_RESTING:
			gain *= 1.5;
			break;
		case POSITION_SITTING:
			gain *= 1.25;
			break;
		}
	}
	else
	{
		// NPCs that aren't in combat regenerate mana 50% faster to compensate for increased player health/mana regen.
		if (character->specials.fighting == NULL)
		{
			gain *= 1.5;
		}
	}

	if (is_affected_by(*character, AFF_POISON))
	{
		gain *= 0.25;
	}

	if (get_condition(*character, FULL) == 0 || get_condition(*character, THIRST) == 0)
	{
		gain *= 0.25;
	}

	gain = adjust_regen_for_level(character->player.level, gain);

	// Cast back into an integer when we're done.
	return int(gain);
}

/* Hitpoint gain pr. game hour */
int	hit_gain(const char_data* character)
{
	using namespace utils;

	double gain(character->player.level);

	if (is_pc(*character))
	{
		gain = 8.0 + character->tmpabilities.con / 2.0 + get_prof_level(PROF_WARRIOR, *character) / 3.0 + get_prof_level(PROF_RANGER, *character) / 5.0;

		switch (character->specials.position) 
		{
		case POSITION_SLEEPING:
			gain *= 1.5;
			break;
		case POSITION_RESTING:
			gain *= 1.25;
			break;
		case POSITION_SITTING:
			gain *= 1.125;
			break;
		}
	}
	else
	{
		// NPCs that aren't in combat regenerate health 50% faster to compensate for increased player health/mana regen.
		if (character->specials.fighting == NULL)
		{
			gain *= 1.5;
		}
	}

	if (is_affected_by(*character, AFF_POISON))
	{
		gain *= 0.25;
	}

	if (get_condition(*character, FULL) == 0 || get_condition(*character, THIRST) == 0)
	{
		gain *= 0.25;
	}

	gain = adjust_regen_for_level(character->player.level, gain);

	// Cast back into an integer when we're done.
	return int(gain);
}

int	move_gain(const char_data* character)
/* move gain pr. game hour */
{
	using namespace utils;

	const char_ability_data& stats = character->tmpabilities;
	if (is_npc(*character))
	{
		if (is_mob_flagged(*character, MOB_MOUNT))
		{
			return 26 + (character->player.level * 2) + stats.con + stats.dex;
		}
	}

	double gain = 7.0;

	// Animals get move regen bonus
	if (GET_BODYTYPE(character) == 2)
	{
		gain += character->player.level + 10;
	}

	/* Prof/Level calculations */
	gain += (stats.con + stats.dex) / 2.0;

	double expertise_modifier = get_prof_level(PROF_RANGER, *character) / 6.0 + get_raw_knowledge(*character, SKILL_TRAVELLING) / 10.0;
	gain += expertise_modifier;

	/* Skill/Spell calculations */


	/* Position calculations    */
	switch (character->specials.position)
	{
	case POSITION_SLEEPING:
		gain *= 1.75;
		break;
	case POSITION_RESTING:
		gain *= 1.5;
		break;
	case POSITION_SITTING:
		gain *= 1.125;
		break;
	}

	if (is_npc(*character))
	{
		// Tames get double move regen (have to be animals).
		if (affected_by_spell(const_cast<char_data*>(character), SKILL_TAME))
		{
			gain *= 2.0;
			char_data* master = character->master;
			if (master && get_specialization(*master) == PLRSPEC_PETS)
			{
				// If the tamer is animals spec, double move regen again.
				gain *= 2.0;
			}
		}
		return int(gain - 5);
	}
	else
	{
		int race = character->player.race;
		if (race == RACE_WOOD || race == RACE_HIGH)
		{
			gain += 5.0;
		}

		if (is_affected_by(*character, AFF_POISON))
		{
			gain *= 0.25;
		}

		if (get_condition(*character, FULL) == 0 || get_condition(*character, THIRST) == 0)
		{
			gain *= 0.25;
		}

		return int(gain);
	}
}

void set_title(char_data* character)
{
	if (GET_TITLE(character))
		RELEASE(GET_TITLE(character));
	CREATE(GET_TITLE(character), char, strlen(READ_TITLE(character)) + 5);

	sprintf(GET_TITLE(character), "the %s", READ_TITLE(character));
	*(GET_TITLE(character) + 4) = toupper(*(GET_TITLE(character) + 4));
}


void check_autowiz(struct char_data *ch)
{
   char	buf[100];
   extern int	min_wizlist_lev;

   if (GET_LEVEL(ch) >= LEVEL_IMMORT) {
      sprintf(buf, "nice ../bin/autowiz %d %s %d %s %d &", min_wizlist_lev,
         WIZLIST_FILE, LEVEL_IMMORT, IMMLIST_FILE, getpid());
      mudlog("Initiating autowiz.", NRM, LEVEL_IMMORT, FALSE);
      system(buf);
   }
}

/* EXP for a level, is level^2 * 1500 */
/* EXP for a given min_level is xp_to_level(mini_level)/10000 */

/*
 * Gain experience with sanity checking.
 *
 * Add experience to a character.  You can't gain more than 7000
 * experience from any single gain and you cannot lose more than
 * 10,000 experience from any single loss.  Furthermore, characters
 * can only gain experience if they're between levels 1 and
 * LEVEL_IMMORT - 1.
 *
 * NOTE: I can't think of any time when this function is used with
 * a negative gain.  Those cases always use gain_exp_regardless,
 * because I've never seen this 10,000 exp loss minimum come into
 * play.
 */
void
gain_exp(struct char_data *ch, int gain)
{
  if (GET_LEVEL(ch) < 0)
    return;

  /* Don't let LEVEL_IMMORT - 1 chars gain exp, otherwise they'll be imms */
  if (gain > 0 && GET_LEVEL(ch) < LEVEL_IMMORT - 1) {
    gain = MIN(7000, gain);
    gain_exp_regardless(ch, gain);
  }
    
  /* Imms shouldn't lose experience */
  if (gain < 0 && GET_LEVEL(ch) < LEVEL_IMMORT) {
    gain = MAX(-10000, gain);  /* Never lose more than 10000 */
    gain_exp_regardless(ch, gain);
  }
}

/*
 * Add exp to a character regardless of how large or small the
 * experience gain is.  For example, gain_exp_regardless is used
 * when creating new implementors or when a player dies from a
 * mob death.
 */
void
gain_exp_regardless(struct char_data *ch, int gain)
{
  int i, is_altered;
  int hp_lack;

  i = GET_MINI_LEVEL(ch);
  is_altered = 0;
  
  /* NPCs aren't allowed to gain experience */
  if (IS_NPC(ch))
    return;

  if (gain > 0) {	 
    GET_EXP(ch) += gain;

    for (i = GET_MINI_LEVEL(ch);  i*i*3/20 <= GET_EXP(ch); i++) {
      if (i > GET_MINI_LEVEL(ch)) {
        advance_mini_level(ch);
        is_altered = 1;
        GET_MINI_LEVEL(ch) = i;
      }
    }
  }
      
  if (gain < 0) {
    GET_EXP(ch) += gain;
    is_altered = 1;

    /* Each loop loses one level, with tolerance of 20,000 exp */
    while (xp_to_level(GET_LEVEL(ch)) - 20000 > GET_EXP(ch)) {
      GET_LEVEL(ch) = GET_LEVEL(ch) - 1;
      GET_MINI_LEVEL(ch) = 100 * GET_LEVEL(ch);
      SPELLS_TO_LEARN(ch) -= PRACS_PER_LEVEL +
                             GET_LEA_BASE(ch) / LEA_PRAC_FACTOR;

      send_to_char("You lose a level!\n\r", ch);
    }
  }
      
  /* Sanity check: don't let exp go negative */
  if (GET_EXP(ch) < 0)
    GET_EXP(ch) = 0;

  /* If we did anything, recalc hp, mana, moves and other points */
  if (is_altered) {
    hp_lack = GET_MAX_HIT(ch) - GET_HIT(ch);
    affect_total(ch);
    GET_HIT(ch) = MAX(GET_HIT(ch), GET_MAX_HIT(ch) - hp_lack);
  }
}


void	gain_condition(struct char_data *ch, int condition, int value)
{
   char intoxicated;

   if (GET_COND(ch, condition) == -1) /* No change */
      return;
      
   if (IS_SHADOW(ch))
	 return;

   intoxicated = (GET_COND(ch, DRUNK) > 0);

   GET_COND(ch, condition)  += value;

   GET_COND(ch, condition) = MAX(0, GET_COND(ch, condition));
   GET_COND(ch, condition) = MIN(24, GET_COND(ch, condition));

   if (GET_COND(ch, condition) || PLR_FLAGGED(ch, PLR_WRITING))
      return;

   switch (condition) {
   case FULL :
      send_to_char("You are hungry.\n\r", ch);
      return;
   case THIRST :
      send_to_char("You are thirsty.\n\r", ch);
      return;
   case DRUNK :
      if (intoxicated)
	 send_to_char("You are now sober.\n\r", ch);
      return;
   default :
      break;
   }

}

void Crash_extract_objs(obj_data *);

int	check_idling(struct char_data *ch)
{       // returns 1 if the char was extracted, 0 otherwise
  extern int r_mortal_idle_room[];
  int j;

  if((GET_LEVEL(ch) >= LEVEL_GOD) && (ch->desc) && (ch->desc->descriptor))
    {
      (ch->specials.timer)++;
      if(ch->specials.timer > 15)
	SET_BIT(PLR_FLAGS(ch), PLR_ISAFK);
      return 0;
    }
  
  if (++(ch->specials.timer) > 8)
    if (ch->specials.was_in_room == NOWHERE && ch->in_room != NOWHERE) {
      ch->specials.was_in_room = ch->in_room;
      if (ch->specials.fighting) {
		stop_fighting(ch->specials.fighting);
		stop_fighting(ch);
      }
      if (IS_RIDING(ch))
		stop_riding(ch);
      act("$n disappears into the void.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You have been idle, and are pulled into a void.\n\r", ch);
      save_char(ch, NOWHERE, 0);
      Crash_crashsave(ch);
      char_from_room(ch);
      char_to_room(ch, r_mortal_idle_room[GET_RACE(ch)]);
      SET_BIT(PLR_FLAGS(ch), PLR_ISAFK);
    } 
    else if (ch->specials.timer > 28) {
      if (ch && !IS_NPC(ch) && (ch->specials.was_in_room != NOWHERE)){
		if (ch->in_room != NOWHERE)
		  char_from_room(ch);
		char_to_room(ch,  ch->specials.was_in_room);
		save_char(ch, world[ch->specials.was_in_room].number, 0);
		ch->specials.was_in_room = NOWHERE;
      }
      Crash_idlesave(ch);
      sprintf(buf, "%s force-rented and extracted (idle).", GET_NAME(ch));
      mudlog(buf, NRM, LEVEL_GOD, TRUE);

      for (j = 0; j < MAX_WEAR; j++)
		if (ch->equipment[j]){
		  Crash_extract_objs(ch->equipment[j]);
		  ch->equipment[j] = 0;
		}
      Crash_extract_objs(ch->carrying);

      if (ch->desc && ch->desc->descriptor){ 
      	close_socket(ch->desc);
		ch->desc = 0; // was commented out, now put back in by Fingolfin Jan 9
	  }

      extract_char(ch);
      return 1;
    }
  return 0;
}



/* Update both PC's & NPC's and objects*/
void recount_light_room(int room);
void update_room_tracks();
extern int LOOT_DECAY_TIME;

void point_update(void)
{
	int recalc_flag = 0, full_update, feeling, mytime, tmp;
	void	update_char_objects(struct char_data *ch); /* handler.c */
	void	extract_obj(struct obj_data *obj); /* handler.c */
	struct char_data *i, *next_dude;
	struct obj_data *j, *next_thing, *jj, *next_thing2;
	struct affected_type * hjp;

	/* characters */
	recalc_zone_power();
	update_room_tracks();

	mytime = time(0);

	if (((mytime / 3600) % 24 == 9) && ((mytime / 60) % 60 == 30)) 
	{
		send_to_all("ROUTINE REBOOT IN 30 MINUTES.\n\r");
	}
	if (((mytime / 3600) % 24 == 9) && ((mytime / 60) % 60 == 55)) 
	{
		send_to_all("ROUTINE REBOOT IN 5 MINUTES.\n\r");
	}
	if (((mytime / 3600) % 24 == 9) && ((mytime / 60) % 60 == 56))
	{
		send_to_all("ROUTINE REBOOT IN 4 MINUTES.\n\r");
	}
	if (((mytime / 3600) % 24 == 9) && ((mytime / 60) % 60 == 59)) 
	{
		send_to_all("ROUTINE REBOOT IN 1 MINUTE.\n\r");
	}
	if (((mytime / 3600) % 24 == 10) && (((mytime / 60) % 60 == 0) || ((mytime / 60) % 60 == 1))) 
	{
		send_to_all("ROUTINE REBOOT NOW.\n\r");
		circle_shutdown = 1;
	}

	for (i = character_list; i; i = next_dude) 
	{
		next_dude = i->next;

		affect_total(i, AFFECT_TOTAL_TIME);
		feeling = report_zone_power(i);
		// hint/stench messages no longer automatic
		if ((GET_LEVEL(i) < LEVEL_IMMORT) && (GET_POS(i) > POSITION_SLEEPING) && !IS_SET(PLR_FLAGS(i), PLR_WRITING)) 
		{
			if ((feeling < 0) && !(number(0, 3)))
			{
				send_to_char("You catch a waft of evil stench in the air.\n\r", i);
			}
			if ((feeling > 0) & !(number(0, 4)))
			{
				send_to_char("The air brings a hint of goodness' presence to you.\n\r", i);
			}
		}

		if (!IS_NPC(i)) 
		{
			update_char_objects(i);

			//  Time messages removed.
			//  if(PRF_FLAGGED(i, PRF_TIME) && (GET_POS(i) >= POSITION_SLEEPING))
			//    send_to_char("You feel the time passing by...\n\r",i);
			if (check_idling(i)) 
				continue;
		}

		full_update = -1;
		if ((hjp = affected_by_spell(i, SPELL_SLOW_DIGESTION)) != 0) 
		{
			if (hjp->modifier > number(0, 49)) full_update = 0;
		}

		if ((!IS_NPC(i) && !PLR_FLAGGED(i, PLR_RETIRED)) || IS_NPC(i)) 
		{
			gain_condition(i, FULL, full_update);
			gain_condition(i, THIRST, full_update);
		}
		gain_condition(i, DRUNK, -1);

		if (GET_POS(i) == POSITION_STUNNED)
			update_pos(i);  // Added by Fingolfin 30th December 2001

		if (GET_POS(i) >= POSITION_STUNNED) 
		{
			if (GET_AMBUSHED(i))
				if (IS_NPC(i))
					GET_AMBUSHED(i) -= GET_AMBUSHED(i) / (GET_LEVELA(i) + 5) + 1;
				else
					GET_AMBUSHED(i) -= GET_AMBUSHED(i) / 10 + 1;

			recalc_flag = 0;

			if (GET_POS(i) == POSITION_SLEEPING) tmp = 2;
			else tmp = 1;

			if (GET_STR(i) != GET_STR_BASE(i)) {
				SET_STR(i, MIN(GET_STR(i) + number(1, tmp), GET_STR_BASE(i)));
				recalc_flag++;
			}
			if (GET_INT(i) != GET_INT_BASE(i)) {
				GET_INT(i) = MIN(GET_INT(i) + number(1, tmp), GET_INT_BASE(i));
				recalc_flag++;
			}
			if (GET_WILL(i) != GET_WILL_BASE(i)) {
				GET_WILL(i) = MIN(GET_WILL(i) + number(1, tmp), GET_WILL_BASE(i));
				recalc_flag++;
			}
			if (GET_DEX(i) != GET_DEX_BASE(i)) {
				GET_DEX(i) = MIN(GET_DEX(i) + number(1, tmp), GET_DEX_BASE(i));
				recalc_flag++;
			}
			if (GET_CON(i) != GET_CON_BASE(i)) {
				GET_CON(i) = MIN(GET_CON(i) + number(1, tmp), GET_CON_BASE(i));
				recalc_flag++;
			}
			if (GET_LEA(i) != GET_LEA_BASE(i)) {
				GET_LEA(i) = MIN(GET_LEA(i) + number(1, tmp), GET_LEA_BASE(i));
				recalc_flag++;
			}
			if (recalc_flag) {
				//	recalc_abilities(i);
				affect_total(i);
			}
			//Moving all this to fast effect
			//     GET_HIT(i)  = MIN(GET_HIT(i)  + hit_gain(i),  GET_MAX_HIT(i));
			//      GET_MANA(i) = MIN(GET_MANA(i) + mana_gain(i), GET_MAX_MANA(i));
			//      GET_MOVE(i) = MIN(GET_MOVE(i) + move_gain(i), GET_MAX_MOVE(i));
			//      //GET_SPIRIT(i) = MIN(GET_SPIRIT(i) + spirit_gain(i), GET_MAX_SPIRIT(i));
			//      if(GET_SPIRIT(i) < GET_WILL(i)/3 + GET_PROF_LEVEL(PROF_CLERIC,i)/3)
			//	GET_SPIRIT(i) += number(1,GET_WILL(i)+GET_PROF_LEVEL(PROF_CLERIC,i))/10;

			if (IS_NPC(i) && i->specials.attacked_level) {

				if (i->specials.attacked_level <= 1) i->specials.attacked_level = 0;
				else
					if (GET_POS(i) != POSITION_FIGHTING)
						i->specials.attacked_level -= 2;

			}

			if (!affected_by_spell(i, SPELL_POISON) && IS_AFFECTED(i, AFF_POISON))
				damage(i, i, 5, SPELL_POISON, 0);
			//        if (GET_POS(i) == POSITION_STUNNED)
			//  	update_pos(i);

		}
		else if (GET_POS(i) == POSITION_INCAP)
			damage(i, i, 3, TYPE_SUFFERING, 0);
	} /* for */

	/* objects */
	for (j = object_list; j; j = next_thing)
	{
		next_thing = j->next; /* Next in object list */

		//      /* If this is a corpse */
		//if ( (GET_ITEM_TYPE(j) == ITEM_CONTAINER) && (j->obj_flags.value[3]) ) {
		if (j->obj_flags.timer >= 0)
		{
			/* timer count down */
			if (j->obj_flags.timer > 0)
				j->obj_flags.timer--;

			if (j->obj_flags.timer == 0)
			{
				if (j->carried_by)
				{
					act("$p decays in your hands.", FALSE, j->carried_by, j, 0, TO_CHAR);
				}
				else if ((j->in_room != NOWHERE) && (world[j->in_room].people)) 
				{
					act("$p decays into dust.", TRUE, world[j->in_room].people, j, 0, TO_ROOM);
					act("$p decays into dust.", TRUE, world[j->in_room].people, j, 0, TO_CHAR);
				}

				if (GET_ITEM_TYPE(j) == ITEM_CONTAINER)
				{
					// If this is a corpse, let big brother know that it is decaying.
					if (j->obj_flags.value[3] == 1)
					{
						game_rules::big_brother& bb_instance = game_rules::big_brother::instance();
						bb_instance.on_corpse_decayed(j);
					}

					for (jj = j->contains; jj; jj = next_thing2)
					{
						next_thing2 = jj->next_content; /* Next in inventory */
						obj_from_obj(jj);

						if (j->in_obj)
						{
							obj_to_obj(jj, j->in_obj);
						}
						else if (j->carried_by)
						{
							obj_to_room(jj, j->carried_by->in_room);
						}
						else if (j->in_room != NOWHERE)
						{
							obj_to_room(jj, j->in_room);
						}
						else
						{
							log("SYSERR: OBJ DECAYED IN NOWHERE (limits.c)!!!");
						}

						jj->obj_flags.timer = LOOT_DECAY_TIME;
					}
				}

				extract_obj(j);
				continue;
			}
		}

		if (GET_ITEM_TYPE(j) == ITEM_FOUNTAIN) {
			/* supposedly this will refill fountains at zone resets. */
			//      printf("resetting fountain %s\n",j->name);
			j->obj_flags.value[1] = j->obj_flags.value[0];
		}
		else if (GET_ITEM_TYPE(j) == ITEM_LIGHT) {
			if ((j->obj_flags.value[2] > 0) && (j->obj_flags.value[3] > 0) && !(IS_NPC(j->carried_by)))
				j->obj_flags.value[2]--;
			if ((j->obj_flags.value[2] == 0) && (j->obj_flags.value[3] > 0)) {
				// the torch went out messages
				j->obj_flags.value[3] = 0;

				if (j->carried_by) {
					act("Your $o went out.", FALSE, j->carried_by, j, 0, TO_CHAR);
					act("$n's $o went out.", TRUE, j->carried_by, j, 0, TO_ROOM);
					recount_light_room(j->carried_by->in_room);
				}
				else if (j->in_room != NOWHERE) {
					sprintf(buf, "%s here went out.\n\r", j->short_description);
					send_to_room(buf, j->in_room);
					recount_light_room(j->in_room);
				}
				extract_obj(j);
			}
			else if ((j->obj_flags.value[2] < 3) &&
				(j->obj_flags.value[2] >= 0) &&
				(j->obj_flags.value[3] > 0)) {
				// the torch flickers messages

				if (j->carried_by) {
					act("Your $o flickers weakly.", FALSE, j->carried_by, j, 0, TO_CHAR);
					act("$n's $o flickers weakly.", TRUE, j->carried_by, j, 0, TO_ROOM);
					//	  recount_light_room(j->carried_by->in_room);
				}
				else if (j->in_room != NOWHERE) {
					sprintf(buf, "%s here flickers weakly.\n\r", j->short_description);
					send_to_room(buf, j->in_room);
					//	  recount_light_room(j->in_room);
				}
			}
		}
		else if ((GET_ITEM_TYPE(j) == ITEM_CONTAINER) && (!j->obj_flags.value[3])
			&& (!j->carried_by) && (j->in_room != NOWHERE)) {
			/* the object is not a corpse, and is on the ground... */
			/* restoring its closed/locked state */
			if (!(j->obj_flags.value[1] & CONT_CLOSED) &&
				(j->obj_flags.value[4] & CONT_CLOSED))
				act("$o closes quietly.", TRUE, 0, j, 0, TO_ROOM);
			if ((j->obj_flags.value[1] & CONT_CLOSED) &&
				!(j->obj_flags.value[4] & CONT_CLOSED))
				act("$o opens quietly.", TRUE, 0, j, 0, TO_ROOM);
			if (!(j->obj_flags.value[1] & CONT_LOCKED) &&
				(j->obj_flags.value[4] & CONT_LOCKED))
				act("You hear a soft click.", FALSE, 0, j, 0, TO_ROOM);
			j->obj_flags.value[1] = j->obj_flags.value[4];
		}

	}
}

void update_room_tracks(){
  room_data * tmproom;
  int roomnum, tmp;

  for(roomnum = 0; roomnum <= top_of_world; roomnum++){
    tmproom = &world[roomnum];
    
    for(tmp = 0; tmp < NUM_OF_TRACKS; tmp++)
      if(tmproom->room_track[tmp].data/8 == time_info.hours){
		tmproom->room_track[tmp].char_number = 0;
		tmproom->room_track[tmp].data = 0;
        tmproom->room_track[tmp].condition = 0;
      }
  }
}

/*
 * Function called when a character chooses "1" from the main
 * menu and the character's level is 0.
 */
void
do_start(struct char_data *ch)
{
  int tmp;
  extern int top_of_p_table;  /* From db.cc */
  
  GET_MINI_LEVEL(ch) = 99;
  GET_LEVEL(ch) = 0;
  GET_EXP(ch) = 1500;
  
  GET_BODYTYPE(ch) = 1;
  set_title(ch);
  
  roll_abilities(ch, 80, 85);
  
  ch->constabilities.move = 80;
  ch->constabilities.mana = 40;
  ch->points.spirit = 9;
  ch->constabilities.hit  = 10;  /* These are BASE numbers   */
  SPELLS_TO_LEARN(ch) = 10;
  
  if(!number(0,1))
    ch->constabilities.hit++;
  
  GET_GOLD(ch) = number(200,300);
  
  advance_mini_level(ch);
  gain_exp_regardless(ch,1500);
  
  for(tmp = 0; tmp < MAX_SKILLS; tmp++)
    ch->skills[tmp] = 0;
  
  recalc_skills(ch);
  recalc_abilities(ch);
  GET_REROLLS(ch) = 0;
  
  GET_HIT(ch) = GET_MAX_HIT(ch);
  GET_MANA(ch) = GET_MAX_MANA(ch);
  GET_MOVE(ch) = GET_MAX_MOVE(ch);   
  
  GET_COND(ch, THIRST) = 48;
  GET_COND(ch, FULL) = 48;
  GET_COND(ch, DRUNK) = 0;
  
  ch->player.time.played = 0;
  ch->player.time.logon = time(0);

  /*
   * If no player files exist, the top of the p_table is -1.  We
   * call create_entry before we call do_start, and create_entry
   * will create a new cell in the ptab and increment the top by
   * 1.  Hence if the top is 0, there are no players other than
   * this one and we want it to be the imp.
   */
  if (top_of_p_table == 0) {
    log("Player table was empty: creating new implementor.\r\n");
    gain_exp_regardless(ch, xp_to_level(LEVEL_IMPL));
  }
}

// Simply checks that a character can breathe in the room they are in.
// If not, they are affected by asphyxiation, the effects of which are dealt with elsewhere.
   
void check_breathing(char_data *ch){
  affected_type newaf;
  affected_type *tmpaf;

  tmpaf = affected_by_spell(ch, SPELL_ASPHYXIATION);
  if (!(can_breathe(ch))){
	if (tmpaf){
	  tmpaf->modifier += 1;
	} else {
	  newaf.type = SPELL_ASPHYXIATION;
	  newaf.duration = 100;
	  newaf.modifier = 1;
	  newaf.location = APPLY_NONE;
	  newaf.bitvector = 0;
	  affect_to_char(ch, &newaf);
	  send_to_char("Panic!  You cannot breathe!!\n\r", ch);
	}
  } else {
	if (tmpaf) {
	  if (tmpaf->modifier > 1)
		tmpaf->modifier -= 1;
	  
	  else {
		affect_from_char(ch, SPELL_ASPHYXIATION);
		send_to_char("Your breathing has returned to normal.\n\r", ch);
	  }
	}
  }
}

void do_power_of_arda(char_data *ch){
  affected_type * tmpaf;
  affected_type newaf;
  int level, maxlevel;

  level = get_sun_level(ch->in_room);
  maxlevel = level * 25;     // so if you are in a shady room, the penalty can never be too high
  tmpaf = affected_by_spell(ch, SPELL_ARDA);
  if (level){
    if (tmpaf){
	  if (tmpaf->modifier > maxlevel)
		tmpaf->modifier = MAX(tmpaf->modifier - level, 30); // if you are in a shady room, the affection can never disappear
	  else
		tmpaf->modifier = MIN(tmpaf->modifier + level, 400);
      tmpaf->duration = 100;  // to keep the affection going
    } else {
      newaf.type = SPELL_ARDA;
      newaf.duration = 100;  // immaterial this figure
      newaf.modifier = level;
      newaf.location = APPLY_NONE;
      newaf.bitvector = 0;
      affect_to_char(ch, &newaf);
      send_to_char("The power of Arda weakens your body.\n\r", ch);
    }
  } else {
    if (tmpaf){
	  if (GET_POS(ch) < POSITION_RESTING)
		level = 20;
		else if (GET_POS(ch) < POSITION_STANDING)
		  level = 15;
		  else level = 10;
      tmpaf->modifier -= level;
      if (tmpaf->modifier <= 0)
        tmpaf->duration = 0;
    }
  }
}

void	affect_update_person(struct char_data * i, int mode )
{
  affected_type *otheraf;
  // mode != 0 for fast_update

   static struct affected_type *af, *next_af_dude;
   int tmp, freq, val, time_phase;
   
   if(i->desc && i->desc->connected &&(i->desc->connected != CON_LINKLS))
     return; // PC is not in 'playing' mode

   time_phase = get_current_time_phase();
   

   for (af = i->affected; af; af = next_af_dude) {
     next_af_dude = af->next;
     if(skills[af->type].is_fast || (!mode && (time_phase == af->time_phase))){
       if ((af->duration >= 1)|| (af->duration < 0)){
         if(af->duration >= 1)af->duration--;

         switch(af->type){
         case SPELL_POISON:
	   otheraf = affected_by_spell(i, SPELL_RESIST_POISON);
	   if (otheraf) {
	     af->duration = MAX(af->duration - otheraf->modifier, 0);
	     otheraf->duration = af->duration;
	   }

	   /* If poison is fatal, damage returns non-zero */
           if (damage(i, i, 5, SPELL_POISON, 0))
             return;
           break;
	 case SPELL_CURING:
	 case SPELL_RESTLESSNESS:
           if (!(char_perception_check(i))) break;
	   freq = (SECS_PER_MUD_HOUR * 4) / PULSE_FAST_UPDATE;
	   tmp = af->modifier / freq;
	   if(af->modifier > 0)
	     tmp += ((af->modifier % freq) >= number(1,freq))? 1 : 0;
	   else
	     tmp -= (-(af->modifier % freq) >= number(1,freq))? 1 : 0;
	   
	   if(((GET_HIT(i) >= GET_MAX_HIT(i)) && (tmp < 0)) ||
	      ((GET_MOVE(i) >= GET_MAX_MOVE(i)) && (tmp > 0)) ||
	      ((GET_HIT(i) <= 1) && (tmp > 0))  ||
	      ((GET_MOVE(i) <= 0) && (tmp < 0))) break;
	   GET_HIT(i) -= tmp;
	   if(GET_HIT(i) > GET_MAX_HIT(i)) GET_HIT(i) = GET_MAX_HIT(i);
	   GET_MOVE(i) += tmp;
	   if(GET_MOVE(i) > GET_MAX_MOVE(i)) GET_MOVE(i) = GET_MAX_MOVE(i);
	   if(GET_MOVE(i) < 0) GET_MOVE(i) = 0;
	   break;
	 case SPELL_FEAR:
	   do_flee(i,"",0,0,0);
	   if(saves_spell(i, af->modifier-=2, 0)) af->duration = 0;
	   break;
	 case SPELL_VITALITY:
           if (!(char_perception_check(i))) break;
	   freq = (SECS_PER_MUD_HOUR * 4) / PULSE_FAST_UPDATE;
	   val = af->duration*6 / freq;
	   tmp = val / freq;
	   if(af->modifier > 0)
	     tmp += ((val % freq) >= number(1,freq))? 1 : 0;
	   else if(af->modifier < 0)
	     tmp -= ((val % freq) >= number(1,freq))? 1 : 0;
	   GET_MOVE(i) += tmp;
	   if(GET_MOVE(i) > GET_MAX_MOVE(i)) GET_MOVE(i) = GET_MAX_MOVE(i);
	   if(GET_MOVE(i) < 0){ 
	     GET_MOVE(i) = 0;
	   }
	   break;
	 case SPELL_REGENERATION:
           if (!(char_perception_check(i))) break;
	   freq = (SECS_PER_MUD_HOUR * 4) / PULSE_FAST_UPDATE;
	   val = af->duration*6 / freq;
	   tmp = val / freq;
	   if(af->modifier > 0)
	     tmp += ((val % freq) >= number(1,freq))? 1 : 0;
	   else if(af->modifier < 0)
	     tmp = -tmp - ((val % freq) >= number(1,freq))? 1 : 0;
	   else tmp = 0;
	   GET_HIT(i) += tmp;
	   if(GET_HIT(i) > GET_MAX_HIT(i)) GET_HIT(i) = GET_MAX_HIT(i);
	   if(GET_HIT(i) <= 0){ 
	     act("$n suddenly collapses on the ground.",
		 TRUE, i, 0, 0, TO_ROOM);
	     send_to_char("Your body failed to the magic.\n\r",i);
	     raw_kill(i, NULL, TYPE_UNDEFINED);
	     add_exploit_record(EXPLOIT_REGEN_DEATH, i, 0, NULL);
	     return;
	   }
	   break;
	 case SPELL_ACTIVITY:
	   if(IS_NPC(i))
	     one_mobile_activity(i);
	 case SPELL_CONFUSE:
	   if (IS_AFFECTED(i, AFF_CONCENTRATION)  && (af->duration >= 10))
		 af->duration -= 3;
	   break;
	   
	 case SPELL_ASPHYXIATION:
	   if (!(can_breathe(i))){
		 if (af->modifier > 40){
		   send_to_char("You gasp one final time as your body loses its battle for life...\n\r", i);
		   act("$n gasps one last time... and dies.", TRUE, i, 0, 0, TO_ROOM);
		   raw_kill(i, NULL, SPELL_ASPHYXIATION);
		   return;
		 }
		 if (af->modifier > 20){
		   if (damage(i, i, (af->modifier / 5), SPELL_ASPHYXIATION,0))
			 return;
		   GET_MOVE(i) = MAX(GET_MOVE(i) - 4, 10);
		 }
	   }
	   break;
	 default:
	   break;
	 }
	   if (GET_POS(i) == POSITION_STUNNED)
	     update_pos(i);	   
       }
       else {
	 if (af->type > 0 && af->type < MAX_SKILLS)
	   /* It must be a spell */
	   if (!af->next || af->next->type != af->type ||
               af->next->duration > 0)
             affect_remove_notify(i, af);
         else
             affect_remove(i, af);
	
	 if (af->type == SPELL_ANGER)
	   i->specials.attacked_level = 0;
       }
     }
   }
}



ASPELL(spell_poison);

void
affect_update_room(struct room_data *room)
{
  struct affected_type *tmpaf, *next_tmpaf, *checkaf;
  struct affected_type newaf;
  struct char_data *tmpch, *next_tmpch;
  int time_phase, tmp, direction, mod, roomnum, movechance;
  extern char *dirs[];

  time_phase = get_current_time_phase();

  movechance = number(1, 100); 	//to be used to check if mists move.

  for(tmpaf = room->affected; tmpaf; tmpaf = next_tmpaf){
    next_tmpaf = tmpaf->next;

    switch(tmpaf->type){
    case ROOMAFF_SPELL:
      if((tmpaf->location >= 0) && (tmpaf->location < MAX_SKILLS) &&
	 skills[tmpaf->location].spell_pointer) {
	if(tmpaf->location == SPELL_MIST_OF_BAAZUNGA)
	  if(!IS_SET(room->room_flags, SHADOWY)) 
	    SET_BIT(room->room_flags, SHADOWY);

	if(1 /*skills[tmpaf->location].targets  & TAR_CHAR_ROOM*/){
	  for(tmpch = room->people; tmpch; tmpch = next_tmpch){
	    next_tmpch = tmpch->next_in_room;

	    /* 1 in 13 chance that a room spell won't do anything */
	    if(!(tmp = number(0, 12)) ||
	       (skills[tmpaf->location].is_fast && !number(0, 2))) {
	      (skills[tmpaf->location].spell_pointer)
		(tmpaf->modifier, tmpch, "", SPELL_TYPE_SPELL,
		 tmpch, 0, 0, 0);
	    }
	  } 
	}
	else {
	  sprintf(buf2,"Attempt to cast spell %d in room %d",
		  tmpaf->location, room->number);
	  mudlog(buf2,NRM, LEVEL_GOD, FALSE);
	}
      }
    }
    
    if(((time_phase == tmpaf->time_phase) || 
	((tmpaf->type == ROOMAFF_SPELL) && skills[tmpaf->location].is_fast))
       && (tmpaf->duration > 0))
      tmpaf->duration--;
    if(tmpaf->location == SPELL_MIST_OF_BAAZUNGA && 
       tmpaf->duration > 0 && time_phase == tmpaf->time_phase) {
      /* 70% chance of mist thinking about moving */
      sprintf(buf,"check mist movement");
      mudlog(buf,NRM,LEVEL_GOD,FALSE);
      if(movechance < 75){
	direction = number (0, NUM_OF_DIRS);
	/* Decide if the random direction is legal, if so, move the mist */
	if(!(room->dir_option[direction])) { 
	  sprintf(buf,"no option for movement");
	  mudlog(buf,NRM,LEVEL_GOD,FALSE);
	}
	else if(room->dir_option[direction]->to_room != NOWHERE) {
	  roomnum = room->dir_option[direction]->to_room;
	  if(!(room_affected_by_spell(&world[roomnum],
				      SPELL_MIST_OF_BAAZUNGA))) {
	    if(tmpaf->modifier != 1)
	      REMOVE_BIT(room->room_flags, SHADOWY);
	    if((checkaf = room_affected_by_spell(&world[roomnum],
						 SPELL_MIST_OF_BAAZUNGA))) {
	      mod = checkaf->modifier;
	      sprintf(buf,"WARNING LOMAN: Mist already in move to room");
	      mudlog(buf,NRM,LEVEL_GOD,FALSE); 
	    }
	    else if(IS_SET(world[roomnum].room_flags, SHADOWY)) 
	      mod = 1;
	    else
	      mod = 0;
	    
	    newaf.type = ROOMAFF_SPELL;
	    newaf.duration = tmpaf->duration;
	    newaf.modifier = mod;
	    newaf.location = SPELL_MIST_OF_BAAZUNGA;
	    newaf.bitvector = 0;
	    
	    sprintf (buf, "The mists drift %s.\n\r", dirs[direction]);
	    send_to_room(buf, room->number);
	    
	    affect_to_room(&world[roomnum], &newaf);
	    affect_remove_room(room ,tmpaf);
	  }
	}
      }
    }
    
    if(tmpaf)
      if(tmpaf->duration == 0) {
	if(tmpaf->location == SPELL_MIST_OF_BAAZUNGA && tmpaf->modifier != 1)
	  REMOVE_BIT(room->room_flags, SHADOWY);
	affect_remove_room(room, tmpaf);
      }
  }
}



extern universal_list * affected_list;

extern universal_list * affected_list_pool;

void affect_update(){
  universal_list * tmplist, *tmplist2, *tmplist3;;
  char mybuf[1000];

  tmplist3 = 0;
  for(tmplist = affected_list; tmplist; tmplist = tmplist2){
    tmplist2 = tmplist->next;

    if(tmplist->type == TARGET_CHAR){

      if (char_exists(tmplist->number) && tmplist->ptr.ch && tmplist->ptr.ch->affected){
		affect_update_person(tmplist->ptr.ch, 0);
      } else {
		if(char_exists(tmplist->number))
		  sprintf(mybuf,"Getting %s off the affected_list.", GET_NAME(tmplist->ptr.ch));
		else
		  strcpy(mybuf,"Getting Unknown char off the affected_list.");
		mudlog(mybuf,CMP,LEVEL_GRGOD, TRUE);
		from_list_to_pool(&affected_list, &affected_list_pool, tmplist);
      }
    }
    else if(tmplist->type == TARGET_ROOM){
      affect_update_room(tmplist->ptr.room);
    }
    tmplist3 = tmplist; /* prev item */
  }
}

void fast_update(){
  struct char_data * i;
  struct char_data * next_dude;
  int freq, hitregen, moveregen, manaregen, roll;
  
  freq = (SECS_PER_MUD_HOUR * 4) / PULSE_FAST_UPDATE;
  roll = number (1,freq);
  for (i = character_list; i; i = next_dude) {
    next_dude = i->next;
    hitregen = hit_gain(i)/freq + ((roll<=hit_gain(i)%freq)?1:0);
    moveregen= move_gain(i)/freq+((roll<=move_gain(i)%freq)?1:0);
    manaregen= mana_gain(i)/freq+((roll<=mana_gain(i)%freq)?1:0);

    GET_HIT(i)  = MIN(GET_HIT(i)  + hitregen,  GET_MAX_HIT(i));
    GET_MANA(i) = MIN(GET_MANA(i) + manaregen, GET_MAX_MANA(i));
    GET_MOVE(i) = MIN(GET_MOVE(i) + moveregen ,GET_MAX_MOVE(i));
    if(GET_SPIRIT(i) < GET_WILL(i)/3 + GET_PROF_LEVEL(PROF_CLERIC,i)/3)
	  GET_SPIRIT(i) += number(1,GET_WILL(i)+GET_PROF_LEVEL(PROF_CLERIC,i))/(10*freq);

    if (EVIL_RACE(i))
      do_power_of_arda(i);
	check_breathing(i);

//     affect_update_person(i, 1);
  }
}
