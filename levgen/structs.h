#ifndef __levgen_structs_defines__
#define __levgen_structs_defines__

#define MaxHall 100	// A maximum number of different halls to process
#define HallSize 25 // The largest hall (in rooms) to expect
#define Reserved -1 // room.pnum takes this value when the room should be left blank
#define P(x,y) plane[x][y]	
#define hasExits(r) (r.exits[NORTH]||r.exits[EAST]||r.exits[SOUTH]||r.exits[WEST])

// Is position (x,y) - measured from upper left corner - inside the level boundaries?
#define IN_MAZE(lv,x,y) ((x>=0)&&(x<lv->sizex)&&(y>=0)&&(y<lv->sizey))
// Maximum expected level dimensions - the actual dimensions are specified in the .lev file
#define SIZEX 30
#define SIZEY 30

// Maximum number of connections to (and with) the outside world.
#define MaxConnection 200

#define NORTH 0
#define EAST  1
#define SOUTH 2
#define WEST  3
#define UP    4
#define DOWN  5

#define MAXDIR 6
#define MAXBUF 160
#ifdef WIN32
#define bzero(a,b) memset(a,0,b)
#endif
#define MaxRoom 99999
#define ISNUM(a) (a>='0')&&(a<='9')

#define ZONARGS 8
#define MAXCMD 200
#define MAXPAIRS 100 // It unlikely that we'll need more than 100 settings

static char *DIRNAME[]={"North", "East", "South", "West", "Up", "Down"};

#endif

#ifndef __levgen_rotsint__
#define __levgen_rotsint__
typedef long int rotsint;

#endif

#ifndef __levgen_room__
#define __levgen_room__
struct room{
	rotsint pnum; // the prototype room number
	rotsint inum; // the instance number
	rotsint exits[MAXDIR];
	rotsint rx,ry;
	rotsint hallnr;
	rotsint preserveDir; // >-1 if this direction is to be preserved
	char symbol;
};

typedef struct room room;

#endif

#ifndef __levgen_level__
#define __levgen_level__

struct level{	// Describes geometric and logic characteristics of a level
	int sizex,sizey;		// Dimensions
	rotsint maxroom,base,zonenr;	// Highest room number, room base number and zone number
	char *target_zone_name;		// Name of files
	char *target_world_name;
	char *source_zone_name;
	char *source_world_name;
	char *alt_source_zone_name;
	char *alt_source_world_name;

	char *zone_name;
	int location_x,location_y;
};

typedef struct level level;

#endif

#ifndef __levgen_hall__
#define __levgen_hall__

struct hall{	// Describes stereomeric characteristics of a hall
	room rooms[HallSize];	// Rooms in a hall
	int maxroom;			// Number of rooms in the hall
	int nr;					// Internal hall number
	int x,y;				// Position of hall on the level
};

typedef struct hall hall;

#endif

#ifndef __levgen_connection__
#define __levgen_connection__

struct connection{	// Describes connections to an outside room
	int x,y; // connect extern room to a room in level at x,y
	rotsint exnum;// room number of extern room
	int dir; // direction to connect to (one will walk into to get out of the level)
};

typedef struct connection connection;

#endif


extern char* DIRECTIONS[MAXDIR];
extern int DX[4];
extern int DY[4];

extern room roomTable[MaxRoom];
extern rotsint TopRoom;

extern hall hallTable[MaxHall];	// Collection of halls levgen can rely on to construct a level
extern int TopHall;	// Number of halls in hallTable[]
extern rotsint nextFreeRoom;	// next free room number to be allocated
extern int TopConnection;	// Number of external connections in connectionTable[]
extern room plane[SIZEX][SIZEY];	// The level itself
extern connection connectionTable[MaxConnection]; // External connections
extern room *roomSequence[SIZEX*SIZEY]; // Order in which rooms will be written to .wld

#ifndef __levgen_zoncmd__
#define __levgen_zoncmd__

struct ZONcmd{
	char cmd;
	int if_flag;
	rotsint args[ZONARGS];
	char *comments;
};

typedef struct ZONcmd ZONcmd;

#endif

#ifndef __levgen_zone__
#define __levgen_zone__

struct zone{
	rotsint num;
	char *name, *description;
	char *owners; // can be 0 0
	char symbol;
	int x,y,level;
	rotsint top,lifespan,reset_mode;
	int cmds;
	ZONcmd cmd[MAXCMD];
};

typedef struct zone zone;

#endif

#ifndef __levgen_pair__
#define __levgen_pair__

struct pair{	// A pair consists of (key_name,key_value)
	char *field;
	char *value;
};

typedef struct pair pair;

struct config{	// This struct represents the entire configuration section of a .lev file
	struct pair fields[MAXPAIRS];	// key-value pairs
	int items;						// number of pairs in configuration
};

typedef struct config config;

#ifndef __levgen_wldcache__
#define __levgen_wldcache__


typedef struct cacheLine cacheLine;
typedef struct wldcache wldcache;

struct cacheLine{
	rotsint rnum;
	long position;
	cacheLine *next;
};

struct wldcache{
	cacheLine *head;
	cacheLine *tail;
};

#endif

#endif
