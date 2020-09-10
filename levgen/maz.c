#include <malloc.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "maz.h"
#include "wld.h"
#include "zon.h"
#include "config.h"


#define DIR(x) (x?1:0)
#define CDIR(i) ((i+2)%4)

#define px (x+h->rooms[i].rx)
#define py (y+h->rooms[i].ry)


int MAZreadNextHall(FILE *fl, hall *h){
	char buf[MAXBUF];
	rotsint rnum, rx, ry;
	rotsint cn[4];
	char s[MAXBUF];
	if (feof(fl)) return 0;
	bzero(buf,MAXBUF);
	if (!fgets(buf,MAXBUF,fl)) return 0;
	while ((!feof(fl))&&(buf[0]!='#')) fgets(buf,MAXBUF,fl);
	if (feof(fl)) return 0;
	buf[0]=0;
	while ((buf[0]!='#')&&(!feof(fl)))
		{
		fgets(buf,MAXBUF,fl); // get rest of headline, eg hall title, newline etc.
		if ((ferror(fl))&&(!feof(fl))) { fprintf(stderr,"MAZreadNextHall:Error reading file.\n"); exit(1); }
		if (buf[0]=='#') 
			{ 
			// Rewind so that next MAZreadNextHall() will start at '#'
			fseek(fl,ftell(fl)-strlen(buf),SEEK_SET);
			if (ferror(fl)) 
				{
				fprintf(stderr,"MAZreadNextHall:Error reading file.\n");
				exit(1);
				}
			break;
			} 
		s[0]=0;
		if (sscanf(buf,"%d %d %d %d %d %d %d%s\n",&rnum,&rx,&ry,&cn[NORTH],&cn[EAST],&cn[SOUTH],&cn[WEST],s)<7) return 1;
		if (s[0]) roomTable[rnum].symbol=s[0];
		MAZregisterRoom(h,&roomTable[rnum],rx,ry,cn);
		}
	if (feof(fl)) return 0;
	MAZreorderHall(h);
	return 1;
}

void MAZregisterRoom(hall *h, room *r,int rx, int ry, int cn[4]){
	int e;
	h->rooms[h->maxroom]=*r;
	for (e=NORTH;e<=WEST;e++) h->rooms[h->maxroom].exits[e]=cn[e];
	h->rooms[h->maxroom].exits[UP]=h->rooms[h->maxroom].exits[DOWN]=0;
	h->rooms[h->maxroom].rx=rx;  h->rooms[h->maxroom].ry=ry;
	h->maxroom++;
}

void MAZregisterHall(hall *h){
	h->nr=TopHall;
	hallTable[TopHall++]=*h;
}

void MAZreorderHall(hall *h){
	rotsint i;
	room r=WLDemptyRoom();
	// find room at 0,0
	for (i=0;h->rooms[i].pnum;i++)
		if ((h->rooms[i].rx==0)&&(h->rooms[i].ry==0)) break;
	if (!h->rooms[i].pnum) {fprintf(stderr,"MAZreorderHall:Error, could not find base room in hall.\n"); exit(1); }
	r=h->rooms[0];
	h->rooms[0]=h->rooms[i];
	h->rooms[i]=r;
}

void MAZwipePlane(){
	int x,y;
	for (x=0;x<SIZEX;x++)
	for (y=0;y<SIZEY;y++) plane[x][y]=WLDemptyRoom();
}

int countExits(room *r){
	int i,l;
	for (i=NORTH,l=0;i<=DOWN;i++) if (r->exits[i]) l++;
	return l;
}


void MAZdrawLevel(FILE *fl,level *lv){
#define mp(x,y) levelmap[x][y]
	int x,y;
	char c;
	for (y=0;y<lv->sizey;y++)
		{
		for (x=0;x<lv->sizex;x++)
			{
			c=' ';
			if (P(x,y).pnum>0) 
				{
				if (countExits(&P(x,y))>2) c='+'; else c='.';
				} 
			else c=' ';
			if (P(x,y).symbol) c=plane[x][y].symbol;
			fprintf(fl,"%c",c);
			if (ferror(fl)) 
				{
				fprintf(stderr,"MAZdrawLevel:Error writing file.\n"); 
				exit(1);
				}
			if ((P(x,y).exits[EAST])||(P(x+1,y).exits[WEST])) 
				fprintf(fl,"-"); else fprintf(fl," ");
			if (ferror(fl)) 
				{
				fprintf(stderr,"MAZdrawLevel:Error writing file.\n");
				exit(1);
				}
			}
		fprintf(fl,"\n");
		if (ferror(fl)) 
			{
			fprintf(stderr,"MAZdrawLevel:Error writing file.\n");
			exit(1);
			}
		for (x=0;x<lv->sizex;x++)
			if ((P(x,y).exits[SOUTH]) || (P(x,y+1).exits[NORTH]))
				fprintf(fl,"| "); else fprintf(fl,"  ");
		if (ferror(fl)) 
			{
			fprintf(stderr,"MAZdrawLevel:Error writing file.\n");
			exit(1);
			}
		fprintf(fl,"\n");
		if (ferror(fl)) 
			{
			fprintf(stderr,"MAZdrawLevel:Error writing file.\n");
			exit(1);
			}
		} 
}
#undef mp

