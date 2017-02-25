/* ************************************************************************
*   File: mage.c                                        Part of CircleMUD *
*  Usage: actual effects of mage only spells                              *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993 by the Trustees of the Johns Hopkins University     *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

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
#include "zone.h"  /* For zone_table */
/*
 * Mage level for offensive spell damage (not saves).
 * Note that here you get 3 levels per 10 int, rather than 2 for saves
 */
#define MAG_POWER(ch) (level + (GET_MAX_RACE_PROF_LEVEL(PROF_MAGE, ch) * GET_LEVELA(ch)/30) + GET_INT(ch)/5 + (number(0,GET_INT(ch)%5) ? 1 : 0))

#define RACE_SOME_ORC(ch) ((GET_RACE(ch) == RACE_URUK || GET_RACE(ch) == RACE_ORC || GET_RACE(ch) == RACE_MAGUS))


/*
 * external structures 
 */

extern struct room_data world;
extern struct obj_data *obj_proto;
extern struct obj_data *object_list;
extern struct char_data *character_list;
extern int rev_dir[];
extern int top_of_world;
extern char *  dirs[];
extern char * refer_dirs[];
extern char     *room_bits[];
extern char *   sector_types[];


/*
 * external functions
 */
void list_char_to_char(struct char_data *list, struct char_data *ch, int mode);
char saves_spell(struct char_data *, sh_int, int);
//void do_stat_object(struct char_data *ch, struct obj_data *j);
void stop_hiding(struct char_data *ch, char);
ACMD (do_look);
void do_identify_object (struct char_data *, struct obj_data *);

/*
 *Spells are listed below have been split into five categories.
 * - Non Offensive spells.
 * - Teleportation spells.
 * - Offensive spells.
 * - Uruk Lhuth spells.
 * - Specilization spells.
 *Each group of spells is seperated by a double commented striaght line, with the group
 * name and which spells are withing the code segement.
 *Each single Spell function is seperated by a _SINGLE_ commented line, under which the
 * spells general details will commented.
 *I'm quite aware at the fact that this is probably not needed, but i think this is a 
 * good practice, as it makes code easily readable. 
 * Khronos 26/03/05
 */

/*-----------------------------------------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------------------------------*/
/*
 * Non Offensive Spells
 * Spells in this segement in order
 * - Create Light
 * - Locate Living (plus the locate life coordinates function)
 * - Cure Self
 * - Detect Evil
 * - Reveal Life
 * - Shield
 * - Flash
 * - Vitalize Self
 * - Summon
 * - Identify
*/


/*-----------------------------------------------------------------------------------------------------------*/
/*
 * Spell Create light
 * Basically loads object 7006 (a brilliant orb of light) to the caster
 */

ASPELL (spell_create_light){
  struct obj_data *tmpobj;
  int object_vnum = 7006;

  if (!ch) return;
  
  object_vnum = real_object(object_vnum);
  tmpobj = read_object(object_vnum, REAL);
  
  if (!tmpobj){
    send_to_char ("Spell Error - Notify Imms.\r\n",ch);
    return;
  }
  
  obj_to_char (tmpobj, ch);
  act ("$p materializes in your hands.", FALSE, ch, tmpobj, 0, TO_CHAR);
  return;
}
  
/*----------------------------------------------------------------------------------------------------------*/
/*
 *These functions  provide the coordinates for the locate life spell
 */
struct loclife_coord{
  int number;   // room number in world[]
  signed char n;       //
  signed char e;       // north/east/up coordinates
  signed char u;       //
};

int loclife_add_rooms(loclife_coord room, loclife_coord * roomlist, 
		      int * roomnum, int room_not){
  int tmp, dmp, count;
  int dirarray[NUM_OF_DIRS];
  struct room_direction_data * exptr;
  
  for (tmp = 0; tmp < NUM_OF_DIRS; tmp++) dirarray[tmp] = tmp;
 
  reshuffle(dirarray, NUM_OF_DIRS);
  count = 0;

  for(dmp = 0; dmp < NUM_OF_DIRS; dmp++){
    exptr = world[room.number].dir_option[dirarray[dmp]];
   
  if(exptr){
      if(!(IS_SET(exptr->exit_info,EX_CLOSED) && 
	   IS_SET(exptr->exit_info,EX_DOORISHEAVY)) &&
	   (exptr->to_room != NOWHERE) && (exptr->to_room != room_not)){
	      for(tmp = 0; tmp < *roomnum; tmp++)
		if(roomlist[tmp].number == exptr->to_room) break;
	
	      if((tmp == *roomnum) && (*roomnum < 254)){
		roomlist[*roomnum].number = exptr->to_room;
		roomlist[*roomnum].n = room.n;
		roomlist[*roomnum].e = room.e;
		roomlist[*roomnum].u = room.u;
		switch(dirarray[dmp]){
		case 0: /* north */ roomlist[*roomnum].n++; break;
		case 1: /* east  */ roomlist[*roomnum].e++; break;
		case 2: /* south */ roomlist[*roomnum].n--; break;
		case 3: /* west  */ roomlist[*roomnum].e--; break;
		case 4: /* up    */ roomlist[*roomnum].u++; break;
		case 5: /* down  */ roomlist[*roomnum].u--; break;
	      };
	  (*roomnum)++;
	  count++;
	 }
      }
    }
  }
  return count;
}
char * loclife_dirnames[216] = {
  /*
   *Lines - up/down here - farthest down - farthest up
   *columns - east/west
   * groups - north/south
   * (n/s, e/w, u/d)
   */
  "here", "down", "down", "up", "up",
  "west", "west and down", "west and down", "west and up", "up and west and up",
  "west", "west and down", "west and down", "west and up", "up and west and up",
  "east", "east and down", "east and down", "east and up", "up and east and up",
  "east", "east and down", "east and down", "east and up", "up and east and up",

  "south", "south and down", "south and down", "south and up", "up and south",
  "southwest", "southwest and down", "southwest and down", "southwest and up", "up and southwest",
  "west-southwest", "west-southwest and down", "west-southwest and down", "west-southwest and up", "up and west-southwest",
  "southeast",  "southeast and down", "southeast and down", "southeast and up", "up and southeast",
  "east-southeast", "east-southeast and down", "east-southeast and down", "east-southeast and up", "up and east-southeast",

  "south", "south and down", "south and down", "south and up", "up and south",
  "south-southwest", "south-southwest and down", "south-southwest and down", "south-southwest and up", "up and south-southwest",
  "southwest", "southwest and down", "southwest and down", "southwest and up", "up and southwest",
  "south-southeast", "south-southeast and down", "south-southeast and down", "south-southeast and up", "up and south-southeast",
  "southeast",  "southeast and down", "southeast and down", "southeast and up", "up and southeast",

  "north", "north and down", "north and down", "north and up", "up and north",
  "northwest", "northwest and down", "northwest and down", "northwest and up", "up and northwest",
  "west-northwest", "west-northwest and down", "west-northwest and down", "west-northwest and up", "up and west-northwest",
  "northeast", "northeast and down", "northeast and down", "northeast and up", "up and northeast",
  "east-northeast", "east-northeast and down", "east-northeast and down", "east-northeast and up", "up and east-northeast",

  "north", "north and down", "north and down", "north and up", "up and north and up",
  "north-northwest", "north-northwest and down", "north-northwest and down", "north-northwest and up", "up and north-northwest",
  "northwest", "northwest and down", "northwest and down", "northwest and up", "up and northwest",
  "north-northeast",  "anorth-northeast nd down", "north-northeast and down", "north-northeast and up", "up and north-northeast",
  "northeast", "northeast and down", "northeast and down", "northeast and up", "up and northeast",

};
char * loclife_dir_convert(loclife_coord rm){
  signed char nr,er,ur;

  nr = abs(rm.n); er = abs(rm.e); ur = abs(rm.u);

  if(nr > 2) nr = 2;
  if(er > 2) er = 2;
  if(ur > 2) ur = 2;

  if(rm.n > 0) nr += 2;
  if(rm.e > 0) er += 2;
  if(rm.u > 0) ur += 2;

  return loclife_dirnames[(nr*5 + er) * 5 + ur];
}


/*--------------------------------------------------------------------------------------------------------*/

/*
 * Spell locate living
 * Uses the above functions to search for mobs within the
 * casters immediate area, then displays the result
 */
