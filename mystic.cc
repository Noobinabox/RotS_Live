/* ************************************************************************
*   File: mystic.cc                                     Part of CircleMUD *
*   Usage: actual effects of mystical spells                              *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993 by the Trustees of the Johns Hopkins University     *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/*
 * Mystic.cc is part of a code clean up project 
 * Mystic.cc is derived from circlemuds magic.cc
 * The change over was made in an effort to better organise
 * the code base by the rots-devel team (go us)
 * Change made by Khronos 28/03/05
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "platdef.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "spells.h"
#include "handler.h"
#include "interpre.h"
#include "db.h"
#include "limits.h"

#include <algorithm>
#include <sstream>
#include <iostream>
#include <vector>

/*---------------------------------------------------------------------------------------------*/

/*
 * Enternal Structures
 */

extern struct room_data world;
extern struct obj_data *obj_proto;
extern struct char_data *character_list;
extern int top_of_world;
extern int rev_dir[];
extern char *  dirs[];
extern char     *room_bits[];
extern char *   sector_types[];
extern int guardian_mob[][3];
extern char *race_abbrevs[];
/*
 *External functions
 */

char saves_mystic(struct char_data *);
char saves_poison(struct char_data *, struct char_data *);
char saves_confuse(struct char_data *, char_data *);
char saves_leadership(struct char_data *);
char saves_insight(struct char_data *, struct char_data *);
bool check_mind_block(char_data *, char_data *, int, int);
void list_char_to_char(struct char_data *list, struct char_data *ch,
		       int mode);
ACMD(do_look);


/*
 * Use this macro to cause objects to override any affections
 * already on a character.  For example, the onyx ring's evasion
 * affection deletes an previous evasion and replaces it with its
 * permanent evasion.
 */
#define OBJECT_OVERRIDE_AFFECTIONS(_type, _isobj, _ch, _spl) do { \
  if ((_isobj)) {                                                 \
    if ((_type) == SPELL_TYPE_ANTI) {                             \
      affect_from_char_notify((_ch), (_spl));                     \
      return;                                                     \
    } else                                                        \
      affect_from_char((_ch), (_spl));                            \
  }                                                               \
} while (0)

/*
 * Mystic spells are order by category and level
 * The spells are split into 4 Categories
 *  - Perception and Mental Skills
 *  - Self_Affection Spells
 *  - Regeneration Spells
 *  - Offensive (for lack of a better word) skills
 *  - Everything Else 
 */

/* Percaption and Mental skills ordered by level
 * - Curse
 * - Revive
 * - Mind Block
 * - Insight
 * - Pragmatism 
 */

int damage_stat(char_data * killer, char_data * ch, int stat_num, int amount);

ASPELL(spell_curse){
  int damage_table[7];
  int i, count, last_stat, num, actual_count;

  if(GET_MENTAL_DELAY(ch) > PULSE_MENTAL_FIGHT+1){
    send_to_char("Your mind is not ready yet.\n\r",ch);
    return;
  }

  count = (level + 2*10)*GET_PERCEPTION(victim)/100/10;
  if(!count){
    act("You try to curse $N, but can't reach $S mind.",FALSE, ch, 0, victim,
	TO_CHAR);
    return;
  }

  if (affected_by_spell(ch, SPELL_MIND_BLOCK)){
	act("You cannot curse with a blocked mind.",FALSE, ch, 0, victim, TO_CHAR);
	return;
  }
  
  if (GET_BODYTYPE(victim) != 1 && !IS_SHADOW(victim)) {
    act("You try to curse $N, but could not fathom its mind.", FALSE, ch, 0, victim, TO_CHAR);
    return;
  }

  for(i=0; i<6; i++) damage_table[i] = 0;

  /* choosing the first stat to damage */
  last_stat = num = number(0,6);

  damage_table[num] = 1;
  count--;

  for(i=0; i<count; i++){
    num = number(0,9);
    if(num > 6) num = last_stat;
    else last_stat = num;
    
    damage_table[num]++;
  }
  
  act("$n points ominously at $N and curses.",TRUE, ch, 0, victim, TO_NOTVICT);
  act("$n points ominously at you and curses.",TRUE, ch, 0, victim, TO_VICT);
  act("You point at $N and curse.",FALSE, ch, 0, victim, TO_CHAR);

  actual_count = 0;
  for(i=0; i<6; i++){
    if(damage_table[i]){
      GET_SPIRIT(ch) -= number(0,damage_table[i]);
      actual_count += damage_table[i];
      if(GET_SPIRIT(ch) < 0){
	GET_SPIRIT(ch) = 0;
	break;
      }
      if(damage_stat(ch, victim, i, damage_table[i])) break;;
    }
  }
  set_mental_delay(ch, actual_count*PULSE_MENTAL_FIGHT);
}

int restore_stat(char_data * ch,int stat_num, int amount);

