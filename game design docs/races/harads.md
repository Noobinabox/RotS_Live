# Haradrim Race Configration

Listed below is all the racial configuration and design for Haradrims.
* Bleed attack

Ideas for race:
* Lay traps in rooms
  * Have it act like followers/recruits and only so many can be active at a time.
* Spear toss with disposable items, such as crafting or gather.
* Hamstring/Maiming shot
* Bonus to spears and bows
* Mark
* Summon Swarm
* Sun
* Spears
* Archery
* Blinding Shot


### Bleed Attack
In structs.h a new struct needs to be added to track bleed in rooms.<br />
```c++
struct room_bleed_data {
	sh_int char_number;
	byte data;
	sh_int condition;
	
	room_bleed_data() { char_number = 0; data = 0; condition = 0;}
}
```

Then you'll need to add the following to ```struct room_data```:
```c++
struct room_data {
	...
	struct room_bleed_data bleed_track[NUM_OF_TRACKS];
}
```

In **act_mov.cc** we'll need to add bleed data for the following functions:
```c++
perform_move_mount(struct char_data *ch, int dir)
```

We need to add a check to see if the riders of the mounts have a bleed affect and mark the room with blood.
```c++
ACMD(do_move)
```
Here is our basic move function and we should track the blood trail.

In **graph.cc** we'll need to modify/add the following:

In **limits.cc** we'll need to modify the following:
```c++
void update_room_tracks()
```

In **weather.cc** we'll need to modify the following:
```c++
void age_room_tracks()
```
### Mark
This will be a bow skill that will apply an affected called bleed. As the player travels around the world they will leave a blood trail in the room. They will also receive a move penalty till the affects wears off. With every heal that the player does it will reduce the timer on the bleed affect. Regen will remove a small tick if the player has any damage, but if they are at full health it will remove the ticks faster. Cure self will remove a static percent on the duration.<br />
#### Adding the skill
In **spells.h** add the following:
```c++
/* Haradrim skills listed below */
const int SKILL_MARK = 152;

/* End Haradrim skills */
```
Next we'll need to register the CMD in **interpre.h**:
```c++
/* Haradrim skills listed below */
const int CMD_MARK = 230;

/* End Haradim skills */
```

In **interpre.cc** I added all of the following:
```c++
ACMD(do_mark);
```
Under const char *command[] add the following:
```c++
	"mark",
```
>This will register mark as a comamnd in the game, make note of the number because you'll need it later.

Under assign_command_pointer(void) add the following:
```c++
COMMANDO(236, POSITION_FIGHTNING, do_mark, 0, TRUE, 0,
	TAR_CHAR_ROOM | TAR_FIGHT_VICT, TAR_IGNORE, CMD_MASK_NO_UNHIDE);
```

In **ranger.cc** add the following functions:


In **struct.h** add the following:
```c++
const int APPLY_MARK = 32;
```


#### Tracking Bleeding Targets
In structs.h a new struct needs to be added to track bleed in rooms.<br />
```c++
struct room_bleed_data {
	sh_int char_number; // Race number for players, virt_number for mobs
	byte data; // data/8 = time of bleed, data & 8 = direction
	sh_int condition; // Effective condition of the tracks in hours

	room_bleed_data() { char_number = 0; data = 0; condition = 0; }
};
```
Then we need to add the following to ```struct room_data```:
```c++
struct room_data {
	...
	struct room_bleed_data bleed_track[NUM_OF_TRACKS];
};
```




### Summon Swarm
This ability will summon a swarm scarab mob that attacks the target and hunts it. The mob will only last for a little while and shouldn't have more than 60 hps. However it will do some serious amount of damage and have a chance to poison. To use this ability the harad will use both mana and moves plus have a long tick time.

### Sun
When the harad is in the sun he will receive special bonuses, but if he's in a blizzard he'll receive something close to POA.

### Spears
Harads are allow to ambush with spears but at a reduced amount of damage.

Add the following to ranger.cc on ```do_ambush```:
```c++
  if ((ch->equipment[WIELD]->obj_flags.value[2] > 2)) {

	  if ((GET_RACE(ch) == RACE_HARADRIM) && (ch->equipment[WIELD]->obj_flags.value[3] != TYPE_SPEARS))
	  {
    	send_to_char("You need to wield a smaller weapon to surprise your victim.\r\n", ch);
    	return;
	  }
  }
```
This allows haradrims to ambush with a spear.


### Archery
Harads will receive a reduced 2% break percentage on arrows.

### Blinding Shot
This ability will be a bow ability with a percentage to temporarily blind the target. It will be a small duration much like bash or maybe a little less