ASPELL(spell_locate_living){
  struct char_data *mobs;
  int isscanned[255];
  int tmp, tmp2, roomnum, num, notscanned, bigcount,roomrange, mobrange;
  char buf[255];
  loclife_coord roomlist[255];
  loclife_coord new_room;
 
  if(ch->in_room == NOWHERE) return;

  for(tmp = 0; tmp < 255; tmp++) 
    roomlist[tmp].number = roomlist[tmp].n = roomlist[tmp].e =
      roomlist[tmp].u = isscanned[tmp] = 0;
     roomnum = 0;

  roomrange = 5 + level / 3;
  mobrange = 2 + level / 3;

  new_room.number = ch->in_room;
  new_room.n = new_room.e = new_room.u = 0;
  notscanned = loclife_add_rooms(new_room, roomlist, &roomnum, 
				 ch->in_room);

  bigcount = 0;
  while((notscanned > 0) && (bigcount < 255) && (roomnum < roomrange)){
    num = number(0,notscanned-1);
    for(tmp = 0, tmp2 = 0; (tmp2 < num) || (isscanned[tmp]); tmp++)
      if(!isscanned[tmp]) tmp2++;

    isscanned[tmp] = 1;
    notscanned += -1 + loclife_add_rooms(roomlist[tmp], roomlist, &roomnum,
					 ch->in_room);
    bigcount++;
  }

  bigcount = 0;
  for(tmp = 0; (tmp < roomnum) && (bigcount < mobrange); tmp++){
    mobs = world[roomlist[tmp].number].people;
    while(mobs && (bigcount < mobrange)){
      sprintf(buf,"%s at %s to the %s.\n\r",
	      (IS_NPC(mobs)?GET_NAME(mobs):pc_star_types[mobs->player.race]),
	      world[roomlist[tmp].number].name, 
	      loclife_dir_convert(roomlist[tmp]));
      send_to_char(buf,ch);
      bigcount++;
      mobs = mobs->next_in_room;
    }
  }
  if(bigcount)
    send_to_char("You could not further concentrate.\n\r",ch);
  else
    send_to_char("The area seems to be empty.\n\r",ch);
  
  return;
}

/*----------------------------------------------------------------------------------------------------------*/
/*
 * Self explanatory this one
 * Simply adds hit points to your current hit points
 */

ASPELL(spell_cure_self)
{
  if(GET_HIT(ch) >= GET_MAX_HIT(ch)) {
    send_to_char("You are already healthy.\n\r",ch);
    return;
  }
  
  GET_HIT(ch) = MIN(GET_MAX_HIT(ch), GET_HIT(ch) +  level/2 + 10);
  send_to_char("You feel better.\n\r",ch);
  return;
}


/*----------------------------------------------------------------------------------------------------------*/

/*
 * Gets the zone allignment, the same as our mage sense ability
 * Do we need this as a spell, Does anyone even use it?
 */

ASPELL(spell_detect_evil)
{
  if(report_zone_power(ch) == -1)
    send_to_char("Evil power is very strong in this place.\n\r", ch);
  else
    send_to_char("You cannot detect the presence of evil here.\n\r", ch);
}  

/*----------------------------------------------------------------------------------------------------------*/
/*
 * Saving a reveal spell:
 * Saving a reveal spell is a tricky deal, since we're dealing
 * with so many factors.  These are:
 *   - the hider's spell save: half of the hider's spell save
 *     is effective when determining a save against a reveal
 *     spell; this is accomplished by adding a negative bonus
 *     to the saves_spell function.
 *   - the hider's hide level: for every x points of hide level
 *     the hider is bonused by one spell save.  This x is
 *     dependent on the spell being cast; word of sight has a
 *     slightly higher x, thus making it harder to save.
 *   - the LEVELA/GET_PROF_MAGE difference: since the LEVELA /
 *     GET_PROF_MAGE difference is so extreme (in order to give
 *     offensive mid-ranged mages a much harder time), we have
 *     to give these mid-mages a bit of a bonus for reveal,
 *     otherwise, it'd be absolutely useless at lower levels,
 *     and this is not the intent of the LEVELA/GPM difference.
 *     We achieve this difference in the 'negative bonus' way.
 *
 * Additionally, when a reveal spell saves, it has a decent
 * chance to lower the hider's GET_HIDING value (hide level);
 * this chance is slightly higher for word of sight than it is
 * for reveal life.
 */

ASPELL(spell_reveal_life)
{
  struct char_data *hider;
  int found, hider_bonus;
  
  if(ch->in_room == NOWHERE) 
    return;
  
  act("Suddenly, a flash of intense light floods your surroundings.",
      TRUE, ch, 0, 0, TO_ROOM);
  send_to_char("A surge of light reveals to you every corner of the room.\n\r",
	       ch);
  
  for(hider = world[ch->in_room].people, found = 0; hider; 
      hider = hider->next_in_room) {
    if(hider != ch) {
      if(GET_HIDING(hider) > 0) {
	hider_bonus = GET_HIDING(hider) / 25;
	hider_bonus += number(0, GET_HIDING(hider)) % 25 ? 1 : 0;
	hider_bonus -= (30 - level) / 3;  /* correction for midmages */
	if(!saves_spell(hider, level, hider_bonus)) {
	  stop_hiding(hider, FALSE);
	  send_to_char("You've been discovered!\r\n", hider);
	  found = 1;
	}
	else if(!number(0, GET_HIDING(hider) / 35)) {
	  send_to_char("The intense glow fades, and you remain hidden; "
		       "yet you feel strangely insecure.\r\n", hider);
	  GET_HIDING(hider) -= GET_PROF_LEVEL(PROF_MAGE, ch) / 3;
	}
	else
	  send_to_char("The bright light fades, and you seem to have "
		       "remained undiscovered.\r\n", hider);
      }
      /* not hiding? no contest, we found you */
      else
	found = 1;
    }
  }
  
  if(!found)
    send_to_char("The place seems empty.\n\r",ch);
  else
    list_char_to_char(world[ch->in_room].people, ch, 0);
  
  return;
}

/*----------------------------------------------------------------------------------------------------------*/
/* Spell Shield
 * Gives the caster a magical shield that absorbs damage
 * which is currently hugely disproportioned to the actual 
 * ammount of mana lose, making this spell as it stands very 
 * overpowered. IT NEEDS TO BE CHANGED
 * Khronos 27/03/05
 */

ASPELL(spell_shield)
{
  struct affected_type af;
  
  if(!victim) victim = ch;
  if (affected_by_spell(victim, SPELL_SHIELD)) {
    send_to_char("You are already protected by a magical shield.\n\r", ch);
    return;
  }
  
  af.type       = SPELL_SHIELD;
  af.duration   = level + 5;
  af.modifier   = level * 5;   /* % of HP */
  af.bitvector  = AFF_SHIELD;
  af.location   = APPLY_NONE;

  affect_to_char(victim, &af);
  send_to_char("You surround yourself with a magical shield.\n\r", victim);
}


/*----------------------------------------------------------------------------------------------------------*/

/*
 * Flash Spell Whitie Only
 * Disengages the caster from their victim
 * Gives darkie races POA when cast in the same room as them
 * Should we prehaps give Darkies a similar ability?
 */

