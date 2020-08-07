# Special Mobs in Arda

Here is where all the documentation for special mobs are located, such as dragons, vampire killers, and guildmasters.

### Table of Content
* [Dragons](#dragons)
* [Swarms](#swarms)

## Dragons
Dragons use the special program #30 which will cast dragon's breath on the players in the room. This is declared in the spec_pro.cc file under ```SPECIAL(dragon)```. Listed below are the plans for this program:
1. If the dragon is not in combat we do nothing
1. The chance of the dragon using it's breath weapon is ```(mob_level * 1.25) / 4```
1. If the room is already affected by ```SPELL_DRAGONSBREATH``` the chance for recasting is reduced by 5%
1. Damage is ```dice((mob_level / 2), 6)```
1. Next we loop through all creatures in the room and apply the damage, we ignore the dragon in the room because they aren't affect by their own breath weapon.
1. Final step is to apply the room afflection ```SPELL_DRAGONSBREATH```
    * If the room isn't affected by dragon's breath then we'll need to set it, but if it's currently active we'll refresh the duration on the room.
    
Below is the current code in spec_pro.cc
```c++
SPECIAL(dragon) {
	struct char_data *tmpch, *tmpch_next;
	int dam_total, mob_level, chance_roll, dam_roll;
	room_data *room;
	// If the dragon isn't fighting do nothing
	if (GET_POS(host) != POSITION_FIGHTING) {
		return 0;
	}
	
	mob_level = GET_LEVEL(host);
	chance_roll = (mob_level * 1.25) / 4;
	
	if(room->affected)
	// Checks for random roll on doing damage
	if (number(0, chance_roll))
		return 0;

	// Calculate the damage based on mob level / 2
	// For example: level 50 dragon will do 25d6 worth of damage which comes out to min: 25 and max: 150
	dam_total = dice((mob_level / 2), 6);

	// Here is where we damage all the players/mobs in the room,
	// but we exclude the dragon itself.
	for (tmpch = world[host->in_room].people; tmpch; tmpch = tmpch_next)
	{
		tmpch_next = tmpch->next_in_room;
		if (tmpch != host)
		{
			damage(host, tmpch, total, SPELL_DRAGONSBREATH, 0);
		}
	}
	act("$N snorts and a gout of fire shoots out of its nostrils at YOU!", FALSE, host, 0, host, TO_ROOM);

	return 0;
}
```
## Swarms
Swarms are a special mob that can be summoned by a Haradrim player. It will follow the player and assist them if they attack something. If the victim flees the swarm will start hunting them and move away from the master. After being summoned they have a timer till they disappear, but if they separate from their master the timer is increased. Listed below are the plans for this program:
1. If the swarm doesn't have a target and isn't in combat do nothing
1. If the master is in combat assist them
1. If we have a victim and not in combat