ASPELL(spell_revive){
  int i, i2, num, count, actual_count, stat_dam, stat_dam2;
  int revive_table[7];

  if(GET_MENTAL_DELAY(ch) > PULSE_MENTAL_FIGHT+1){
    send_to_char("Your mind is not ready yet.\n\r",ch);
    return;
  }

  count = (3*9 + level)*GET_PERCEPTION(victim)/100/9;

  if(count*2 > GET_SPIRIT(ch)) count = GET_SPIRIT(ch)/2;

  if(count <= 0){
    send_to_char("You couldn't gather enough spirit to heal.\n\r",ch);
    return;
  }

  stat_dam = 100;
  i = -1;

  stat_dam2 = GET_STR(victim) / GET_STR_BASE(victim);
  if(stat_dam2 < stat_dam){
    stat_dam = stat_dam2;
    i = 0;
  }
  stat_dam2 = GET_INT(victim) / GET_INT_BASE(victim);
  if(stat_dam2 < stat_dam){
    stat_dam = stat_dam2;
    i = 1;
  }
  stat_dam2 = GET_WILL(victim) / GET_WILL_BASE(victim);
  if(stat_dam2 < stat_dam){
    stat_dam = stat_dam2;
    i = 2;
  }
  stat_dam2 = GET_DEX(victim) / GET_DEX_BASE(victim);
  if(stat_dam2 < stat_dam){
    stat_dam = stat_dam2;
    i = 3;
  }
  stat_dam2 = GET_CON(victim) / GET_CON_BASE(victim);
  if(stat_dam2 < stat_dam){
    stat_dam = stat_dam2;
    i = 4;
  }
  stat_dam2 = GET_LEA(victim) / GET_LEA_BASE(victim);
  if(stat_dam2 < stat_dam){
    stat_dam = stat_dam2;
    i = 5;
  }
  
  if(i < 0){
    send_to_char("No healing was needed there.\n\r",ch);
    return;
  }
  num = i;

  for(i = 0; i<7; i++) revive_table[i] = 0;

  for(i=0; i < count; i++){
    i2 = number(0,10);
    if(i2 <= 6) num = i2;
    revive_table[num]++;
  }
  act("$n tries to revive $N.",FALSE, ch, 0, victim, TO_NOTVICT);
  act("$n tries to revive you.",FALSE, ch, 0, victim, TO_VICT);
  act("You try to revive $N.",TRUE, ch, 0, victim, TO_CHAR);

  actual_count = 0;
  for(i=0; i<6; i++){
    num = restore_stat(victim, i, revive_table[i]);
    actual_count += num;
    GET_SPIRIT(ch) -= num;
    if(GET_SPIRIT(ch) <= 0) break;
  }
  if(!actual_count){
    act("Your spell does no good to $M.",FALSE, ch, 0, victim, TO_CHAR);
    act("$n tries to revive you, but does little good.", FALSE, ch, 0, 
	victim, TO_VICT);
  }
  else{
    set_mental_delay(ch, actual_count*PULSE_MENTAL_FIGHT);
  }

}


ASPELL(spell_mind_block){
  struct affected_type af;
  
  if (victim != ch){
    send_to_char("You can only protect your own mind.\n\r",ch);
    return;
  }
  if (affected_by_spell(ch,SPELL_MIND_BLOCK)){
    send_to_char("Your mind is protected already.\n\r",ch);
    return;
  }
  af.type = SPELL_MIND_BLOCK;
  af.duration  = 15 + GET_PROF_LEVEL(PROF_CLERIC,ch) * 2;
  af.modifier  = 0;
  af.location  = APPLY_NONE;
  af.bitvector = 0;
  
  affect_to_char(ch, &af);
  send_to_char("You create a magical barrier around your mind.\n\r", victim);
}



ASPELL(spell_insight){
  affected_type af;
  affected_type * afptr;
  int my_duration;

  
  if((type == SPELL_TYPE_ANTI) || is_object){
    
    affect_from_char(victim, SPELL_INSIGHT);
    
    if(type == SPELL_TYPE_ANTI) return;
  }

  if((victim != ch) && !is_object){
    //if(GET_PERCEPTION(victim)*2 < number(0,100)){
    if(saves_insight(victim, ch)) {
      act("You failed to affect $S mind.",FALSE, ch, 0, victim, TO_CHAR);
      return;
    }
  }
  if(is_object)
    my_duration = -1;
  else
    my_duration = 10 + level;

  if ((afptr = affected_by_spell(victim, SPELL_PRAGMATISM))) {
    if(!is_object && (number(0, afptr->duration) > number (0, my_duration))){
      act("You failed to break $S pragmatism.",FALSE, ch, 0, victim, TO_CHAR);
      return;      
    }
    else{
      act("You break $S pragmatism.",FALSE, ch, 0, victim, TO_CHAR);
      affect_from_char(victim, SPELL_PRAGMATISM);
    }
  }

  if (!affected_by_spell(victim, SPELL_INSIGHT)) {
    af.type      = SPELL_INSIGHT;
    af.duration  = my_duration;
    af.modifier  =  50;
    af.location  = APPLY_PERCEPTION;
    af.bitvector = 0;
    
    affect_to_char(victim, &af);
    send_to_char("The world seems to gain a few edges for you.\n\r", victim);
  }
  else{
      act("$E has insight already.",FALSE, ch, 0, victim, TO_CHAR);
      return;      
  }
}


ASPELL(spell_pragmatism){
  affected_type af;
  affected_type * afptr;
  int my_duration;

  if(victim != ch){
    if(GET_PERCEPTION(victim) < number(0,100)){
      act("You failed to affect $S mind.",FALSE, ch, 0, victim, TO_CHAR);
      return;
    }
  }
  if(is_object)
    my_duration = -1;
  else
    my_duration = 10 + level;

  if ((afptr = affected_by_spell(victim, SPELL_INSIGHT))) {
    if(number(0, afptr->duration) > number (0, my_duration)){
      act("You failed to break $S insight.",FALSE, ch, 0, victim, TO_CHAR);
      return;      
    }
    else{
      act("You break $S insight.",FALSE, ch, 0, victim, TO_CHAR);
      affect_from_char(victim, SPELL_INSIGHT);
    }
  }
    
  if (!affected_by_spell(victim, SPELL_PRAGMATISM)) {
    af.type      = SPELL_PRAGMATISM;
    af.duration  = 10 + level;
    if (GET_RACE(victim) != RACE_WOOD)
      af.modifier  =  -50;
    else
      af.modifier = - 100;
    af.location  = APPLY_PERCEPTION;
    af.bitvector = 0;
    
    affect_to_char(victim, &af);
    send_to_char("The world seems much duller..\n\r", victim);
  }
  else{
      act("$E is quite pragmatic already.",FALSE, ch, 0, victim, TO_CHAR);
      return;
  }
}

/*
 * Self Affection spells listed below in order
 * - Detect Hidden
 * - Detect Magic
 * - Evasion
 * - Resist Magic
 * - Slow Digestion
 * - Divination
 * - Infravision
 */