ASPELL(spell_flash){
  char_data * tmpch;
  affected_type * tmpaf;
  affected_type newaf;
  int afflevel,maxlevel;

  if(ch->in_room < 0) return;

  for(tmpch = world[ch->in_room].people; tmpch; tmpch = tmpch->next_in_room){
    if(tmpch != ch) 
      send_to_char("A blinding flash of light makes you dizzy.\n\r",tmpch);
    if(tmpch->specials.fighting)
      GET_ENERGY(tmpch) -= 400;
    if(tmpch->specials.fighting == ch)  GET_ENERGY(tmpch) -= 400;
    if(!saves_spell(tmpch,level,0)){
          //  6-11-01 Errent attempts to make flash give power of arda to darkies - look out!
          if(RACE_EVIL(tmpch)){
            afflevel = 50;
            maxlevel = afflevel * 25;
            tmpaf = affected_by_spell(tmpch,SPELL_ARDA);
            if (tmpaf){
              if (tmpaf->modifier > maxlevel)
                tmpaf->modifier = MAX(tmpaf->modifier - afflevel, 30);
              else
                tmpaf->modifier = MIN(tmpaf->modifier + afflevel, 400);

              tmpaf->duration = 100;  // to keep the affection going

            } else {
              newaf.type = SPELL_ARDA;
              newaf.duration = 400;  // immaterial this figure
              newaf.modifier = afflevel;
              newaf.location = APPLY_NONE;
              newaf.bitvector = 0;
              affect_to_char(tmpch, &newaf);
              send_to_char("The power of Arda weakens your body.\n\r", tmpch);
            }

          }

      if(tmpch->specials.fighting){
	stop_fighting(tmpch);
	      }
      if((tmpch != ch) &&
	 (tmpch->delay.wait_value > 0) && (GET_WAIT_PRIORITY(tmpch) < 40)){
	break_spell(tmpch);
	}
      if(tmpch->specials.fighting)
	GET_ENERGY(tmpch) -= 400;
    }
  }
  send_to_char("You produce a bright flash of light.\n\r",ch);
  return;
}

/*----------------------------------------------------------------------------------------------------------*/

/*
 * Spell vitalize self
 * Gives the caster back a number of moves based
 * on the casters mage level
 */
ASPELL(spell_vitalize_self)
{
  if(GET_MOVE(ch) >= GET_MAX_MOVE(ch)) {
    send_to_char("You are already rested.\n\r",ch);
    return;
  }
  
  GET_MOVE(ch) = MIN(GET_MAX_MOVE(ch),GET_MOVE(ch) + 2 * level);
  send_to_char("You feel refreshed.\n\r",ch);
  return;
}

/*----------------------------------------------------------------------------------------------------------*/
/*
 * Summon spell
 * Transfers a character from where they are to 
 * the casters room.
 * We don't use this spell anymore should it be completely
 * removed?
 */

ASPELL(spell_summon)
{
  int ch_x, ch_y, v_x, v_y, dist;

/*  if(GET_LEVEL(ch) < LEVEL_GOD) {
    send_to_char("Summon no longer has power over creatures of Arda\n\r", ch);
    return;
  }*/
  
  if(!victim)
    return;
  if(ch->in_room == NOWHERE) 
    return;

  if(IS_NPC(victim)) {
    send_to_char("No such player in the world.\n\r", ch);
    return;
  }
  
  if(victim == ch) {
    send_to_char("You are already here.\n\r",ch);
    return;
  }
  
  if(victim->in_room == ch->in_room) {
    act("$E is already here.\n\r", FALSE, ch, 0, victim, TO_CHAR);
    return;
  }
  
  if((GET_POS(victim) == POSITION_FIGHTING) ||
     (IS_SET(world[ch->in_room].room_flags, NO_TELEPORT)) ||
     (PRF_FLAGGED(victim, PRF_SUMMONABLE)) ||
     (GET_LEVEL(victim) >= LEVEL_IMMORT)) {
    send_to_char("You failed.\n\r", ch);
    return;
  }
  
  ch_x = zone_table[world[ch->in_room].zone].x;
  ch_y = zone_table[world[ch->in_room].zone].y;
  v_x = zone_table[world[victim->in_room].zone].x;
  v_y = zone_table[world[victim->in_room].zone].y;
  dist = ((ch_x - v_x) ^ 2) + ((ch_y - v_y) ^ 2);

  /* Make high level mobs harder to summon */
  if((GET_LEVEL(victim) > 10) && IS_NPC(victim))
    level -= (GET_LEVEL(victim) - 10);
  
  if(!saves_spell(victim, level, (dist)) && !other_side(ch,victim)) {
    act("$N appears in the room.", TRUE, ch, 0, victim, TO_ROOM);
    act("$N appears in the room.", TRUE, ch, 0, victim, TO_CHAR);
    if (IS_RIDING(victim)) 
      stop_riding(victim);
    char_from_room(victim);
    char_to_room(victim, ch->in_room);
    act("$N summons you!", FALSE, victim, 0, ch, TO_CHAR);
    do_look(victim, "", 0, 0, 0);
  }
  else
    send_to_char("You failed.\n\r",ch);
}


/*----------------------------------------------------------------------------------------------------------*/

/*
 * Spell Identify
 * Allows mortals to use the vstat command!
 * Gives details on objects 
 */

ASPELL(spell_identify)
{
struct affected_type af;

  if (!obj) {
    send_to_char("You should cast this on objects only.\n\r", ch);
    return;
  }
  
  do_identify_object(ch, obj);
  if (!affected_by_spell(ch, SPELL_HAZE)) {
    af.type      = SPELL_HAZE;
    af.duration  = 1;	     
    af.modifier  = 1;  	     
    af.location  = APPLY_NONE;
    af.bitvector = AFF_HAZE;
    
    affect_to_char(ch, &af);  
  }

  GET_MOVE(ch) = GET_MOVE(ch) - level; 
  send_to_char ("You feel dizzy and tired from your mental exertion.\r\n", ch);

}

/*----------------------------------------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------*/

/* Teleportation Spells
 *Spells in this segement in order
 * - Blink (and blink related funcations)
 * - Relocate
 * - Beacon
 * On the note of Teleportation spells they are very
 * UnTolkienish, even moreso than our other spells
 * should we perhaps give thought to removing them from the
 * game completely, with the possible exception of blink
 * Khronos 27/03/05
 */

/*----------------------------------------------------------------------------------------------------------*/

int
random_exit(int room)
{
  // selects a random exit from the room (real number 'room')
  // and returns the real number of the new room.

  int tmp,num, res;
  sh_int romfl;
  int ex_rooms[NUM_OF_DIRS];
  
  if((room < 0) || (room > top_of_world)) 
    return NOWHERE;

  for(tmp = 0, num = 0; tmp < NUM_OF_DIRS; tmp++) 
    ex_rooms[tmp] = 0;

  for(tmp = 0, num = 0; tmp < NUM_OF_DIRS; tmp++)
    if(world[room].dir_option[tmp])
      if((world[room].dir_option[tmp]->to_room != NOWHERE) && 
	 ((!IS_SET(world[room].dir_option[tmp]->exit_info, EX_DOORISHEAVY)) ||
	  (!IS_SET(world[room].dir_option[tmp]->exit_info, EX_CLOSED))) &&
	 !IS_SET(world[room].dir_option[tmp]->exit_info, EX_NOBLINK)){
	
	romfl = world[world[room].dir_option[tmp]->to_room].room_flags;
	
	if(!IS_SET(romfl, DEATH) && !IS_SET(romfl, NO_MAGIC)&& 
	   !IS_SET(romfl, SECURITYROOM)){
	  ex_rooms[num] = world[room].dir_option[tmp]->to_room;
	  num++;        
	}
	
      }
  if(num == 0) 
    return room;
  
  res = number(0,num-1);

  return ex_rooms[res];
}
							      



ASPELL(spell_blink)
{
  int room, tmp, fail, dist;
  
  if(!ch) 
    return;

  if(!victim)
    victim = ch;

  room = victim->in_room;
  fail = 0;

  if(GET_SPEC(ch) == PLRSPEC_TELE)
    dist = 5;
  else
    dist = 3;

  for(tmp = 0; tmp < dist; tmp++) {
    if(room == NOWHERE) {
      fail = 1; 
      break;
    }
    room = random_exit(room);
  }
  if(room == NOWHERE) 
    fail = 1;
  
  if(IS_SET(world[room].room_flags, NO_TELEPORT))
  { 
    // Oops, this is a NO_TELEPORT room, we fail.
    fail = 1;
  }

  if(fail){
    send_to_char("The world spins around, but nothing happens.\n\r",victim);
  }
  else{
    stop_riding(victim);
    if(ch != victim) 
      send_to_char("You relocated your victim.\n\r",ch);
    send_to_char("The world spinned around you, and changed.\n\r",victim);
    act("$n disappeared in a flash of light.\n\r",TRUE, victim, 0, 0, TO_ROOM);
    char_from_room(victim);
    char_to_room(victim, room);
    do_look(victim, "", 0, 15, 0);
     act("$n appeared in a flash of light.\n\r",TRUE, victim, 0, 0, TO_ROOM);
  }    

}

