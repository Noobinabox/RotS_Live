/* This file deals with procedures relating to profs. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platdef.h"

#include "structs.h"
#include "utils.h"
#include "comm.h" 
#include "interpre.h"
#include "db.h"
#include "limits.h"
#include "profs.h"
#include "spells.h"
#include "handler.h"

#include "char_utils.h"
#include <cmath>

#define MAX_STATSUM 99


extern struct char_data *character_list;
extern int max_race_str[];
extern struct obj_data *object_list;

struct prof_type existing_profs[DEFAULT_PROFS] = {
  {'m', {0, 100, 25, 16, 9}},
  {'t', {0,  25,100,  9, 16}},
  {'r', {0,  16,  9,100, 25}},
  {'w', {0,   9, 16, 25,100}},
  {'n', {0,  64, 64,  9, 13}},
  {'i', {0, 121, 16,  9,  4}},
  {'h', {0, 25, 121,  0,  4}},
  {'s', {0,  9,  13, 64, 64}},
  {'b', {0,  0,   4, 25,121}},
  {'a', {0, 36,  36, 36, 42}},
};

sh_int race_modifiers[MAX_RACES][8] = {
  { 0, 0, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, 0, 0},
  { 2, 0,-2,-3, 4,-1, 0, 0},
  {-1, 1, 0, 2,-2, 0, 0, 0},
  {-3,-1, 0, 2, 2, 0, 0, 0},
  { 0, 2, 0, 2,-2, 0, 0, 0},  /* 5 */
  { 0, 0, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, 0, 0},
  { 0,-4,-3, 0, 2,-3, 0, 0},  /* 11 */
  { 0, 0,-1, 0, 1, 0, 0, 0},
  {-1,-3,-3,-1,-1,-5, 0, 0},
  { 0, 0, 0, 0, 0, 0, 0, 0},
  {-1,-1,-3, 0, 1,-2, 0, 0}
};

/*
 * This function, when given i < 170*4, returns 200*sqrt(i).
 * CH is only needed to send overflow to.
 */
inline
int do_squareroot(int i, struct char_data *ch)
{
  if(i/4 > 170) {
    if(ch)
      send_to_char("NON_FATAL OVERFLOW, Dim error 1 in do_squareroot.\r\n"
		   "Please notify imps.\n\r", ch);
    i = 170*4;
  }
  
  return (4 - i%4)*square_root[i/4] + (i%4)*square_root[i/4+1];
}

inline int class_HP(const char_data* character)
{
	double hp_coofs = 3 * utils::get_prof_points(PROF_WARRIOR, *character) +
		2 * utils::get_prof_points(PROF_RANGER, *character) +
		utils::get_prof_points(PROF_CLERIC, *character);

	if (GET_RACE(character) == RACE_ORC)
	{
		hp_coofs = hp_coofs * 4.0 / 7.0;
	}

	return int(std::sqrt(hp_coofs) * 200.0);
}



void draw_line(char *buf, int length)
{
  int k;
  char buff[81];
  
  for( k=0 ; k < length ; k++)
    buff[k] = '*';
  buff[k] = 0;
  strcat(buf , buff);
}



void
draw_coofs(char *buf, struct char_data *ch)
{
  char buf2[80];
  
  sprintf(buf, "\r\n"
          "    0%%,      20%%,      40%%,      60%%,      80%%,      100%%"
	  "\n\r"
	  "    |         |         |         |         |         |\n\r");
  
  sprintf(buf2,"Mag: ");
  draw_line(buf2, GET_PROF_COOF(1,ch)/20);
  strcat(buf,buf2);
  
  sprintf(buf2,"\n\rMys: ");
  draw_line(buf2, GET_PROF_COOF(2,ch)/20);
  strcat(buf,buf2);
  
  sprintf(buf2,"\n\rRan: ");
  draw_line(buf2, GET_PROF_COOF(3,ch)/20);
  strcat(buf,buf2);
  
  sprintf(buf2,"\n\rWar: ");
  draw_line(buf2, GET_PROF_COOF(4,ch)/20);
  strcat(buf,buf2);
  strcat(buf,"\n\r\0");
}