ASPELL(spell_detect_hidden)
{
  struct affected_type af;
  int loc_level, my_duration;
  
  if(!victim)
    return;
  
  if((type == SPELL_TYPE_ANTI) || is_object) {
    affect_from_char(victim, SPELL_DETECT_HIDDEN);
    if(type == SPELL_TYPE_ANTI) 
      return;
  }
  
  if(victim != ch)
    loc_level = GET_PROF_LEVEL(PROF_CLERIC, victim) + level;
  else 
    loc_level = level;
  
  if(is_object)
    my_duration = -1;
  else 
    my_duration = 3 * loc_level;
  
  if(!affected_by_spell(victim, SPELL_DETECT_HIDDEN)) {
    send_to_char("You feel your awareness improve.\n\r", victim);
    
    af.type      = SPELL_DETECT_HIDDEN;
    af.duration  = my_duration;
    af.modifier  = 0;
    af.location  = APPLY_NONE;
    af.bitvector = AFF_DETECT_HIDDEN;
    affect_to_char(victim, &af);
  }
}



ASPELL(spell_detect_magic)
{
  struct affected_type af;
  
  if(!victim) 
    victim = ch;

  if(affected_by_spell(victim, SPELL_DETECT_MAGIC) ) {
    if(victim == ch)
      send_to_char("You already can sense magic.\n\r",ch);
    else
      act("$E already can sense magic.\n\r", TRUE, ch, 0, victim, TO_CHAR);
    return;
  }

  af.type      = SPELL_DETECT_MAGIC;
  af.duration  = level * 5;
  af.modifier  = 0;
  af.location  = APPLY_NONE;
  af.bitvector = AFF_DETECT_MAGIC;
  
  affect_to_char(victim, &af);
  send_to_char("Your eyes tingle.\n\r", victim);
}



ASPELL(spell_evasion)
{
   struct affected_type af;
   int loc_level, my_duration;

   if(!victim) 
     return;
   
   if((type == SPELL_TYPE_ANTI) || is_object){
     affect_from_char(victim, SPELL_ARMOR);
     if(type == SPELL_TYPE_ANTI) return;
   }
   
   if(victim != ch)
     loc_level = (GET_PROF_LEVEL(PROF_CLERIC, victim) + level + 5) / 4;
   else 
     loc_level = (level + 5) / 2;
   
   if(is_object) 
     my_duration = -1;
   else
     my_duration = 12 + loc_level;
   
   if(!affected_by_spell(victim, SPELL_ARMOR)) {
     af.type      = SPELL_ARMOR;
     af.duration  = my_duration;
     af.modifier  =  loc_level;
     af.location  = APPLY_ARMOR;
     af.bitvector = AFF_EVASION;
     
     affect_to_char(victim, &af);
     send_to_char("You feel someone protecting you.\n\r", victim);
   }
}



ASPELL(spell_resist_magic)
{
  struct affected_type af;
  int loc_level;
  
  if(!victim)
    return;
  
  if(victim != ch)
    loc_level = (GET_PROF_LEVEL(PROF_CLERIC, victim) + level) / 2;
  else
    loc_level = level;
    
  if(!affected_by_spell(victim, SPELL_RESIST_MAGIC)) {
    af.type      = SPELL_RESIST_MAGIC;
    af.duration  = loc_level;
    af.modifier  = number(0, 1) + (loc_level / 5);
    af.location  = APPLY_SAVING_SPELL;
    af.bitvector = 0;
    
    affect_to_char(victim, &af);
    send_to_char("You feel yourself resistant to magic.\n\r", victim);
  }
}



ASPELL(spell_slow_digestion)
{
  struct affected_type af;
  int loc_level;

  if(!victim)
    return;
  
  if(victim != ch)
    loc_level = GET_PROF_LEVEL(PROF_CLERIC, victim) + level;
  else
    loc_level = level;
  
  if(!affected_by_spell(victim, SPELL_SLOW_DIGESTION)) {
    af.type      = SPELL_SLOW_DIGESTION;
    af.duration  = loc_level + 12;
    af.modifier  = loc_level;
    af.location  = APPLY_NONE;
    af.bitvector = 0;
    
    affect_to_char(victim, &af);
    send_to_char("You feel your stomach shrinking.\n\r", victim);
  }
}


ASPELL(spell_divination)
{
	// Ensure that we have a valid caster and location.
	if (ch == NULL || ch->in_room == NOWHERE)
		return;

	char buff[1000];
	
	const room_data& cur_room = world[ch->in_room];

	sprintf(buff, "You feel confident about your location.\n\r");
	sprintbit(cur_room.room_flags, room_bits, buf, 0);
	sprintf(buff, "%s (#%d) [ %s, %s], Exits are:\n\r", buff, cur_room.number, sector_types[cur_room.sector_type], buf);
	send_to_char(buff, ch);

	bool found = false;
	for (int dir = 0; dir < NUM_OF_DIRS; dir++)
	{
		room_direction_data* exit = cur_room.dir_option[dir];
		if (exit && exit->to_room != NOWHERE)
		{
			const room_data& exit_room = world[exit->to_room];

			found = true;
			sprintf(buff, "%5s: to %s (#%d)\n\r", dirs[dir], exit_room.name, exit_room.number);
			if (exit->exit_info != 0)
			{
				const char* keyword = exit->keyword ? exit->keyword : "";
				const char* key_name = "None";
				if (exit->key > 0)
				{
					int obj_index = real_object(exit->key);
					if (obj_index >= 0)
					{
						const obj_data& key = obj_proto[obj_index];
						key_name = key.short_description;
					}
					else
					{
						send_to_char("You found a key that shouldn't exist!  Please notify the imm group immediately at rots.management3791@gmail.com with your room name.", ch);
					}
				}

				sprintf(buff, "%s     door '%s', key '%s'.\n\r", buff, keyword, key_name);
			}
			send_to_char(buff, ch);
		}
	}
	if (!found)
	{
		send_to_char("None.\n\r", ch);
	}

	sprintf(buff, "Living beings in the room:\n\r");
	if (cur_room.people)
	{
		for (char_data* character = cur_room.people; character; character = character->next_in_room)
		{
			strcat(buff, GET_NAME(character));
			if (character->next_in_room)
			{
				strcat(buff, ", ");
			}
			else
			{
				strcat(buff, ".\n\r");
			}
		}
		send_to_char(buff, ch);
	}
	else
	{
		send_to_char("Living beings in the room:\n\r None.\n\r", ch);
	}

	if (cur_room.contents)
	{
		sprintf(buff, "Objects in the room:\n\r");
		for (obj_data* item = cur_room.contents; item; item = item->next_content)
		{
			strcat(buff, item->short_description);
			if (item->next_content)
			{
				strcat(buff, ", ");
			}
			else
			{
				strcat(buff, ".\n\r");
			}
		}
		send_to_char(buff, ch);
	}
	else
	{
		send_to_char("Objects in the room:\n\r None.\n\r", ch);
	}
}