/*----------------------------------------------------------------------------------------------------------*/
ASPELL(spell_relocate)
{
  int zon_start, zon_target, zon_try, dist;
  int x,y, del_x, del_y, tmp, tmp2, num, tmpnum;
  struct room_data * room;
  struct affected_type af;
  
  if(ch->in_room == NOWHERE) 
    return;
  
  zon_start = world[ch->in_room].zone;
  x = zone_table[zon_start].x;
  y = zone_table[zon_start].y;
  del_x = del_y = 0;
  
  /* The base range of relocate is 4 zones */
  dist = 4;
  
  /*
   * If the player has Anger, remove 2 zones from the range, 
   * otherwise if the player's specialization is teleportation,
   * add a zone.
   */
  if(affected_by_spell(ch, SPELL_ANGER))
    dist -= 2;
  else if(GET_SPEC(ch) == PLRSPEC_TELE)
    dist += 1;
  
  switch(digit) {
  case 0:
    del_y = -dist; 
    break;

  case 1: 
    del_x = dist;
    break;

  case 2: 
    del_y = dist;
    break;

  case 3: 
    if(GET_RACE(ch) == RACE_URUK && (x >= 8 && x <= 10)) {
      send_to_char("The light of Valar blinds your eyes, and "
		   "prevents you from going west.\n\r", ch);
      return;
    }
    del_x = -dist; 
    break;

  case 4:
    send_to_char("There is but the sky up there.\n\r",ch);
    return;

  case 5:
    send_to_char("There is but the ground down there.\n\r",ch);
    return;

  default:
    send_to_char("Unrecognized direction, please report.\n\r",ch);
    return;
  }

  zon_target = -1;
  zon_try = 0;
  num = 0;
  while(zon_target < 0) {
    while((zon_target < 0) && (del_x || del_y)) {
      for( ; zon_try <= top_of_zone_table; zon_try++)
	if((zone_table[zon_try].x == (x + del_x)) &&
	   (zone_table[zon_try].y == (y + del_y)))
	  break;
      
      if((zon_try > top_of_zone_table) &&(num == 0)) {
	del_x /= 2;
	del_y /= 2;     
	zon_try = 0;
      }
      else
	zon_target = zon_try;
    } 

    if((zon_try >= top_of_zone_table) || !(del_x || del_y)) 
      break;

    for(tmp = 0; tmp <= top_of_world; tmp++) {
      room = &world[tmp];
      if((room->zone == zon_target) && !room->people && 
	 !IS_SET(room->room_flags, DEATH) && 
	 !IS_SET(room->room_flags, SECURITYROOM) &&
         !IS_SET(room->room_flags, NO_TELEPORT)) {
	for(tmp2 = 0; tmp2 < NUM_OF_DIRS; tmp2++)
	  if(room->dir_option[tmp2])
	    break;
	if(tmp2 < NUM_OF_DIRS)
	  num++;
      }
    }
    if((num == 0) || !(del_x || del_y)) {
      zon_target = -1;
      del_x /= 2;
      del_y /= 2;
    }
  }
  if((zon_target < 0) || (num == 0)) {
    send_to_char("There is no suitable land in that direction.\n\r",ch);
    return;
  }
  num = number(1, num);
  
  zon_target = -1;
  zon_try = 0;
  tmpnum = 0;
  while(zon_target < 0) {
    while((zon_target < 0) && (del_x || del_y)) {
      for(; zon_try <= top_of_zone_table; zon_try++)
	if((zone_table[zon_try].x == (x + del_x)) &&
	   (zone_table[zon_try].y == (y + del_y)))
	  break;
      
      if(zon_try <= top_of_zone_table)
	zon_target = zon_try;
      else {
	send_to_char("Something went wrong in your spell.\n\r", ch);
	return;
      }
    } 
    for(tmp = 0; tmp <= top_of_world; tmp++) {
      room = &world[tmp];
      if((room->zone == zon_target) && !room->people && 
	 !IS_SET(room->room_flags,DEATH) && 
	 !IS_SET(room->room_flags,SECURITYROOM)) {
	for(tmp2 = 0; tmp2 < NUM_OF_DIRS; tmp2++)
	  if(room->dir_option[tmp2]) 
	    break;
	if(tmp2 < NUM_OF_DIRS) 
	  tmpnum++;
	if(tmpnum == num)
	  break;
      }
    }
    if(tmpnum == 0)
      zon_target = -1;
    if(tmpnum == num)
      break;
  }

  if(tmp <= top_of_world) {
    stop_riding(ch);
    act("$n screams a shrill wail of pain, and suddenly disappears.\n\r",
	FALSE, ch, 0, 0, TO_ROOM);
    char_from_room(ch);
    char_to_room(ch, tmp);
    act("$n appears in an explosion of soft white light.\n\r", 
	FALSE, ch, 0, 0, TO_ROOM);
    send_to_char("Pain fills your body, and your vision blurs. "
		 "You now stand elsewhere.\n\r", ch);
    
    /* Apply confuse and haze */
     if(!affected_by_spell(ch, SPELL_CONFUSE)) {
      af.type      = SPELL_CONFUSE;
      af.duration  = 40;	     /* level 30 confuse */
      af.modifier  = 1;  	     /* modifier doesn't matter */
      af.location  = APPLY_NONE;
      af.bitvector = AFF_CONFUSE;

      affect_to_char(ch, &af);
      act("Strange thoughts stream through your mind, making it "
	  "hard to concentrate.", TRUE, ch, 0, 0, TO_CHAR);
      act("$n appears to be confused!", FALSE, ch, 0, 0, TO_ROOM);
    }

    if(!affected_by_spell(ch, SPELL_HAZE)) {
      af.type      = SPELL_HAZE;
      af.duration  = 1;		  /* 1 tick */
      af.modifier  = 1;		  /* modifier doesn't matter */
      af.location  = APPLY_NONE;
      af.bitvector = AFF_HAZE;
      
      affect_to_char(ch, &af);
      act("You feel dizzy as your surroundings seem to blur and twist.\n\r", 
	  TRUE, ch, 0, 0, TO_CHAR);
      act("$n staggers, overcome by dizziness!",FALSE, ch, 0, 0, TO_ROOM);
    }
    do_look(ch,"", 0, 0, 0);
  }
}

/*----------------------------------------------------------------------------------------------------------*/

ASPELL(spell_beacon){

  affected_type newaf;
  int mode;
  waiting_type * wtl = &ch->delay;
  mode = 0;

  if(wtl->targ2.type == TARGET_TEXT){

    if(!str_cmp(wtl->targ2.ptr.text->text, "set"))
      mode = 1;

    else if(!str_cmp(wtl->targ2.ptr.text->text, "return"))
      mode = 2;

    else if(!str_cmp(wtl->targ2.ptr.text->text, "release"))
      mode = 3;

  }
  else if(wtl->targ2.type == TARGET_NONE){
    mode = 2;
  }

  if(!mode){
    send_to_char("You can only 'set' your beacon, 'return' to it or 'release' it.\n\r",ch);
    return;
  }

  if(mode == 1){ // Setting the beacone

    if(IS_SET(world[ch->in_room].room_flags, NO_TELEPORT))
    {
      send_to_char("You cannot seem to set your beacon here.\n\r", ch);
      return;
    }

    if(affected_by_spell(ch, SPELL_BEACON)){
      send_to_char("You reset your beacon here.\n\r",ch);
      affect_from_char(ch, SPELL_BEACON);
    }
    else
    {
      send_to_char("You set your beacon here.\n\r",ch);
    }
    
    newaf.type      = SPELL_BEACON;
    newaf.duration  = level;
    newaf.modifier  = ch->in_room;
    newaf.location  = 0;
    newaf.bitvector = 0;
    
    affect_to_char(ch, &newaf);

    return;
  }

  if(mode == 2){ // Returning..
    
    affected_type * oldaf;

    if(!(oldaf = affected_by_spell(ch, SPELL_BEACON))){
      
      send_to_char("You have no beacons set.\n\r",ch);
      return;
    }

    if(oldaf->modifier > top_of_world){
      
      send_to_char("Your beacon has been corruped, you cannot return to it.\n\r",ch);

      affect_from_char(ch, SPELL_BEACON);
      return;
    }
    else {
      room_data *to_room = &world[oldaf->modifier];
      zone_data *old_zone = &zone_table[world[ch->in_room].zone];
      zone_data *new_zone = &zone_table[to_room->zone];

      int distance =
	(old_zone->x - new_zone->x)*(old_zone->x - new_zone->x) +
	(old_zone->y - new_zone->y)*(old_zone->y - new_zone->y);
      
      if(distance > 25){
	
	send_to_char("Your beacon is too far to be of use.\n\r",ch);
	return; // not removing the beacon.
      }
      
      act("$n disappears in bright spectral halo.", FALSE, ch, 0, 0, TO_ROOM);
      char_from_room(ch);
      char_to_room(ch, oldaf->modifier);
      act("$n appears in bright spectral halo.",FALSE, ch, 0, 0, TO_ROOM);
      
      send_to_char("You return to your beacon!\n\r",ch);
      do_look(ch,"", 0,0,0);
      
      affect_from_char(ch, SPELL_BEACON);
      return;
    }
  }
  if(mode == 3){
    if(affected_by_spell(ch, SPELL_BEACON)){
      send_to_char("You release your beacon.\n\r",ch);
      affect_from_char(ch, SPELL_BEACON);
    }
    else{
      send_to_char("You have no beacons to release.\n\r",ch);
    }
  }
  
}
/*---------------------------------------------------------------------------------------------------------*/