void makePlane(level *lv); // forward declaration

/* Determine wether a hall can be placed at x,y.
Reasons for failure :
- Parts of the hall will land on already occupied space
- The hall will extend beyond level borders
- The maximum desired room number in the resulting zone will be exceeded
*/
int MAZcanPlaceHall(int x, int y, hall *h, level *lv){
	rotsint i;
	if (lv->maxroom+lv->base<nextFreeRoom+h->maxroom) return 0;
	for (i=0;(i<h->maxroom)&&(IN_MAZE(lv,px,py))&&(!P(px,py).pnum);i++);
	if (i==h->maxroom) return 1;
	return 0;
}


int pickHall(){ // Pick a random hall
	int r1=rand()%TopHall;
	int r2=rand()%TopHall;
	if (hallTable[r1].maxroom>hallTable[r2].maxroom) return r1;
	return r2;
}

void MAZremoveHall(int x, int y, hall *h){ // Remove a hall already placed
// Not very time-efficient, but the only way to get it done this century.
// Phase 1: removing all hall-rooms from the plane
	rotsint i;
	for (i=0;h->rooms[i].pnum;i++) 
		P(px,py)=WLDemptyRoom();
// Phase 2: decreasing allocated rooms indicator
	nextFreeRoom-=h->maxroom;
}

// Place a hall on the plane at x,y
void MAZputHall(int x, int y, hall *h, level *lv){
	rotsint i;
	int ce,e,tx,ty;
	for (i=0;h->rooms[i].pnum;i++) 
		{  
		P(px,py)=h->rooms[i];		// placing room on plane
		P(px,py).hallnr=h->nr;		// assigning hall to room
		roomSequence[nextFreeRoom-lv->base]=&P(px,py);
		P(px,py).inum=nextFreeRoom++;	// assigning new room number to room instance
		for (e=NORTH;e<=WEST;e++)
		if (P(px,py).exits[e]){
			ce=CDIR(e);
			tx=px+DX[e];
			ty=py+DY[e];
			if ((IN_MAZE(lv,tx,ty)&&(P(tx,ty).pnum)))
				{
				if (P(tx,ty).exits[ce]) 
					{
					P(tx,ty).exits[ce]=P(px,py).inum;
					P(px,py).exits[e]=P(tx,ty).inum;
					} 
				else
					{
					P(tx,ty).exits[ce]=0;
					P(px,py).exits[e]=0;
					}
				}
			else
				{
				if (!IN_MAZE(lv,tx,ty)) P(px,py).exits[e]=0;
				}
			}
		}
}

// Place a hall on the plane at x,y and engage a recursive placement of more halls.
// It is assumed that the caller has ascertained that the hall can be placed safely.