ASPELL(spell_infravision){
  
struct affected_type af;
  
  
  if(!victim) victim = ch;

  if ( affected_by_spell(victim, SPELL_INFRAVISION) ){
    if(victim == ch)
      send_to_char("You already can see in the dark.\n\r",ch);
    else
      act("$E already can see in the dark.\n\r",TRUE, ch, 0, victim, TO_CHAR);
    return;
  }
  af.type      = SPELL_INFRAVISION;
  af.duration  = level ;
  af.modifier  = 0;
  af.location  = APPLY_NONE;
  af.bitvector = AFF_INFRARED;
  
  affect_to_char(victim, &af);
  send_to_char("Your eyes burn.\n\r", victim);

}


/*
 * Regeneration Spells listed below in order
 * - Resist poison
 * - Curing Saturation
 * - Restlessness
 * - Remove Poison
 * - Vitality
 * - Dispel Regeneration
 * - Regeneration
 */


ASPELL(spell_resist_poison)
{
  affected_type *af;
  affected_type newaf;
  
  if (!victim)
	victim = ch;
  af = affected_by_spell(victim, SPELL_POISON);
  if (af){
	if (affected_by_spell(victim, SPELL_RESIST_POISON)){
	  send_to_char("The poison is already being resisted.\n\r", ch);
	} else {
	  newaf.type = SPELL_RESIST_POISON;
	  newaf.duration = af->duration;
	  newaf.modifier = GET_PROF_LEVEL(PROF_CLERIC, ch);
	  newaf.location = APPLY_NONE;
	  newaf.bitvector = 0;
	  affect_to_char(victim, &newaf);
	  send_to_char("You begin to resist the poison.\n\r", victim);
	  if (victim != ch)
		send_to_char("They begin to resist the poison.\n\r", ch);
	}
  } else
	if (victim == ch)
	  send_to_char("But you have not been poisoned!\n\r", ch);
	else
	  send_to_char("But they are not poisoned!\n\r", ch);
}



ASPELL(spell_curing)
{
  struct affected_type af;
  int loc_level;

  if(!victim)
    return;
  
  if(victim != ch)
    loc_level = (GET_PROF_LEVEL(PROF_CLERIC, victim) + level + 5) / 2;
  else
    loc_level = level+5;
    
  if(!affected_by_spell(victim, SPELL_CURING)) {
    af.type      = SPELL_CURING;
    af.duration  = loc_level * (SECS_PER_MUD_HOUR * 4) / PULSE_FAST_UPDATE/2;
    af.modifier  = -loc_level;
    af.location  = APPLY_NONE;
    af.bitvector = 0;
    
    affect_to_char(victim, &af);
    send_to_char("You feel yourself becoming healthier.\n\r", victim);
  }
}



ASPELL(spell_restlessness)
{
  struct affected_type af;
  int loc_level;
  
  if(!victim)
    return;

  if(victim != ch)
    loc_level = (GET_PROF_LEVEL(PROF_CLERIC,victim) + level + 5) / 2;
  else 
    loc_level = level+5;
  
  if(!affected_by_spell(victim, SPELL_RESTLESSNESS)) {
    af.type      = SPELL_RESTLESSNESS;
    af.duration  = loc_level * (SECS_PER_MUD_HOUR * 4) / PULSE_FAST_UPDATE/2;
    af.modifier  = loc_level;
    af.location  = APPLY_NONE;
    af.bitvector = 0;
    
    affect_to_char(victim, &af);
    send_to_char("You feel yourself lighter.\n\r", victim);
  }
}



ASPELL(spell_remove_poison)
{
  
  if(!ch  || (!victim && !obj)){
    mudlog("remove_poison without all arguments.", NRM, 0, 0);
    return;
  }
  
  if (victim) {
    if (affected_by_spell(victim, SPELL_POISON)) {
      affect_from_char(victim, SPELL_POISON);
      act("A warm feeling runs through your body.", FALSE, victim, 0, 0, TO_CHAR);
      act("$N looks better.", FALSE, ch, 0, victim, TO_ROOM);
    }
  } else {
    if ((obj->obj_flags.type_flag == ITEM_DRINKCON) || 
	(obj->obj_flags.type_flag == ITEM_FOUNTAIN) || 
	(obj->obj_flags.type_flag == ITEM_FOOD)) {
      obj->obj_flags.value[3] = 0;
      act("The $p steams briefly.", FALSE, ch, obj, 0, TO_CHAR);
    }
  }
}


ASPELL(spell_vitality)
{
  struct affected_type af;
  struct affected_type *hjp;
  int loc_level;
  
  if(!victim)
    return;
  
  if(victim != ch)
    loc_level = (GET_PROF_LEVEL(PROF_CLERIC, victim) + level) / 2;
  else
    loc_level = level;
  
  loc_level = level;
  loc_level = loc_level/3 * (SECS_PER_MUD_HOUR * 4) / PULSE_FAST_UPDATE;
  
  hjp = affected_by_spell(victim, SPELL_VITALITY);
  if(!hjp) {
    af.type      = SPELL_VITALITY;
    af.duration  = loc_level;
    af.modifier  = 1;
    af.location  = APPLY_NONE;
    af.bitvector = 0;
    
    affect_to_char(victim, &af);
    send_to_char("You feel yourself becoming much lighter.\n\r", victim);
  }
  else if(hjp->duration < loc_level / 2) {
    hjp->duration = loc_level;
    send_to_char("You feel another surge of lightness.\n\r", victim);
    act("You renew $N's vitality.",FALSE, ch, 0, victim, TO_CHAR);
  }
  else {
    if(victim == ch)
      send_to_char("You are still as light as can be.\n\r",ch);
    else
      act("You could not improve $N's vitality.",
	  FALSE, ch, 0, victim, TO_CHAR);
  }
}