/*----------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------*/
/* Offensive spells listed in order off
 * - Magic Missile
 * - Chill Ray
 * - Lightning Bolt
 * - Dark Bolt
 * - Fire Bolt
 * - Cone of Cold
 * - Earthquake
 * - Lightning Strike
 * - Fireball  
 */


/*----------------------------------------------------------------------------------------------------------*/
/*
 * Spell Magic Missile
 * Our most basic Offensive spell
 * Does small ammounts of phys damage 
 */

ASPELL(spell_magic_missile)
{
  int dam;
  
  dam = number(1, MAG_POWER(ch) / 6) + 12;
  
  if(saves_spell(victim, level, 0))
    dam >>= 1;
  damage(ch, victim, dam, SPELL_MAGIC_MISSILE,0);
}

/*----------------------------------------------------------------------------------------------------------*/
/*
 * Spell Chill Ray
 * This spell is supposed to do medium ammounts of damage
 * but is currently our MOST effective spells vs everything
 * its "freezeing" affect means people can spam chill ray smobs
 * and never get hit. Plus i've seen it do 48 damage a cast
 * This spell needs to be addresses ASAP AND CHANGED
 * Khronos 27/03/05
 */

ASPELL(spell_chill_ray)
{
  int dam;

  dam = number(1, MAG_POWER(ch)) / 2 + 20;
  
  if(!saves_spell(victim, level, 0))    
    GET_ENERGY(victim) -= GET_ENERGY(victim) / 2 + 
      2 * GET_ENE_REGEN(victim) * 2;
  else
    dam >>= 1;
  damage(ch, victim, dam, SPELL_CHILL_RAY,0);
}


/*----------------------------------------------------------------------------------------------------------*/
/*
 * Spell Lightning bolt
 * Very effective spell if a little under used
 * Its fast and does reasonable damage, if we tweak chill ray
 * i can see this spell becomming more popular, also if lightning
 * spec was introduced this could become a prefered spell
 * On a side note if we do introduce spec lightning perhaps if a
 * player was speced in lightning there would be no lose of spell 
 * affectiveness if they were indoors
 */

ASPELL(spell_lightning_bolt)
{
  int dam;
  
  dam = number(0, MAG_POWER(ch)) / 2 + 25;

  if(OUTSIDE(ch))
    dam += number(0, MAG_POWER(ch) / 4) + 2;
  else  
    send_to_char("Your lightning is weaker inside, as you can not "
		 "call on nature's full force here.\n\r", ch);  
  if(saves_spell(victim, level, 0))
    dam >>= 1;
  damage(ch, victim, dam, SPELL_LIGHTNING_BOLT,0);
}

/*----------------------------------------------------------------------------------------------------------*/
/*
 * Spell Dark bolt
 * Our Darkie races spell of choice
 * A good spell well fast to cast and relatively low on mana
 * It affected by sunlight which weakens its damage
 */

ASPELL(spell_dark_bolt)
{
  int dam;
  
  dam = number(0, MAG_POWER(ch)) / 2 + 25;
  
  
  if (!SUN_PENALTY(ch)) 
    dam += number(0, MAG_POWER(ch) / 4) + 4;
    else 
      send_to_char("Your spell is weakened by the intensity of light.\n\r", ch);
  
    if(saves_spell(victim, level, 0))
    dam >>= 1;
  damage(ch, victim, dam, SPELL_DARK_BOLT, 0);
}

/*----------------------------------------------------------------------------------------------------------*/
/*
 * spell Firebolt
 * powerful mid level spell for whities only
 * very random spell damage
 */

ASPELL(spell_firebolt)
{
  int dam;
  
  dam = number(1, MAG_POWER(ch)) / 4 + number(1, MAG_POWER(ch)) / 4 +
    number(1, MAG_POWER(ch)) / 8 + number(1, MAG_POWER(ch)) / 8 +
    number(1, MAG_POWER(ch)) / 16 + number(1, MAG_POWER(ch)) / 16 +
    number(1, 65);
  
/*  if(RACE_SOME_ORC(ch) && !number(0, 9)) {
    send_to_char("You fail to control the fire you invoke!\n\r", ch);
    victim = ch;
    dam = dam / 3;
  }
*/

  if(saves_spell(victim, level, 0))
    dam >>= 1;
  damage(ch, victim, dam, SPELL_FIREBOLT, 0);
}

/*----------------------------------------------------------------------------------------------------------*/
/* 
 * Spell cone of cold
 * Powerful upper end cold spell
 * not as popular as chill ray as it can't perma bash chars
 * Should we prehaps remove the directional stuff on this spell
 * Will we ever use it??
 * Khronos 27/03/05
 */

ASPELL(spell_cone_of_cold)
{
  int dam, tmp;
  char buf1[255], buf2[255];
  struct char_data *tmpch;
  
  dam = number(1, MAG_POWER(ch)) / 2 + MAG_POWER(ch) / 4 + 25;
  
  if(victim) {
    if(saves_spell(victim, level, 0))
      dam = dam * 2 / 3 ;    
    damage(ch, victim, dam, SPELL_CONE_OF_COLD,0);
    return;
  }
  
  /* Directional part */
  if((digit < 0) || (digit >= NUM_OF_DIRS))
    return;
  
  if(!EXIT(ch, digit) || (EXIT(ch, digit)->to_room == NOWHERE)) {
    send_to_char("There is nothing in that direction.\n\r", ch);
    return;
  }
  if(IS_SET(EXIT(ch, digit)->exit_info, EX_CLOSED)) {
    if(IS_SET(EXIT(ch, digit)->exit_info, EX_ISHIDDEN)) {
      send_to_char("There is nothing in that direction.\n\r", ch);
      return;
    }
    else {
      sprintf(buf1,"Your cone of cold hit %s, to no real effect.\n\r",
	      EXIT(ch, digit)->keyword);
      send_to_char(buf1,ch);
      return;
    }
  }
  if(IS_SET(EXIT(ch, digit)->exit_info, EX_NO_LOOK)) {
    send_to_char("Your cone of cold faded before reaching its' target.\n\r",
		 ch);
    return;
  }
  
  sprintf(buf1,"You send a cone of cold to %s.\n\r", refer_dirs[digit]);
  send_to_char(buf1,ch);

  sprintf(buf1,"A sudden wave of cold strikes you, coming from %s.\n\r",
	  refer_dirs[rev_dir[digit]]);
  sprintf(buf2,"You feel a sudden wave of cold coming from %s.\n\r",
	  refer_dirs[rev_dir[digit]]);

  for(tmpch = world[EXIT(ch, digit)->to_room].people;
      tmpch;
      tmpch = tmpch->next_in_room) {
    if(saves_spell(tmpch,level,0)) {
      tmp = GET_HIT(tmpch);
      if(tmp < dam / 2)
	tmp = 1;
      else 
	tmp = tmp - dam / 2;

      GET_HIT(tmpch) = tmp;
      send_to_char(buf1, tmpch);
    }
    else
      send_to_char(buf2, tmpch);
  }
}

