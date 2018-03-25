# Game Engine Changes

Documentation for proposed changes to the game engine in general.

### Table of Content
* [Game Loop](#game_loop)
* [Status Affects](#affects)
* [Wait System] (#wait)

## game_loop
Currently the game loop uses mini-ticks in a very unintuitive way to determine game time.
1. Add a clock class that can be used for handling time with floats.
2. Add a 'tick' function to char_data, room_data, etc that allows updates to happen in the world.
   a. These ticks can happen at differently defined intervals so we are not ticking through every room
      in the world every tick.
	  
## affects
TODO(drelidan): Describe this.

## wait
TODO(drelidan): Describe this.