int
points_used(struct char_data *ch)
{  
  return GET_PROF_POINTS(PROF_MAGE, ch) + GET_PROF_POINTS(PROF_CLERIC, ch) + 
	 GET_PROF_POINTS(PROF_RANGER, ch) + GET_PROF_POINTS(PROF_WARRIOR, ch);
}



void
advance_level_prof(int prof, struct char_data *ch)
{
  SET_PROF_LEVEL(prof,ch,GET_PROF_LEVEL(prof ,ch) + 1);
  switch(prof){
  case PROF_MAGE:
    GET_MAX_MANA(ch) += 2;
    send_to_char("You feel more adept in magic!\n\r",ch);
    break;
  case PROF_CLERIC:
    send_to_char("Your spirit grows stronger!\n\r",ch);
    break;
  case PROF_RANGER:
    send_to_char("You feel more agile!\n\r",ch);
    break;
  case PROF_WARRIOR:
    send_to_char("You have become better at combat!\n\r",ch);
    break;
  }
}



/* Gain maximum in various points */
void
check_stat_increase(struct char_data *ch)
{
  int statsum, i, j;
  ush_int order[6]={0, 0, 0, 0, 0, 0};
  
  statsum = ch->constabilities.con + ch->constabilities.intel +
    ch->constabilities.wil + ch->constabilities.dex + 
    ch->constabilities.str + ch->constabilities.lea;

  for(i = 0; i < 6; i++)
    statsum -= race_modifiers[GET_RACE(ch)][i];
  
  /* First, no stat increases if you haven't statted yet. */
  if(GET_LEVEL(ch) < 6)
    return;
  if(statsum > MAX_STATSUM)
    return;
  if(statsum > 97) {
    if(number(0, 99) > (7 + (MAX_STATSUM - statsum) * 3/2))
      return;
  } else if(statsum > 96) {
    if(number(0, 99) > (10 + (MAX_STATSUM - statsum) * 3/2))
      return;
  } else if(statsum > 95) {
    if(number(0, 99) > (13 + (MAX_STATSUM - statsum) * 3/2))
      return;
  } else if(statsum > 93) {
    if(number(0, 99) > (16 + (MAX_STATSUM - statsum) * 3/2))
      return;
  } else if(statsum > 91) {
    if(number(0, 99) > (19 + (MAX_STATSUM - statsum) * 3/2))
      return;
  } else if(statsum > 86) {
    if(number(0, 99) > (22 + (MAX_STATSUM - statsum) * 3/2))
      return;
  } else {
    if(number(0, 99) > (25 + (MAX_STATSUM - statsum) * 3/2))
      return;
  }
  /* so now decide which stat to add */
  for(i = 1; i < 5; i++)
    for(j = 1; j < i; j++) {
      if(GET_PROF_COOF(i, ch) >= GET_PROF_COOF(j,ch))
	order[j]++;
      else  
	order[i]++;
    }
  
#define STAT_CHANCE       14
#define PRIME_STAT_BONUS  16

  i = number(0, 99);

  i-= STAT_CHANCE;
  if(order[PROF_WARRIOR] == 0)
    i -= PRIME_STAT_BONUS;
  if(i < 0) {
    send_to_char("Great strength flows through you!\n",ch);
    add_exploit_record(EXPLOIT_STAT, ch, GET_LEVEL(ch), "+1 str");
    ch->constabilities.str++;
    ch->tmpabilities.str++;
    return;
  }
  
  i-= STAT_CHANCE;
  if(order[PROF_RANGER] == 0)
    i -= PRIME_STAT_BONUS;
  if(i < 0) {
    send_to_char("Your hands feel quicker!\n",ch);
    add_exploit_record(EXPLOIT_STAT, ch, GET_LEVEL(ch), "+1 dex");
    ch->constabilities.dex++;
    ch->tmpabilities.dex++;
    return;
  }
  
  i-= STAT_CHANCE;
  if(order[PROF_CLERIC] == 0)
    i -= PRIME_STAT_BONUS;
  if(i < 0) {
    send_to_char("You feel more wilful!\n",ch);
    add_exploit_record(EXPLOIT_STAT, ch, GET_LEVEL(ch), "+1 will");
    ch->constabilities.wil++;
    ch->tmpabilities.wil++;
    return;
  }
  
  i-= STAT_CHANCE;
  if(order[PROF_MAGE] == 0)
    i -= PRIME_STAT_BONUS;
  if(i < 0) {
    send_to_char("Your intelligence has improved!\n",ch);
    add_exploit_record(EXPLOIT_STAT, ch, GET_LEVEL(ch), "+1 int");
    ch->constabilities.intel++;
    ch->tmpabilities.intel++;
    return;
  }
  
  i-= STAT_CHANCE;
  if(i < 0) {
    send_to_char("You feel much more health!\n",ch);
    add_exploit_record(EXPLOIT_STAT, ch, GET_LEVEL(ch), "+1 con");
    ch->constabilities.con++;
    ch->tmpabilities.con++;
    return;
  }
  
  i-= STAT_CHANCE;
  if(i < 0) {
    send_to_char("You seem more learned!\n",ch);
    add_exploit_record(EXPLOIT_STAT, ch, GET_LEVEL(ch), "+1 learn");
    ch->constabilities.lea++;
    ch->tmpabilities.lea++;
    return;
  }   
}