/*----------------------------------------------------------------------------------------------------------*/
/*
 * Spell earthquake
 * Powerful spell that deals damage to every character in the room
 * has the ability to demolish Orc's quite quickly, perhaps orcs should
 * have a specific save vs the spell
 * Khronos 27/03/05
 */

ASPELL(spell_earthquake){
  int dam_value, crack_chance, tmp;
  struct char_data * tmpch, * tmpch_next;
  room_data * cur_room;
  int crack;

  if(!ch) return;
  if(ch->in_room == NOWHERE) return;
  cur_room = &world[ch->in_room];

  crack_chance = 1;

  if((cur_room->sector_type == SECT_CITY) || 
     (cur_room->sector_type == SECT_CRACK) ||
     (cur_room->sector_type == SECT_WATER_SWIM) ||
     (cur_room->sector_type == SECT_WATER_NOSWIM) ||
     (cur_room->sector_type == SECT_UNDERWATER))
    crack_chance = 0;

  if(IS_SET(cur_room->room_flags, INDOORS)) crack_chance = 0;
  if(cur_room->dir_option[DOWN] && IS_SET(cur_room->dir_option[DOWN]->exit_info, EX_CLOSED|EX_DOORISHEAVY)) crack_chance = 0; 

  if(cur_room->dir_option[DOWN] && !cur_room->dir_option[DOWN]->exit_info) 
    crack_chance = 1;  
  else  
    if(number(0,100) > level) crack_chance = 0;

  /* Here goes normal earthquake */

  dam_value = number(1,30) + level;
  
  if(crack_chance) dam_value /= 2;

  for(tmpch = world[ch->in_room].people; tmpch; tmpch = tmpch_next){
    tmpch_next = tmpch->next_in_room;
    if(tmpch != ch){
      if ( saves_spell(tmpch,level,0) )
	damage(ch, tmpch, dam_value / 2, SPELL_EARTHQUAKE, 0);
      else
	damage(ch, tmpch, dam_value, SPELL_EARTHQUAKE, 0);
    }
  //  return;
  }

  if(crack_chance) {
    if(cur_room->dir_option[DOWN] && 
       (cur_room->dir_option[DOWN]->to_room != NOWHERE)) {
      crack = cur_room->dir_option[DOWN]->to_room;
      if(crack != NOWHERE) {
	if(cur_room->dir_option[DOWN]->exit_info) {
	  act("The way down crashes open!", FALSE, ch, 0, 0, TO_ROOM);
	  send_to_char("The way down crashes open!\n\r",ch);
	  cur_room->dir_option[DOWN]->exit_info = 0;
	  if(world[crack].dir_option[UP] && 
	     (world[crack].dir_option[UP]->to_room == ch->in_room) &&
	     world[crack].dir_option[UP]->exit_info){
	    tmp = ch->in_room;
	    ch->in_room = crack;
	    act("The way up crashes open!", FALSE, ch, 0, 0, TO_ROOM);
	    world[crack].dir_option[UP]->exit_info = 0;
	    ch->in_room = tmp;
	  }
	}
      }
    }
    /* no room there, so create one */
    else {
      crack = world.create_room(world[ch->in_room].zone);
      world[ch->in_room].create_exit(DOWN, crack);
      
      RELEASE(world[crack].name);
      world[crack].name = str_dup("Deep Crevice");
      RELEASE(world[crack].description);
      world[crack].description = str_dup(
 "   The crevice is deep, dark and looks unsafe. The walls of fresh broken\n\r"
 "rock are uneven and still crumbling. Some powerful disaster must have \n\r"
 "torn the ground here recently.\n\r"); 
      world[crack].sector_type = SECT_CRACK;
      world[crack].room_flags = cur_room->room_flags;
      act("The ground is cracked under your feet!", FALSE, ch, 0, 0, TO_ROOM);
      send_to_char("Your spell cracks the ground open!\n\r",ch);
    }
    
    /* deal out the damage */
    for(tmpch = cur_room->people; tmpch; tmpch = tmpch_next) {
      tmpch_next = tmpch->next_in_room;
      if(!saves_spell(tmpch,level + 5 - GET_DEX(tmpch)/3,0) &&
	 ((tmpch != ch) || (!number(0,1)))){

	act("$n loses balance and falls down!", TRUE, tmpch, 0, 0, TO_ROOM);
	send_to_char("The earthquake throws you down!\n\r",tmpch);
	stop_riding(tmpch);
	char_from_room(tmpch);
	char_to_room(tmpch, crack);
	act("$n falls in.", TRUE, tmpch, 0, 0, TO_ROOM);      
	tmpch->specials.position = POSITION_SITTING;

	if ( saves_spell(tmpch,level,0) )
	  damage(ch, tmpch, dam_value, SPELL_EARTHQUAKE, 0);
	else
	  damage(ch, tmpch, dam_value * 2, SPELL_EARTHQUAKE, 0);
      }
    }
  }
}

/*----------------------------------------------------------------------------------------------------------*/
/* 
 * Spell lightning strike
 * Very powerful spell capable of doing huge damage
 * Only works when the weather is stormy
 * Perhaps in future if we imp a lighhning spec a character
 * speced in lightning won't have to wait on weather to cast
 * this spell (but have the damage reduced slightly)
 * Khronos 27/03/05
 */

ASPELL(spell_lightning_strike)
{
  int dam;

  dam = number(0, MAG_POWER(ch)) / 2 * 2 + number(0, MAG_POWER(ch)) / 2 + 40;
  
  if(!OUTSIDE(ch)) {
    send_to_char("You can not call lightning inside!\n\r", ch);
    return;
  }
  if(weather_info.sky[world[ch->in_room].sector_type] != SKY_LIGHTNING) {
    send_to_char("The weather is not appropriate for this spell.\n\r", ch);
    return;
  }
  
  if(saves_spell(victim, level, 0))
    dam = dam * 2 / 3;  
  damage(ch, victim, dam, SPELL_LIGHTNING_STRIKE, 0);
}


/*----------------------------------------------------------------------------------------------------------*/
/*
 * Spell searing darkness
 * Very powerful spell capable of doing huge damage
 * Uruk-Hai only spell that does fire and darkness as damage.
 * Maga 11/21/2016 
 */

ASPELL(spell_searing_darkness)
{
  int dam, damFIRE, damDARK;

  damFIRE = number(0, MAG_POWER(ch)) / 2 + 15;

  if(saves_spell(victim, level, 0))
    damFIRE = damFIRE * 1/3;

  damDARK = number(0, MAG_POWER(ch)) / 2 + 15;
  
  if(!SUN_PENALTY(ch))
    damDARK += number(0, MAG_POWER(ch) / 4) + 5;
  else
    send_to_char("Your spell is weakened by the intensity of light.\n\r", ch);

  dam = damFIRE + damDARK;
  
  damage(ch, victim, dam, SPELL_SEARING_DARKNESS, 0);
}



/*----------------------------------------------------------------------------------------------------------*/
/*
 * Spell Fireball
 * OUr high end damage spell
 * does big damage at the risk of hitting others in the room
 */

ASPELL(spell_fireball)
{
  int dam, tmp;
  struct char_data *tmpch;
  struct char_data *tmpch_next;
  
  if(ch->in_room == NOWHERE) 
    return;
  
  dam = number(1, MAG_POWER(ch)) / 2 + number(1, MAG_POWER(ch)) / 2 + 
    number(1,MAG_POWER(ch)) / 2 + 30;

  if(RACE_SOME_ORC(ch))
    dam -= 5;
  if(RACE_SOME_ORC(ch) && !number(0, 9)) {
    send_to_char("You fail to control the fire you invoke!\n\r", ch);
    victim = ch;
    dam = dam/3;
  }
  
  if(saves_spell(victim, level, 0))
    damage(ch, victim, dam * 2 / 3, SPELL_FIREBALL, 0);
  else
    damage(ch, victim, dam, SPELL_FIREBALL, 0);
  
  for(tmpch = world[ch->in_room].people; tmpch; tmpch = tmpch_next) {
    tmpch_next = tmpch->next_in_room;
    
    tmp = number(0, 99);
    if((tmpch != ch) && (tmpch != victim)) {
      if((tmpch->specials.fighting == ch) && (tmp < 80)) {
	if(saves_spell(victim, level, 0))
	  damage(ch, tmpch, dam / 6, SPELL_FIREBALL2, 0);
	else
	  damage(ch, tmpch, dam / 3, SPELL_FIREBALL2, 0);
      }
      if((tmpch->specials.fighting != ch) && (tmp < 20)) {
	if(saves_spell(victim, level, 0))
	  damage(ch, tmpch, dam / 10, SPELL_FIREBALL2, 0);
	else
	  damage(ch, tmpch, dam / 5, SPELL_FIREBALL2, 0);
      }
    }
  }
}