void MAZplaceHall(int x, int y, hall *h, int nr, level *lv){
	rotsint i,e,s,ec,placed;
	int tx,ty;
	int tries=0;

	// Phase 1: placing all rooms of the hall onto the plane
	MAZputHall(x,y,h,lv);
	// Phase 2: adding new halls recursively to all open exits.
	for (i=0;h->rooms[i].pnum;i++) // for all rooms we just set...
	for (e=NORTH;e<=WEST;e++)	// take each exit (that is not 0)...
		if (P(px,py).exits[e]>0)
		{
		tx=px+DX[e];
		ty=py+DY[e];
		if ((IN_MAZE(lv,tx,ty))&&(!P(tx,ty).pnum)) // look at the location the exit is leading to.
			{// if that spot is empty...
			tries=0;
again:	// ...try several times to place various halls before giving up...
			placed=0;
			tries++;
			s=pickHall();
			if (MAZcanPlaceHall(tx,ty,&hallTable[s],lv))
				{
				MAZputHall(tx,ty,&hallTable[s],lv); //... try to fill it.
				// but... can it be connected as well?
				ec=CDIR(e); // Complementary direction 
				if (!P(tx,ty).exits[ec]) MAZremoveHall(tx,ty,&hallTable[s]); 
				else // ouch, it didn't connect. Have to try again.
					{ 
					nextFreeRoom-=hallTable[s].maxroom; 
					// Since nextFreeRoom has already been advanced by MAZputHall() a few lines earlier,
					// and we will in the next line place the hall again, the numbering must be continuous.
					MAZplaceHall(tx,ty,&hallTable[s],s,lv); 
					placed=1;
					}
				}
			if ((!placed)&&(tries<TopHall)) goto again;
			}
// once all recursions return, we must interconnect rooms.	
		}
}

/* As MAZplaceHall() puts rooms on the level, it inter-connectes only rooms 
that were close in the generation sequence. For example :
 345		We have 8 rooms here, and they are created in ascending order (first 1, last 8).
 2 6		Room 1 is connected to room 2, 2 to 3 etc and 7 to 8 but rooms 1 and 8  
 187		have not been connected yet - the only way to get form 1 to 8 is via 2,3,4,5,6,7.
		MAZconnect searches for such neighbouring rooms that allow connections and connects them.
*/

void MAZconnect(level *lv){
	int x,y,tx,ty,e,ce;
//	return;
	for (y=0;y<lv->sizey;y++)
	for (x=0;x<lv->sizex;x++)
	if (!P(x,y).pnum) 
		{
		P(x,y)=WLDemptyRoom();
		}
	for (y=0;y<lv->sizey;y++)
	for (x=0;x<lv->sizex;x++)
	if (P(x,y).pnum) 
		{	
		for (e=NORTH;e<=WEST;e++) 
		if (P(x,y).exits[e])
			{
			tx=x+DX[e]; ty=y+DY[e]; // the spot our exit is leading to
			ce=CDIR(e); // complementary direction
			if ((!IN_MAZE(lv,tx,ty))||(P(tx,ty).pnum<1)||(P(tx,ty).exits[ce]<1)) 
				{
				if (P(x,y).preserveDir!=e) P(x,y).exits[e]=0;
				if (!IN_MAZE(lv,tx,ty)) 
					if (P(tx,ty).preserveDir!=e) P(tx,ty).exits[ce]=0;
				continue;
				}
		   	P(x,y).exits[e]=P(tx,ty).inum;
			}
		}
}

/* makePlane invokes other functions to create and write a level. */
void makePlane(level *lv ){
	rotsint x,d,l,i,t=0; 
// First, let's place the obligatory external exits
	printf("Placing halls...\n");
	for (i=0;i<TopConnection;i++)
		{
		t++;
		MAZputHall(connectionTable[i].x,connectionTable[i].y,&hallTable[0],lv);
		P(connectionTable[i].x,connectionTable[i].y).exits[connectionTable[i].dir]=connectionTable[i].exnum;
		P(connectionTable[i].x,connectionTable[i].y).preserveDir=connectionTable[i].dir;
		}
	for (l=0;l<TopConnection;l++){
	x=TopHall;
	while (x){
		x--;
		i=pickHall();
		for (d=NORTH;d<=WEST;d++)
			if (MAZcanPlaceHall(DX[d]+connectionTable[l].x,DY[d]+connectionTable[l].y,&hallTable[i],lv))
			{
			if (!hallTable[i].rooms[0].exits[CDIR(d)]) continue;
			MAZplaceHall(DX[d]+connectionTable[l].x,DY[d]+connectionTable[l].y,&hallTable[i],i,lv);
			x=0;
			}
		}
	}
	printf("Connecting plane...\n");
	MAZconnect(lv);
}

