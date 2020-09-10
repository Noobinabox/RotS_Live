#ifndef __wld__
#define __wld__

#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <process.h>
#endif

#include "structs.h"

char * scat(char *c1, char *c2);
void WLDfgets(char *c, int max, FILE *fl);
void errlog(char *c);
void WLDregisterRoom(room r);
int WLDreadNextRoom(FILE *fl, room *r);
void WLDprintRoom(room r);
void WLDloadFile(char *fnm);

int WLDTranslateRoom(FILE *out, room *r, hall *h, level *lv, wldcache *wc);
room *WLDruplicateRoom(room R);
room WLDemptyRoom();

long WLDcached(wldcache *wc, rotsint r);
void WLDaddLine(wldcache *wc, cacheLine *cl);
cacheLine *WLDtoCacheLine(rotsint rnum, long position);

#endif