/*----------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------*/
/* Uruk Lhuth spells in order of
 * - Word of Pain
 * - Leach
 * - Word of Sight
 * - Word of Shock
 * - Black Arrow
 * - Word of Agony
 * - Shout of Pain
 * - Spear of Darkness
 */

/*----------------------------------------------------------------------------------------------------------*/

/*
 * `word of pain' is analogous to magic missile
 */
ASPELL(spell_word_of_pain)
{
  int dam;
  
  dam = number(1, MAG_POWER(ch) / 6) + 15;
  
  if(saves_spell(victim, level, 0))
    dam >>= 1;
  
  damage(ch, victim, dam, SPELL_WORD_OF_PAIN, 0);
}


/*----------------------------------------------------------------------------------------------------------*/

/*
 * leach: replaces chill ray; a bit less damage, but half of
 * the damage done goes to the caster.  There is also a small
 * drain of random (albeit small) number of moves.
 */
ASPELL(spell_leach)
{
  int moves, dam;
  
  dam = number(1, MAG_POWER(ch) / 4) + 18;
  
  if(saves_spell(victim, level, 0))
    dam >>= 1;
  else {
    moves = MIN(GET_MOVE(victim), number(0, 5));
    GET_MOVE(victim) += -moves;
    GET_MOVE(ch) = MIN(GET_MAX_MOVE(ch), GET_MOVE(ch) + moves);
    GET_HIT(ch) = MIN(GET_MAX_HIT(ch), GET_HIT(ch) + dam / 2);
    send_to_char("Your life's ichor is drained!\n\r", victim);
  }
  
  damage(ch, victim, dam, SPELL_LEACH, 0);
}

/*----------------------------------------------------------------------------------------------------------*/



/*
 * For a detailed discussion on the workings of reveal-
 * like spells, see the comment heading the reveal_life
 * spell.  For brevity, we leave all other reveal spells
 * completely undiscussed within their respective
 * function definitions.
 */
ASPELL(spell_word_of_sight)
{
  struct char_data *hider;
  int found, hider_bonus;
  
  if(ch->in_room == NOWHERE)
    return;
  
  act("A presence seeks the area, searching for souls."
      ,TRUE, ch ,0, 0, TO_ROOM);
  send_to_char("Your mind probes the area seeking other souls.\n\r",
	       ch);
  
  for(hider = world[ch->in_room].people, found = 0; hider; 
      hider = hider->next_in_room) {
    if(hider != ch) {
      if(GET_HIDING(hider) > 0) {
	hider_bonus = GET_HIDING(hider) / 30;
	hider_bonus += number(0, GET_HIDING(hider) % 30) ? 1 : 0;
	hider_bonus -= (30 - level) / 3;
	if(!saves_spell(hider, level, hider_bonus)) {
	  send_to_char("You've been discovered!\r\n", hider);
	  stop_hiding(hider, FALSE);
	  found = 1;
	}
	else if(!number(0, GET_HIDING(hider) / 40)) {
	  send_to_char("The presence of the soul grows intense, and you "
		       "feel less confident of your cover.\r\n", hider);
	  GET_HIDING(hider) -= GET_PROF_LEVEL(PROF_MAGE, ch) / 3;
	}
	else
	  send_to_char("The power of the soul recesses, and you feel "
		       "your cover is uncompromised.\r\n", hider);
      }
      else 
	found = 1;
    }
  }

  if(!found) 
    send_to_char("The place seems empty.\n\r", ch);
  else 
    list_char_to_char(world[ch->in_room].people, ch, 0);

  return;
}

/*----------------------------------------------------------------------------------------------------------*/
/*
 * word of shock is analogous to flash
 */
ASPELL(spell_word_of_shock)
{
  struct char_data *tmpch;
  
  if(ch->in_room < 0)
    return;
  
  for(tmpch = world[ch->in_room].people; tmpch; tmpch = tmpch->next_in_room) {
    if(tmpch != ch) 
      send_to_char("An assault on your mind leaves you reeling.\n\r", tmpch);
    if(tmpch->specials.fighting)
      GET_ENERGY(tmpch) -= 400;
    if(tmpch->specials.fighting == ch)
      GET_ENERGY(tmpch) -= 400;
    if(!saves_spell(tmpch, level, 0)) {
      if(tmpch->specials.fighting)
	stop_fighting(tmpch);
      if((tmpch != ch) &&
	 (tmpch->delay.wait_value > 0) &&
	 (GET_WAIT_PRIORITY(tmpch) < 40))
	break_spell(tmpch);
      if(tmpch->specials.fighting)
	GET_ENERGY(tmpch) -= 400;
    }
  }
  send_to_char("You utter a word of power.\n\r",ch);

  return;
}


/*----------------------------------------------------------------------------------------------------------*/

/*
 * black arrow replaces firebolt; though it deals a smaller
 * amount of damage, it has a chance to poison its victim.
 */
ASPELL(spell_black_arrow)
{
  int dam, min_poison_dam;
  struct affected_type af;
  
  min_poison_dam = 5;
  dam = number(1, MAG_POWER(ch)) / 2 + number(1, MAG_POWER(ch)) / 2 + 13;

  if(!SUN_PENALTY(ch))
    dam += number(0, MAG_POWER(ch) / 6) + 2;
  else  
    send_to_char("Your spell is weakened by the intensity of light.\n\r", ch);
  
  if(saves_spell(victim, level, 0))
    dam >>= 1;
  else if(number(1, 50) < level && GET_HIT(victim) > min_poison_dam) {
    af.type = SPELL_POISON;
    af.duration = level + 1;
    af.modifier = -2;
    af.location = APPLY_STR;
    af.bitvector = AFF_POISON;
    affect_join(victim, &af, FALSE, FALSE);
    
    send_to_char("The vile magic poisons you!\n\r", victim);
    damage((ch) ? ch : victim, victim, 5, SPELL_POISON, 0);
  }
  
  damage(ch, victim, dam, SPELL_BLACK_ARROW,0);
}

/*----------------------------------------------------------------------------------------------------------*/
/*
 * word_of_agony is analogous to cone, and has a chill
 * ray type effect on energy
 */
ASPELL(spell_word_of_agony)
{
   int  dam;

   dam = number(1, MAG_POWER(ch)) / 2 + number(1, MAG_POWER(ch)) / 2 + 20;

   if(saves_spell(victim, level, 0))
     damage(ch, victim, dam * 2 / 3, SPELL_WORD_OF_AGONY, 0);
   else{
     GET_ENERGY(victim) -= GET_ENERGY(victim) / 2 + 4 * GET_ENE_REGEN(victim);
     damage(ch, victim, dam, SPELL_WORD_OF_AGONY, 0);
   }  
}


/*----------------------------------------------------------------------------------------------------------*/
/*
 * shout of pain is analogous to earthquke, obviously
 * leaving no crack
 */
ASPELL(spell_shout_of_pain)
{
  int dam_value;
  struct char_data *tmpch, *tmpch_next;
  struct room_data *cur_room;
  
  if(!ch)
    return;
  if(ch->in_room == NOWHERE)
    return;
  cur_room = &world[ch->in_room];

  dam_value = number(1, 50) + MAG_POWER(ch) / 2;

  for(tmpch = world[ch->in_room].people; tmpch; tmpch = tmpch_next) {
    tmpch_next = tmpch->next_in_room;
    if(tmpch != ch) {
      if(saves_spell(tmpch, level, 0))
	damage(ch, tmpch, dam_value / 2, SPELL_SHOUT_OF_PAIN, 0);
      else
	damage(ch, tmpch, dam_value, SPELL_SHOUT_OF_PAIN, 0);
    }
  }
}


/*----------------------------------------------------------------------------------------------------------*/