void MAZmakeLevel(config *cnf, level *lv){
	zone z;
	int x,y;
	FILE *fl;
	if (!(fl=fopen(lv->target_zone_name,"wb"))) { fprintf(stderr,"MAZmakeLevel:Error opening file %s.\n",lv->target_zone_name); exit(1); }
	nextFreeRoom=lv->base;
	makePlane(lv);
	z.description=strdup("~");
	z.level=0;
	z.lifespan=atoi(CONFvalue("zone.lifespan",cnf));
	z.name=scat(lv->zone_name,"~");
	z.num=lv->zonenr;
	z.owners=strdup("0");
	z.reset_mode=2;
	z.symbol=*(CONFvalue("zone.symbol",cnf));
	z.top=lv->base+lv->maxroom;
	z.x=lv->location_x;
	z.y=lv->location_y;
	fprintf(stdout,"Writing zone %s...",lv->target_zone_name);
	fflush(stdout);
	ZONwrite_zoneHead(fl,&z,lv);
	z=ZONload_zone(lv->source_zone_name);
	for (x=0;x<lv->sizex;x++)
	for (y=0;y<lv->sizey;y++)
		if (plane[x][y].pnum)
		ZONtraceZone(fl,&z,&plane[x][y]);
	fprintf(fl,"S\n#99999\n$~\n");
	if (ferror(fl)) { fprintf(stderr,"MAZmakeLevel:Error writing file.\n"); exit(1); }
	fclose(fl);
	fprintf(stdout,"Done\n");
	fprintf(stdout,"Writing world %s...",lv->target_world_name);
	fflush(stdout);
	fl=fopen(lv->target_world_name,"wb");
	printf("\n");
	MAZdrawLevel(stdout,lv);
	MAZwriteLevel(fl,lv);
	fclose(fl);
	printf("Done.\n");
}

void MAZwriteLevel(FILE * flout, level *lv){
	rotsint i,l;
	wldcache wc;
	l=lv->sizex*lv->sizey;
	wc.head=wc.tail=NULL;
	for (i=0;i<nextFreeRoom-lv->base;i++)
	if (roomSequence[i]->pnum>0)
	WLDTranslateRoom(flout,roomSequence[i],&hallTable[roomSequence[i]->hallnr],lv,&wc);
	fprintf(flout,"#99999\n\r$~\n\r");
	if (ferror(flout)) { fprintf(stderr,"MAZwriteLevel:Error writing file.\n"); exit(1); }
}

void MAZloadLevel(char *fnm, level *lv){
	FILE *f;
	char buf[MAXBUF];
	int x,y;
	char c;
	char t1[5];
	char t2[5];
	if (!(f=fopen(fnm,"rb"))) if (ferror(f)) 
		{
		fprintf(stderr,"MAZloadLevel:Error opening file %s.\n",fnm);
		exit(1);
		}
// Fast forward beyond properties section
	do 
		{
		buf[0]='\0';
		fgets(buf,MAXBUF,f);
		if ((ferror(f))&&(!feof(f))) { fprintf(stderr,"MAZloadLevel:Error reading file.\n"); exit(1); }
		}
	while ((!feof(f))&&(buf[0]!='~'));
	if (feof(f)) 
		{
		fprintf(stderr,"MAZloadLevel: unexpected end of file\n"); 
		exit(1); 
		}
// Read connection section
	buf[0]=0;
	while ((!feof(f))&&(buf[0]!='~'))
		{
		buf[0]=0;
		fgets(buf,MAXBUF,f);
		if ((ferror(f))&&(!feof(f))) 
			{
			fprintf(stderr,"MAZloadLevel:Error reading file.\n");
			exit(1); 
			}
		switch (buf[0])
			{
			case '@': // external connection
				sscanf(&buf[1],"%d %d %ld %d",&connectionTable[TopConnection].x,&connectionTable[TopConnection].y,&connectionTable[TopConnection].exnum,&connectionTable[TopConnection].dir);
				TopConnection++;
				break;
			case '!': // fields that shall remain empty
				sscanf(&buf[1],"%s %s",t1,t2);
				if (t1[0]=='*') x=rand() % lv->sizex; else x=atoi(t1);
				if (t2[0]=='*') y=rand() % lv->sizey; else y=atoi(t2);					
				P(x,y).pnum=Reserved;
				break;
			case 's': // show symbol on map
				sscanf(&buf[1],"%s %s %c",t1,t2,&c);
				if (t1[0]=='*') x=rand() % lv->sizex; else x=atoi(t1);
				if (t2[0]=='*') y=rand() % lv->sizey; else y=atoi(t2);					
				P(x,y).symbol=c;
				break;
			};
		};
		
	while (MAZreadNextHall(f,&hallTable[TopHall])) 	MAZregisterHall(&hallTable[TopHall]);
	TopHall--;
	printf("Finished with %d halls.\n",TopHall);
	fclose(f);
}
