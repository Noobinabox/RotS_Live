#ifndef __zon__
#define __zon__

#include "structs.h" 
#include <stdio.h>

zone ZONload_zone(char *fn);
ZONcmd ZONreadCmd(FILE *fl);
void ZONwrite_zoneHead(FILE *fl, zone *z,level *lv);
void ZONtraceZone(FILE *fl, zone *z, room *r);

#endif 