ASPELL(spell_dispel_regeneration)
{
  if(!victim) {
    send_to_char("Whom do you want to dispel?\n\r", ch);
    return;
  }
  
  if(victim == ch) {
    affect_from_char(victim, SPELL_RESTLESSNESS);
    affect_from_char(victim, SPELL_CURING);
    affect_from_char(victim, SPELL_REGENERATION);
    affect_from_char(victim, SPELL_VITALITY);
    return;  
  }

  if(victim != ch && !saves_mystic(victim))
    affect_from_char(victim, SPELL_RESTLESSNESS);
  if(victim != ch && !saves_mystic(victim))
    affect_from_char(victim, SPELL_CURING);
  if(victim != ch && !saves_mystic(victim))
    affect_from_char(victim, SPELL_REGENERATION);
  if(victim != ch && !saves_mystic(victim))
    affect_from_char(victim, SPELL_VITALITY);

  return;
}



ASPELL(spell_regeneration){
   struct affected_type af;
   struct affected_type * hjp;
    int loc_level;

    if(!victim) return;
    
    loc_level = level - 10;

    loc_level = loc_level/2 * (SECS_PER_MUD_HOUR * 4) / PULSE_FAST_UPDATE;

    hjp = affected_by_spell(victim, SPELL_REGENERATION);
    if(!hjp) {
      af.type      = SPELL_REGENERATION;
      af.duration  = loc_level;
      af.modifier  = 1;
      af.location  = APPLY_NONE;
      af.bitvector = 0;
      
      affect_to_char(victim, &af);
      send_to_char("You feel yourself becoming much healthier.\n\r", victim);
    }
    else if(hjp->duration < loc_level/2){
      hjp->duration = loc_level;
      send_to_char("You feel another surge of energy.\n\r",victim);
      act("You renew $N's regeneration.",FALSE, ch, 0, victim, TO_CHAR);
    }
    else{
      if(victim == ch)
	send_to_char("You are still regenerating fast enough.\n\r",ch);
      else
	act("You could not improve $N's regeneration.",
	    FALSE, ch, 0, victim, TO_CHAR);
    }
}


/*
 *  Offensive Spells listed below in order
 *  - Hallucinate
 *  - Haze 
 *  - Fear
 *  - Poison
 *  - Terror
 */


ASPELL(spell_hallucinate) 
{
  struct affected_type af;
  int loc_level, my_duration;
  int modifier;
  
  if(!victim) 
    return;
  
  loc_level = level;
  if(affected_by_spell(victim, SPELL_HALLUCINATE))
    send_to_char("They are already hallucinating!\n\r", ch);
  
 /*
  *  The modifier represents the number of times that the affected character
  * can "miss" the illusions of the characters/mobiles they're trying to hit.
  * Once they've "missed" this number of times, the effect will be removed.
  * If they hit the player before the modifier is 0, the effect will also be
  * removed.  A player of mystic level 31 or higher gets an additional +1
  * modifier.  A player specialization of Illusion give the player an
  * additional +1 modifier.
  */

 modifier = ((GET_PROF_LEVEL(PROF_CLERIC, ch) / 10) + 2)
           + ((GET_PROF_LEVEL(PROF_CLERIC, ch) > 30) ? 1 : 0)
           + ((GET_SPEC(ch) == PLRSPEC_ILLU) ? 1 : 0);
  my_duration = modifier * 4;

  if (!affected_by_spell(victim, SPELL_HALLUCINATE) &&
      (is_object || !saves_confuse(victim, ch))) {
    af.type      = SPELL_HALLUCINATE;
    af.duration  = my_duration;
    af.modifier  = modifier;
    af.location  = APPLY_NONE;
    af.bitvector = AFF_HALLUCINATE;
    
    affect_to_char(victim, &af);
    act("The world around you blurs and fades.\n\r", TRUE, victim, 0, ch, TO_CHAR);
    act("$n nervously glances around in confusion!",FALSE, victim, 0, 0, TO_ROOM);

    damage(ch, victim, 0, SPELL_HALLUCINATE, 0);
  }
}



ASPELL(spell_haze){
  struct affected_type af;
  int loc_level, my_duration, tmp;
  affected_type * tmpaf;

  if(!victim) return;

  if((type == SPELL_TYPE_ANTI) && is_object){
    for(tmpaf = ch->affected, tmp = 0;(tmp<MAX_AFFECT) && tmpaf;
	tmpaf = tmpaf->next, tmp++)
      if((tmpaf->type == SPELL_HAZE) && (tmpaf->duration == -1)) break;

    if(tmpaf) affect_remove(ch, tmpaf);

    return;
  }
  else if(type == SPELL_TYPE_ANTI){
    affect_from_char(victim, SPELL_HAZE);
    return;
  }

  loc_level = level;

  if(is_object) my_duration = -1;
    else my_duration = number(0,1); 

  if (!affected_by_spell(victim, SPELL_HAZE) &&
      (is_object || !saves_mystic(victim))) {
    af.type      = SPELL_HAZE;
    af.duration  = my_duration;
    af.modifier  = loc_level;
    af.location  = APPLY_NONE;
    af.bitvector = AFF_HAZE;

    affect_to_char(victim, &af);
    act("You feel dizzy as your surroundings seem to blur and twist.\n\r", TRUE, victim, 0, ch, TO_CHAR);
    act("$n staggers, overcome by dizziness!",FALSE, victim, 0, 0, TO_ROOM);
  }
}