void
advance_level(struct char_data *ch)
{
  int i;
  
  send_to_char("You feel more powerful!\n\r",ch);
  
  SPELLS_TO_LEARN(ch) += PRACS_PER_LEVEL + 
    (GET_LEA_BASE(ch) + GET_LEVEL(ch) % LEA_PRAC_FACTOR) / LEA_PRAC_FACTOR;

  if (GET_LEVEL(ch) >= LEVEL_IMMORT) {
    for(i = 0; i < 3; i++)
      GET_COND(ch, i) = (unsigned char) -1;
    GET_RACE(ch) = RACE_GOD;
  }
  
  sprintf(buf, "%s advanced to level %d", GET_NAME(ch), GET_LEVEL(ch));
  mudlog(buf, BRF, (sh_int) MAX(LEVEL_IMMORT, GET_INVIS_LEV(ch)), TRUE);
  
  /* log following levels in exploits */
  if((GET_LEVEL(ch) == 6) || (GET_LEVEL(ch) == 10) || (GET_LEVEL(ch) == 15) ||
     (GET_LEVEL(ch) == 20) || (GET_LEVEL(ch) == 25) || (GET_LEVEL(ch) == 30) ||
     (GET_LEVEL(ch) == 35) || (GET_LEVEL(ch) == 40) || (GET_LEVEL(ch) == 45) ||
     (GET_LEVEL(ch) == 50) || (GET_LEVEL(ch) == 55) || (GET_LEVEL(ch) > 89))
    add_exploit_record(EXPLOIT_LEVEL, ch, GET_LEVEL(ch), NULL);
  
  /* add birth exploit */
  if(GET_LEVEL(ch) == 1)
    add_exploit_record(EXPLOIT_BIRTH, ch, 0, NULL);
  
  if(GET_LEVEL(ch) > 5 && GET_MAX_MINI_LEVEL(ch) < 600) {
    GET_REROLLS(ch)++;
    roll_abilities(ch, 80, 93);
  }

  if(GET_MAX_MINI_LEVEL(ch) < GET_MINI_LEVEL(ch))
    check_stat_increase(ch);
  
  save_char(ch, NOWHERE, 0);
}