/*
 * spear of darkness; damage like fireball, damage malused
 * in sun in the style of dark bolt.  Additionally, spear
 * is not saveable.
 */
ASPELL(spell_spear_of_darkness)
{
  int dam;

  dam = number(0, MAG_POWER(ch)) / 5 + number(8, MAG_POWER(ch)) / 2
    + number(8, MAG_POWER(ch)) / 2;

  if(!SUN_PENALTY(ch))
    dam += number(8, MAG_POWER(ch))/2 + 20;
  else  
    send_to_char("Your spell is weakened by the intensity of light.\n\r", ch);
   
  damage(ch, victim, dam, SPELL_SPEAR_OF_DARKNESS, 0);
}


/*----------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------*/
/* Spec Specific spells list below in order of 
 * - Blaze
 * - Freeze
 * - Mist of bazunga
 */
/*----------------------------------------------------------------------------------------------------------*/

/*
 * Spell blaze
 * powerful spell that sets the room the spell is casted in
 * on fire, literally.
 * Are we going to imp this every?
 * Khronos 27/03/05
 */

ASPELL(spell_blaze)
{
  struct affected_type af;
  struct affected_type *oldaf;
  struct char_data *tmpch;
  struct char_data *tmpch_next;
  int dam;

  if(!victim && !obj) { /* there was no target, hit the room */
    if(!ch) 
      return;
    
    act("$n breathes out a cloud of fire!", TRUE, ch, 0, 0, TO_ROOM);
    send_to_char("You breathe out fire.\n\r", ch);
    
    /* Damage everyone in the room */
    for(tmpch = world[ch->in_room].people; tmpch; tmpch = tmpch_next) {
      tmpch_next = tmpch->next_in_room;
      dam = number(1, 30) + MAG_POWER(ch) / 2; /* same as earthquake */
      if(saves_spell(tmpch, level, tmpch == ch ? 3 : 0))
	dam = MAG_POWER(ch) / 6;
      damage(ch ? ch : tmpch, tmpch, dam, SPELL_BLAZE, 0);
    }
    
    /*
     * Add the new affection.  Keep in mind that the af.modifier is the
     * mage level that will be used to calculate damage in subsequent
     * calls.
     */
    af.type = ROOMAFF_SPELL;
    af.duration = level;
    af.modifier = level;
    af.location = SPELL_BLAZE;
    af.bitvector = 0;
    if((oldaf = room_affected_by_spell(&world[ch->in_room], SPELL_BLAZE))) {
      if(oldaf->duration < af.duration)
	oldaf->duration = af.duration;
      if(oldaf->modifier < af.modifier)
	oldaf->modifier = af.modifier;
    }
    else
      affect_to_room(&world[ch->in_room], &af);

    act("The area suddenly bursts into a roaring firestorm!",
	FALSE, ch, 0, 0, TO_ROOM);
    send_to_char("The area suddenly bursts into a roaring firestorm!\n\r", ch);
  }
  /*
   * We were called by room_affect_update or there was an actual
   * victim specified 
   */
  else if(victim) {
    dam = number(8, level) + 10;
    if(saves_spell(victim, level, 0))
      dam >>= 1;
    
    if(ch != victim)
      act("$n breathes fire on you!", TRUE, ch, 0, victim, TO_VICT);
    damage(ch ? ch : victim, victim, dam, SPELL_BLAZE, 0); 
  }
}



/*----------------------------------------------------------------------------------------------------------*/

/*
 * Bear in mind that `targ2' is the target_info structure that
 * contains our door.  Also realize that we don't really do any
 * exit validation at this point in the game; that's all been
 * done in (first) interpre.cc when the command was originally
 * entered, and then done again in spell_pa.cc's do_cast.
 *
 * TODO: now we just need to make a new 'is frozen' affection
 * for exits, add behavior all over the code for that flag 
 * (mainly in stuff like do_open, do_close, maybe pick, and, of
 * course, the shaping interface [including manuals]).  We also 
 * need a flag like NO_FREEZE for exits so that builders can 
 * designate certain doors as unfreezable.
 */

/* 
 *I think personally this spell is a bad idea,
 * i envisage alot of trap kills from it
 * Khronos 27/03/05
 */
 
ASPELL(spell_freeze)

{
  if(IS_SET(EXIT(ch, digit)->exit_info, EX_CLOSED)) {
    /* Did they use "east", "west", etc. to target this direction? */
    if(ch->delay.targ2.choice == TAR_DIR_WAY)
      sprintf(buf, "A thick sheet of hardened frost forms %s%s%s.\r\n",
	      digit != UP && digit != DOWN ? "to " : "", refer_dirs[digit],
	      digit != UP && digit != DOWN ? "" : " you");
    /* Or did they actually know the name of the door? */
    else if(ch->delay.targ2.choice == TAR_DIR_NAME)
      sprintf(buf, "The %s is frozen shut.\r\n", EXIT(ch, digit)->keyword);
    send_to_room(buf, ch->in_room);
    return;
  }
  else
    send_to_char("You must first have a closed exit to cast upon.\r\n", ch);
}


/*----------------------------------------------------------------------------------------------------------*/

/*
 * uruk mage spell: mist of baazunga
 * The mist of baazunga causes the room it is casted in to
 * be affected become dark for a short period of time.
 * Additionally, every room directly connected to the misted
 * room is affected.  The only other place where the mists
 * are dealt with is in affect_update_room in limits.cc:
 * that code maintains the mist, and causes it to float
 * about from room to room.
 *
 * TODO: mist should actually cause rooms to be dark.  We
 * should also generalize the affection "branching" code so
 * that we can also have things like blaze branch from room
 * to room.
 */
ASPELL(spell_mist_of_baazunga)
{
  struct affected_type af, af2;
  struct affected_type *oldaf;
  struct room_data *room;
  int modifier, direction, mod, roomnum;

  if(!ch)
    return;

  room = &world[ch->in_room];
  if((oldaf = room_affected_by_spell(room, SPELL_MIST_OF_BAAZUNGA)))
    modifier = oldaf->modifier;
  else { 
    if(IS_SET(room->room_flags, SHADOWY))
      modifier = 1;
    else
      modifier = 0;
  }

  af.type = ROOMAFF_SPELL;
  af.duration = level / 5;
  af.modifier = modifier;
  af.location = SPELL_MIST_OF_BAAZUNGA;
  af.bitvector = 0;
  
  /* Apply the full spell to main room */
  if((oldaf = room_affected_by_spell(&world[ch->in_room],
				     SPELL_MIST_OF_BAAZUNGA))) {
    if(oldaf->duration < af.duration)
      oldaf->duration = af.duration;
    /*
     * This has been commented out for a pretty long time;
     * why exactly don't we want to output a message if the
     * caster is renewing the mists?
     *
     * act("$n breathes out dark mists.", TRUE, ch, 0, 0, TO_ROOM);
     * send_to_char("You breathe out dark mists.\n\r", ch);
     */
  }
  else {
    affect_to_room(&world[ch->in_room], &af);
    act("$n breathes out dark mists.", TRUE, ch, 0, 0, TO_ROOM);
    send_to_char("You breathe out dark mists.\n\r", ch);
  }
  
  /* Apply a smaller spell to the joined rooms */
  for(direction = 0; direction < NUM_OF_DIRS; direction++) {
    if(room->dir_option[direction]) {
      if(room->dir_option[direction]->to_room != NOWHERE) {
	roomnum = room->dir_option[direction]->to_room;

	if((oldaf = room_affected_by_spell(&world[roomnum],
					   SPELL_MIST_OF_BAAZUNGA)))
	  mod = oldaf->modifier;
	else if(IS_SET(world[roomnum].room_flags, SHADOWY))
	  mod = 1;
	else
	  mod = 0;
	
	af2.type = ROOMAFF_SPELL;
	af2.duration = level / 6;
	af2.modifier = mod;
	af2.location = SPELL_MIST_OF_BAAZUNGA;
	af2.bitvector = 0;
	
	if((oldaf = room_affected_by_spell(&world[roomnum],
					   SPELL_MIST_OF_BAAZUNGA))) {
	  if(oldaf->duration < af.duration)
	    oldaf->duration = af.duration;
	}
	else
	  affect_to_room(&world[roomnum], &af2);
      }
    }
  } 
  
  return;
}