ASPELL(spell_fear)
{
  struct affected_type af;
  
  if(!victim) {
    send_to_char("Whom do you want to scare?\n\r",ch);
    return;
  }

  if(victim == ch) {
    send_to_char("You look upon yourself.\n\rYou are scared to death.\n\r",ch);
    return;
  }

  if(!IS_NPC(ch) && !IS_NPC(victim) && RACE_GOOD(ch) && RACE_GOOD(victim) &&
     GET_LEVEL(ch) < LEVEL_IMMORT && GET_LEVEL(victim) < LEVEL_IMMORT &&
     ch != victim) {
    act("$N is not scared by your spell.",
	FALSE, ch, 0, victim, TO_CHAR);
    act("You are not scared by $n's spell.", 
	FALSE, ch, 0, victim, TO_VICT);
    act("$n attempts to scare $N but fails.",
	FALSE, ch, 0, victim, TO_NOTVICT);
    return;
  }
  
  if(!affected_by_spell(victim, SPELL_FEAR) && 
     !saves_mystic(victim) && !saves_leadership(victim)) {
    af.type      = SPELL_FEAR;
    af.duration  = level;
    af.modifier  = level+10;
    af.location  = APPLY_NONE;
    af.bitvector = 0;
    
    affect_to_char(victim, &af);
    if((GET_RACE(ch) == RACE_URUK) || (GET_RACE(ch) == RACE_ORC)) {
      act("$n breathes a vile, putrid breath onto you. "
	  "Fear races through your heart!",
	  FALSE, ch, 0, victim, TO_VICT);
      act("$n breathes a vile, putrid breath at $N. $N is scared!",
	  TRUE, ch,0, victim, TO_NOTVICT);
    }
    else {
      act("$n breathes an icy, cold breath onto you. "
	  "Fear races through your heart!",
	  FALSE, ch, 0, victim, TO_VICT);
      act("$n breathes an icy, cold breath at $N; $N is scared!", 
	  TRUE, ch, 0, victim, TO_NOTVICT);
    }
    act("$N looks shocked for a brief moment before the panic sets in.",
	FALSE, ch, 0, victim, TO_CHAR);
  }
  else
    act("$N ignores your breath.", FALSE, ch, 0, victim, TO_CHAR);

  return;
}



ASPELL(spell_poison)
{
  struct affected_type af;
  struct affected_type * oldaf;
  int magus_save = 0;

  if(!victim && !obj && !(ch->specials.fighting)){ /*poisoning the room*/

    if(!ch) return;

    af.type = ROOMAFF_SPELL;
    af.duration = (level)/3;
    af.modifier = level/2;
    af.location = SPELL_POISON;
    af.bitvector = 0;
    
    if ((oldaf = room_affected_by_spell(&world[ch->in_room], SPELL_POISON))) {
      
      if(oldaf->duration < af.duration)
	oldaf->duration = af.duration;
      
      if(oldaf->modifier < af.modifier)
	oldaf->modifier = af.modifier;
    }
    else{
      affect_to_room(&world[ch->in_room], &af);
    }

    act("$n breathes out a cloud of smoke.",TRUE, ch,0,0, TO_ROOM);
    send_to_char("You breathe out poison.\n\r",ch);

    return;
  }

  if (GET_POSITION(ch) == POSITION_FIGHTING) victim = ch->specials.fighting;

  if (victim) {

    if (!saves_poison(victim, ch) && (number(0, magus_save) < 50)) {
      af.type = SPELL_POISON;
      af.duration = level + 1;
      af.modifier = -2;
      af.location = APPLY_STR;
      af.bitvector = AFF_POISON;
      
      affect_join(victim, &af, FALSE, FALSE);
      
      send_to_char("You feel very sick.\n\r", victim);
      damage((ch)?ch:victim, victim, 5, SPELL_POISON,0);
    }
    
  } else { /* Object poison */
    if ((obj->obj_flags.type_flag == ITEM_DRINKCON) || 
	(obj->obj_flags.type_flag == ITEM_FOUNTAIN) || 
	(obj->obj_flags.type_flag == ITEM_FOOD)) {
      obj->obj_flags.value[3] = 1;
    }
  }
}


ASPELL(spell_terror)
{
  struct char_data *tmpch;
  struct affected_type af;
  
  if(!ch || (ch->in_room == NOWHERE)) 
    return;

  send_to_char("You breathe an icy, cold breath across the room.\n\r",
	       ch);
  
  for(tmpch = world[ch->in_room].people; tmpch; tmpch = tmpch->next_in_room) {
    if((tmpch != ch) && !affected_by_spell(tmpch, SPELL_FEAR)) {
      if(!saves_mystic(tmpch) && !saves_leadership(tmpch)) {
	af.type      = SPELL_FEAR;
	af.duration  = level;
	af.modifier  = level+10;
	af.location  = APPLY_NONE;
	af.bitvector = 0;
	
	affect_to_char(tmpch, &af);
	act("$n suddenly breathes an icy, cold breath everywhere. "
	    "Terror overcomes you.", FALSE, ch, 0, tmpch, TO_VICT);
      }
      else
	act("$n suddenly breathes an icy, cold breath. You ignore it.",
	    FALSE, ch, 0, tmpch, TO_VICT);
    }
  }
}


/*
 * Misc Mystic Spells listed below 
 *  - Attune
 *  - Sanctuary
 *  - Enchant Weapon
 *  - Death Ward
 *  - Confuse
 *  - Guardian
 *  - Shift -should we remove this spell completely?
 *  - Protection
 */

ASPELL(spell_attune)
{
  struct obj_data *object;
  
  object = ch->equipment[WIELD];
  if(!(object)) {
    send_to_char("But you are not wielding a weapon!\n\r", ch);
    return;
  }
  
  SET_BIT(object->obj_flags.extra_flags, ITEM_WILLPOWER);
  object->obj_flags.prog_number = 1;
  send_to_char("You attune your mind to your weapon.\n\r", ch);
}