/* Give pointers to the six abilities */
void
roll_abilities(struct char_data *ch, int min, int max)
{
  int i, j, k, order[6], repeat = 1000;
  ush_int table[6];
  ush_int rools[4];
  char stats[256];
  
  while(repeat--) {
    for(i = 0; i < 6; i++)
      table[i] = order[i] = 0;
    
    /* generate first 4 stats */
    for(i = 0; i < 6; i++) {
      for(j = 0; j < 4; j++)
	rools[j] = number(1, 6);
      
      table[i] = rools[0] + rools[1] + rools[2] + rools[3] - 
	MIN(rools[0], MIN(rools[1], MIN(rools[2], rools[3])));
    }
    
    /* order first 6 stats */
    for(k = 0; k < 6; k++)
      for(i = 0; i < k; i++)
	if(table[i] < table[k])
	  SWITCH(table[i], table[k]);
    
    /* select order */  
    for(i = 1; i < 5; i++)
      for(j = 1; j < i; j++) {
	if(GET_PROF_COOF(i, ch) > GET_PROF_COOF(j,ch))
	  order[j]++;
	else  
	  order[i]++;
      }
    
    /* making room for, and inserting con+lea, into pos. 2 and 4 */
    for(i = 0; i < 5; i++) {
      if(order[i] == 3)
	order[i] = 5;
      if(order[i] == 2)
	order[i] = 3;
    }
    
    order[0] = 2;
    order[5] = 4;
    
    if(class_HP(ch) < 3000)
      SWITCH(order[0], order[5]);
    
    /* assign stats, adjusted for race */
    ch->constabilities.con = 1 + table[order[0]] + 
      race_modifiers[GET_RACE(ch)][4];
    ch->constabilities.intel = 1 + table[order[1]] + 
      race_modifiers[GET_RACE(ch)][1];
    ch->constabilities.wil = 1 + table[order[2]] +
      race_modifiers[GET_RACE(ch)][2];
    ch->constabilities.dex = 1 + table[order[3]] +
      race_modifiers[GET_RACE(ch)][3];
    ch->constabilities.str = 1 + table[order[4]] +
      race_modifiers[GET_RACE(ch)][0];
    ch->constabilities.lea = 1 + table[order[5]] +
      race_modifiers[GET_RACE(ch)][5];
    
    if(GET_LEVEL(ch) > 1) {
      sprintf(stats,"STATS: %s rolled  %d %d %d %d %d %d", 
	      GET_NAME(ch), ch->constabilities.str, ch->constabilities.intel, 
	      ch->constabilities.wil, ch->constabilities.dex, 
	      ch->constabilities.con, ch->constabilities.lea);
      log(stats);
    }
    
    recalc_abilities(ch);
    
    ch->tmpabilities = ch->abilities;
    i = table[0] + table[1] + table[2] + table[3] + table[4] + table[5];
    if(repeat == 1)
      mudlog("Couldn't roll abilities on 1000th try!", NRM, LEVEL_GOD, TRUE);
    
    if(i < min || i > max);
    else 
      repeat = 0;     
  }
}

