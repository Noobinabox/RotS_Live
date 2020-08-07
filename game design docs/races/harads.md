# Haradrim Race Configration

Listed below is all the racial configuration and design for Haradrims.
* [ ] [Mark](#mark)
* [ ] [Blind](#blind)
* [ ] [Ambush with Spears](#ambush-with-spears)
* [ ] [Bonus to Spears and Bows](#bonus-to-spears-and-bows)
* [ ] [Lay Traps](#lay-traps)
* [ ] [Windblast](#windblast)
* [ ] [Bend Time](#bend-time)
* [ ] [Summon Swarm](#summon-swarm)
* [ ] [Sun Bonus](#sun-bonus)
* [ ] [Shadowmend](#shadowmend)

### Mark
This is a touch attack made by the Haradrim to open a festering wound on the side of the target. If the attack hits the victim will have their movement and health regeneration slowed down. Healing will reduce the duration of this skill and healing while the victim is health will reduce it even more.

Mark also makes the target leave a blood trail in the room and all Haradrims have been trained to see this without using the hunt command. When a victim is marked they will have a special tag ``(marked)`` which only Haradrims can see.

### Blind
This skill requires the player have item 2100, a small handfull of dust, in their inventory and it uses 20 mana to use. This skill will blind and apply haze to the victim for a short duration. After using this skill it consumes item 2100 on a hit or miss.

### Ambush with Spears
The Haradrims should be able to naturally ambush with a spear, at a reduced damage.

### Bonus to Spears and Bows
Haradrims should receive a speed and offensive bonus to spears. They should have all arrow break percentages reduced in half.

### Lay Traps
Haradrims should be able to apply a trap to a room that damages anything that doesn't save against the trap entering the room.

### Windblast
A wave of thunderous force sweeps out from the Haradrim. Each creature in the room must make a saving throw. Any creatures that fail their saving throw take damage and are pushed out of the room, random directions. On a successful save, they take reduced damage. 

>This skill takes 50 mana to use and 50 movement.

### Bend Time
This skill will allow the Haradrim to double it's current energy and increases their temporary ob by 20. The cost of this skill should be their max_mana and 100 moves. The skill should have a short duration.

### Summon Swarm
This skill will summon 6d6 swarms with a static based on the haradrims ranger level (ranger_level / 3). The swarms should be generated in rooms all around the haradrim and then move towards their master. This will required a special program for the mobs and their behavior.

For this skill the swarm can either be static in their rooms and attack anything that's not their master or form on their master. If they mobs are static the haradrim can use the swarms at a buffer or have them follow him around as buffers/parry split.

>Using this skill should take all the Haradrims mana and all but 5 moves.
### Sun Bonus
When the Haradrim is in the sun he will receive special bonuses, but if he's in a blizzard he'll receive something close to POA.

### Shadowmend
This skill should allow the Haradrim to enter the shadow realm for a very short duration and requires a large amount of mana to use but a short tick. After entering the shadow realm it should disengage the Haradrim from combat and allow the passing through doors such as the skill shift.