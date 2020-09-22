/*			maz.h & maz.c
These files do most of the level (or maze) generation logic.
A level is generated out of clousters of rooms, called halls rather
than single rooms. Imagine halls like Tetris pieces. You can place
only a whole piece, never a single square. A hall can contain up to
HallSize rooms.
*/

#ifndef __maz__
#define __maz__

#include "structs.h"

// Read another hall description from 'fl' into 'h'. Returns 0 upon failure.
int MAZreadNextHall(FILE *fl, hall *h);

// Register a room 'r' into hall 'h', where rx,ry is it's relative location in the hall.
// cn[] are directions to oher rooms
void MAZregisterRoom(hall *h, room *r,int rx, int ry, int cn[4]);

// Shuffle rooms so that the first room in h.rooms is at relative position 0,0
void MAZreorderHall(hall *h);

// Initialize a plane
void MAZwipePlane();

// Construct a level
void MAZmakeLevel(config *cnf, level *lv);

// Draw a level to a file
void MAZdrawLevel(FILE *fl,level *lv);

// Safe a level to file
void MAZwriteLevel(FILE * flout, level *lv);

// Load halls from file 'fnm'
void MAZloadLevel(char *fnm, level *lv);

#endif 