/* This is called whenever some of person's stats/level change */
void
recalc_abilities(struct char_data *ch)
{
	int tmp, tmp2, dex_speed;
	struct obj_data *weapon;

	if (!IS_NPC(ch)) {
		ch->abilities.str = ch->constabilities.str;
		ch->abilities.lea = ch->constabilities.lea;
		ch->abilities.intel = ch->constabilities.intel;
		ch->abilities.wil = ch->constabilities.wil;
		ch->abilities.dex = ch->constabilities.dex;
		ch->abilities.con = ch->constabilities.con;

		ch->abilities.hit = 10 + std::min(LEVEL_MAX, GET_LEVEL(ch)) +
			ch->constabilities.hit * GET_CON(ch) / 20 +
			(class_HP(ch) * (GET_CON(ch) + 20) / 14) *
			std::min(LEVEL_MAX * 100, (int)GET_MINI_LEVEL(ch)) / 100000;

		// dirty test to see if this ranger change can work
		ch->abilities.hit = std::max(ch->abilities.hit -
			(GET_RAW_SKILL(ch, SKILL_STEALTH) *
				GET_LEVELA(ch) +
				GET_RAW_SKILL(ch, SKILL_STEALTH) * 3) / 33, 10);

		if (ch->tmpabilities.hit > ch->abilities.hit)
			ch->tmpabilities.hit = ch->abilities.hit;

		ch->abilities.mana = ch->constabilities.mana + GET_INT(ch) +
			GET_WILL(ch) / 2 + GET_PROF_LEVEL(PROF_MAGE, ch) * 2;

		if (ch->tmpabilities.mana > ch->abilities.mana)
			ch->tmpabilities.mana = ch->abilities.mana;

		ch->abilities.move = ch->constabilities.move + GET_CON(ch) +
			20 + GET_PROF_LEVEL(PROF_RANGER, ch) +
			GET_RAW_KNOWLEDGE(ch, SKILL_TRAVELLING) / 4;
		if ((GET_RACE(ch) == RACE_WOOD) || GET_RACE(ch) == RACE_HIGH)
			ch->abilities.move += 15;

		if (ch->tmpabilities.move > ch->abilities.move)
			ch->tmpabilities.move = ch->abilities.move;

		if ((weapon = ch->equipment[WIELD])) 
		{
			if (!GET_OBJ_WEIGHT(weapon)) {
				/*UPDATE*, temporary check for 0 weight weapons*/
				GET_OBJ_WEIGHT(weapon) = 1;
				sprintf(buf, "SYSERR: 0 wegith weapon");
				mudlog(buf, NRM, LEVEL_GOD, TRUE);
			}

			int bulk = weapon->get_bulk();
			ch->specials.null_speed = 3 * GET_DEX(ch) + 2 * (GET_RAW_SKILL(ch, SKILL_ATTACK) +
					GET_RAW_SKILL(ch, SKILL_STEALTH) / 2) / 3 + 100;

			ch->specials.str_speed = GET_BAL_STR(ch) * 2500000 /
				(GET_OBJ_WEIGHT(weapon) * (bulk + 3));

			if (IS_TWOHANDED(ch))
			{
				ch->specials.str_speed *= 2;
			}

			/* Dex adjustment by Fingol */
			if (bulk < 4)
			{
				dex_speed = GET_DEX(ch) * 2500000 / (GET_OBJ_WEIGHT(weapon) * (bulk + 3));

				tmp2 = (ch->specials.str_speed * bulk / 5) + (dex_speed * (5 - bulk) / 5);

				ch->specials.str_speed = ch->specials.str_speed > tmp2 ? tmp2 : ch->specials.str_speed;
			}

			tmp = 1000000;
			tmp /= 1000000 / ch->specials.str_speed +
				1000000 / (ch->specials.null_speed * ch->specials.null_speed);

			game_types::weapon_type w_type = weapon->get_weapon_type();
			GET_ENE_REGEN(ch) = do_squareroot(tmp / 100, ch) / 20;
			if (GET_RACE(ch) == RACE_DWARF && weapon_skill_num(w_type) == SKILL_AXE)
			{
				GET_ENE_REGEN(ch) += std::min(GET_ENE_REGEN(ch) / 10, 10);
			}
			else if(w_type == game_types::WT_BOW || w_type == game_types::WT_CROSSBOW)
			{
				// Give a speed penalty for melee attacking with bows.
				ch->points.ENE_regen = ch->points.ENE_regen / 3;
			}
		}
		else
		{
			GET_ENE_REGEN(ch) = 60 + 5 * GET_DEX(ch);
		}
	}
}

/* Hp per level:  con/6 for pure mage, con/3 for normal warrior. plus*/
/* 10 hits for pure warrior */
/* and initial 10 hits for pure mage, 20 for warrior */