ASPELL(spell_sanctuary)
{
  struct affected_type af;
  int loc_level;
  
  if (!victim) 
    return;

  if (affected_by_spell(ch, SPELL_ANGER)) {
    send_to_char("Your mind is blinded by anger. "
		 "Try again when you have cooled down.\n\r", ch);
    return;
  }
  if (affected_by_spell(victim, SPELL_ANGER)) {
    send_to_char("Your victim's negative energy resists your"
		 " attempts to form your spell.\r\n", ch);
    return;
  }
   
  if (ch == victim)
    loc_level = GET_PROF_LEVEL(PROF_CLERIC, ch);
  else
    loc_level = (MAX(6, GET_PROF_LEVEL(PROF_CLERIC, victim)));
  
  if (!affected_by_spell(victim, SPELL_SANCTUARY)) {
    af.type      = SPELL_SANCTUARY;
    af.duration  = loc_level;
    af.modifier  = GET_ALIGNMENT(ch);
    af.location  = APPLY_NONE;
    af.bitvector = AFF_SANCTUARY;
    
    affect_to_char(victim, &af);
    send_to_char("You are surrounded by a bright aura.\n\r", victim);
    act("$n is surrounded by a bright aura.",FALSE, victim, 0, 0, TO_ROOM);
  }
}





ASPELL(spell_enchant_weapon)
{
  int i, bonus = 0;
  
  if(!ch || !obj)
    return;
  
  assert(MAX_OBJ_AFFECT >= 2);
  
  if((GET_ITEM_TYPE(obj) == ITEM_WEAPON) && 
     !IS_SET(obj->obj_flags.extra_flags, ITEM_MAGIC)) {
    
    for(i = 0; i < MAX_OBJ_AFFECT; i++)
      if(obj->affected[i].location != APPLY_NONE) {
	send_to_char("There is too much magic in it already.\n\r", ch);
	return;
      }
    
    SET_BIT(obj->obj_flags.extra_flags, ITEM_MAGIC);
    bonus = 6;
    obj->affected[0].location = APPLY_OB;
    obj->affected[0].modifier = bonus;
    
    if(IS_GOOD(ch)) {
      SET_BIT(obj->obj_flags.extra_flags, ITEM_ANTI_EVIL);
      act("$p glows blue.", FALSE, ch, obj, 0, TO_CHAR);
    } 
    else if(IS_EVIL(ch)) {
      SET_BIT(obj->obj_flags.extra_flags, ITEM_ANTI_GOOD);
      act("$p glows red.", FALSE, ch, obj, 0, TO_CHAR);
    } 
    else
      act("$p glows yellow.", FALSE, ch, obj, 0, TO_CHAR);
  }
}


ASPELL(spell_death_ward){
  affected_type af;
  
  if((type == SPELL_TYPE_ANTI) || is_object){
 
    /* XXX: What the fuck? */
    affect_from_char(victim, SPELL_INSIGHT);
 
    if(type == SPELL_TYPE_ANTI) return;
  }

  if (!affected_by_spell(victim, SPELL_DEATH_WARD)) {
    af.type      = SPELL_DEATH_WARD;
    af.duration  = (is_object)? -1 : level*2;
    af.modifier  =  level/2;
    af.location  = 0;
    af.bitvector = 0;
 
    affect_to_char(victim, &af);
    send_to_char("You feel a ward being woven around you.\n\r", ch);
  }
  else{
    send_to_char("You are warded already!\n\r", ch);
  }
  return;
}




ASPELL(spell_confuse){
  struct affected_type af;
  int loc_level, my_duration, tmp;
  affected_type * tmpaf;
  int modifier;
  
  if(!victim) return;
  
  if((type == SPELL_TYPE_ANTI) && is_object){
    for(tmpaf = ch->affected, tmp = 0;(tmp<MAX_AFFECT) && tmpaf;
	tmpaf = tmpaf->next, tmp++)
      if((tmpaf->type == SPELL_CONFUSE) && (tmpaf->duration == -1)) break;
    
    if(tmpaf) affect_remove(ch, tmpaf);
    
    return;
  }
  else if(type == SPELL_TYPE_ANTI){
    affect_from_char(victim, SPELL_CONFUSE);
    return;
  }
  
  loc_level = level;
  
  if(is_object) my_duration = -1;
    my_duration = 10 + level;
   
    modifier = 1; 

  if (!affected_by_spell(victim, SPELL_CONFUSE) &&
      (is_object || !saves_confuse(victim, ch))) {
    af.type      = SPELL_CONFUSE;
    af.duration  = my_duration;
    af.modifier  = modifier;
    af.location  = APPLY_NONE;
    af.bitvector = AFF_CONFUSE;
    
    affect_to_char(victim, &af);
    act("Strange thoughts stream through your mind, making it hard to concentrate.\n\r", TRUE,
	victim,0, ch, TO_CHAR);
    act("$n appears to be confused!",FALSE, victim, 0, 0, TO_ROOM);
  }
}

ASPELL(spell_guardian){
  /* 
   * Guardian now takes an extra
   * argument from the character on cast
   * The argument determines the type of
   * Guardian mob cast by the user
   */ 
 
 static char *guardian_type[] = {
    "aggressive",
    "defensive",
    "mystic",
    "\n"
 };
  char_data * tmpch;
  char_data * guardian;
  char first_word[255];
  int guardian_to_load;
  int guardian_num = 1110;
  
 if (GET_SPEC(ch) != PLRSPEC_GRDN) {
    send_to_char ("You are not dedicated enough to cast this.\r\n",ch);
    return;
  }

  /*
   * Takes the extra arguement from the user
   * Checks if its a valid arguement
   * Loads appropriate guardian number according
   * to race/type.
   */ 
   
 one_argument(arg, first_word);
 guardian_to_load = search_block(first_word, guardian_type, 0);
   
 if (guardian_to_load == -1) {
    send_to_char ("You'll have to be more specific about "
                  "which guardian you wish to summon.\n",ch);
    GET_SPIRIT(ch) +=  30;
    return;
   }
 else
   guardian_num = guardian_mob[GET_RACE(ch)][guardian_to_load];
  
 if (ch->in_room == NOWHERE) return;
    for (tmpch = character_list; tmpch; tmpch = tmpch->next)
        if ((tmpch->master == ch) && IS_GUARDIAN(tmpch))
        break;
 
 if (tmpch) {
    send_to_char("You already have a guardian.\n\r",ch);
    return;
   }

 if (!(guardian = read_mobile(guardian_num, VIRT))) {
    send_to_char("Could not find a guardian for you, please report.\n\r",ch);
    return;
  }

 char_to_room(guardian, ch->in_room);
 act("$n appears with a flash.",FALSE, guardian, 0, 0, TO_ROOM);
 add_follower(guardian, ch, FOLLOW_MOVE);
 SET_BIT(guardian->specials.affected_by, AFF_CHARM);
 SET_BIT(MOB_FLAGS(guardian), MOB_PET);
}


