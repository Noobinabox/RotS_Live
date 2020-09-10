/********************************************
 *											*
 *				levgen v1.0					*
 *											*
 ********************************************

Levgen is a world and zone generation utility for Circlemud conform
world and zone files, which was created for the ROTS mud. Based on
the settings in a configuration file it reads bits out of a world
file and scrambles them into a new wld. This is particulary helpful
when creating random mazes.

Aggrippa,
Athens 9/12/2000
*/
#include "structs.h"
#include "wld.h"
#include "maz.h"
#include "zon.h"
#include <time.h>
#include <stdlib.h>
#include "config.h"

// Re-declare extern variables 
char *DIRECTIONS[MAXDIR]={
	("north"),
	("east"),
	("south"),
	("west"),
	("up"),
	("down")
};

int DX[4]={
	(0),
	(1),
	(0),
	(-1)
};

int DY[4]={
	(-1),
	(0),
	(1),
	(0)
};

room roomTable[MaxRoom];
room *roomSequence[SIZEX*SIZEY];
rotsint TopRoom;
hall hallTable[MaxHall];
int TopHall;
rotsint nextFreeRoom;
room plane[SIZEX][SIZEY];
int lastHall;
connection connectionTable[MaxConnection];
int TopConnection;


// Initialise room-, world-, connection- and zonetables
void init(){
	int i,e;
#ifdef WIN32
	//srand(2);
	srand( (unsigned)time( NULL ) );
#else
	sleep(1);
	srand( (unsigned)time( NULL ) );
#endif
	lastHall=rand() % 100;
	for (i=0;i<MaxRoom;i++) roomTable[i]=WLDemptyRoom();
	TopRoom=0;
	for (i=0;i<MaxHall;i++){
		for (e=0;e<HallSize;e++) hallTable[i].rooms[e]=WLDemptyRoom();
	}
	TopHall=0;
	TopConnection=0;
	for (i=0;i<MaxConnection;i++){
		connectionTable[i].dir=0;
		connectionTable[i].exnum=0;
		connectionTable[i].x=0;
		connectionTable[i].y=0;
	}
}

void printUsage(char *c){
	printf("Usage:\n%s: configfile.lev\n",c);
	exit(1);
}

int main(int argc, char *argv[]){
	level lv;
	struct config c;
	int x,y;
	if (argc==1) printUsage(argv[0]);
	init();
// Read instructions from level file for level creation 
	c=CONFread(argv[1]);
// Load configuration into a level struct.
	lv.base=atoi(CONFvalue("room.base",&c));
	lv.zonenr=atoi(CONFvalue("zone.nr",&c));
	lv.maxroom=atoi(CONFvalue("room.max",&c));
	lv.sizex=atoi(CONFvalue("size.x",&c));
	lv.sizey=atoi(CONFvalue("size.y",&c));	
	lv.source_world_name=CONFvalue("source.wld",&c);
	lv.source_zone_name=CONFvalue("source.zon",&c);
	lv.target_world_name=CONFvalue("output.wld",&c);
	lv.target_zone_name=CONFvalue("output.zon",&c);
	lv.zone_name=CONFvalue("zone.name",&c);
	lv.location_x=atoi(CONFvalue("zone.x",&c));
	lv.location_y=atoi(CONFvalue("zone.y",&c));
	nextFreeRoom=lv.base;
// Loading stereometric room descriptions
	printf("Registering %s\n",lv.source_world_name);
	WLDloadFile(lv.source_world_name);
	printf("Registering new maze\n");
// Load hall definitions
	for (x=0;x<lv.sizex+1;x++)
	for (y=0;y<lv.sizey+1;y++)
	P(x,y)=WLDemptyRoom();
	MAZloadLevel(argv[1],&lv);
// Construct & save the level
	MAZmakeLevel(&c,&lv);
	return 0;
}