ASPELL(spell_shift){
  follow_type * tmpfol;

  if (GET_LEVEL(ch) < LEVEL_IMMORT)
  {
    send_to_char("You try to shift, but only give yourself a headache.\n\r", ch);
    return;
  }

  if (ch->specials.fighting){
	send_to_char("You need to be still to cast this!\n\r", ch);
	return;
  }

  if ((GET_LEVEL(ch) < LEVEL_IMMORT) && (victim))
  {
    send_to_char("You can only cast this on yourself!\n\r", ch);
    return;
  }

  if (IS_SET(PLR_FLAGS(victim), PLR_ISSHADOW)){
	REMOVE_BIT(PLR_FLAGS(victim), PLR_ISSHADOW);
	GET_COND(victim, FULL) = 1;
	GET_COND(victim, THIRST) = 1;
  } else {
	SET_BIT(PLR_FLAGS(victim), PLR_ISSHADOW);
	if (IS_RIDING(victim))
	  stop_riding(victim);
	if (affected_by_spell(victim, SPELL_MIND_BLOCK))
	  affect_from_char(victim, SPELL_MIND_BLOCK);
	if (affected_by_spell(victim, SPELL_SANCTUARY))
	  affect_from_char(victim, SPELL_SANCTUARY);
	for (tmpfol = victim->followers; tmpfol; tmpfol = victim->followers)
      stop_follower(tmpfol->follower, FOLLOW_MOVE);
	if(victim->group_leader)
       stop_follower(victim, FOLLOW_GROUP);
	if(IS_AFFECTED(victim,AFF_HUNT))
	  REMOVE_BIT(victim->specials.affected_by, AFF_HUNT);
	if(IS_AFFECTED(victim,AFF_SNEAK))
	  REMOVE_BIT(victim->specials.affected_by, AFF_SNEAK); 
  }
}




ASPELL(spell_protection){
  static char * protection_sphere[]={
    "fire",
    "cold",
    "lightning",
    "physical",
    "\n"
  };
  
  char_data * loc_victim;
  int res;
  char first_word[255], second_word[255];
  affected_type newaf;

  if(strlen(arg) >= 255) arg[254] = 0;

  half_chop(arg, first_word, second_word);
  fprintf(stderr, "protection: %s, %s\n", first_word, second_word);

  res = search_block(first_word, protection_sphere, 0);

  one_argument(second_word, first_word); // first_word now has the victim

  if(!*first_word)
    loc_victim = ch;
  else
    loc_victim = get_char_room_vis(ch, first_word, 0);

  if(!loc_victim){
    send_to_char("Nobody here by that name.\n\r",ch);
    return;
  }

  if(affected_by_spell(loc_victim, SPELL_PROTECTION, 0)){
    if(loc_victim == ch)
      send_to_char("You have protection already.\n\r",ch);
    else
      act("$N has $S protection already.",FALSE, ch, 0, loc_victim, TO_CHAR);

    return;
  }

  

  switch(res){
  case -1:
    send_to_char("You can master protection from fire, cold, lightning or physical only.\n\r", ch);
    break;

  case 0: /* fire */

    newaf.type      = SPELL_PROTECTION;
    newaf.duration  = (is_object)? -1 : level*2;
    newaf.modifier  = PLRSPEC_FIRE;
    newaf.location  = APPLY_RESIST;
    newaf.bitvector = 0;

    affect_to_char(loc_victim, &newaf);
    send_to_char("You feel resistant to fire!\n\r", loc_victim);

    if(ch != loc_victim)
      act("You grant $N resistance to fire.", FALSE, ch, 0, loc_victim, TO_CHAR);

    break;
  case 1: /* cold */

    newaf.type      = SPELL_PROTECTION;
    newaf.duration  = (is_object)? -1 : level*2;
    newaf.modifier  = PLRSPEC_COLD;
    newaf.location  = APPLY_RESIST;
    newaf.bitvector = 0;

    affect_to_char(loc_victim, &newaf);
    send_to_char("You feel resistant to cold!\n\r", loc_victim);

    if(ch != loc_victim)
      act("You grant $N resistance to cold.", FALSE, ch, 0, loc_victim, TO_CHAR);
  
    break;

  case 2:  /*lightning*/
    newaf.type      = SPELL_PROTECTION;
    newaf.duration  = (is_object)? -1 : level*2;
    newaf.modifier  = PLRSPEC_LGHT;
    newaf.location  = APPLY_RESIST;
    newaf.bitvector = 0;

    affect_to_char(loc_victim, &newaf);
    send_to_char("You feel resistant to lightning!\n\r", loc_victim);

    if(ch != loc_victim)
      act("You grant $N resistance to lightning.", FALSE, ch, 0, loc_victim, TO_CHAR);

    break;
  
  case 3: /* physical */
    newaf.type      = SPELL_PROTECTION;
    newaf.duration  = (is_object)? -1 : level*2;
    newaf.modifier  = PLRSPEC_WILD;
    newaf.location  = APPLY_RESIST;
    newaf.bitvector = 0;

    affect_to_char(loc_victim, &newaf);
    send_to_char("You feel resistant to physical harm!\n\r", loc_victim);

    if(ch != loc_victim)
      act("You grant $N resistance to physical harm.", FALSE, ch, 0, loc_victim, TO_CHAR);

    break;

  default: return;
  };


}

