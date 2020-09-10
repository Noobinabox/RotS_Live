#include "wld.h"
#include <malloc.h>

char * scat(char *c1, char *c2){
	int i,l,m;
	char *c;
	if ((!c1)||(!c2)) 
		{
		fprintf(stderr,"scat: c1 or c2 is NULL\n");
		exit(1);
		}
	c=(char*)malloc(strlen(c1)+strlen(c2)+1);
	l=strlen(c1);
	for (i=0;i<l;i++) c[i]=c1[i];
	l+=strlen(c2);
	for (m=0;i<=l;i++,m++) c[i]=c2[m];
	return c;
}

void WLDfgets(char *c, int max, FILE *fl){
	int i;
	i=0;
	while ((!feof(fl))&&(i<max))
		{
		fread(&c[i],1,1,fl);
		if ((ferror(fl))&&(!feof(fl))) 
			{
			fprintf(stderr,"WLDfgets:Error reading file!\n");
			exit(1);
			}
		if (c[i]=='\n') break;
		if (c[i]!='\r') i++;
		}
	c[i]=0;
}


void WLDregisterRoom(room r){
//	for (i=NORTH;i<=DOWN;i++) r.exits[i]=0;
	roomTable[r.pnum]=r;
}

room WLDemptyRoom(){
	int i;
	room r;
	r.pnum=r.rx=r.ry=r.inum=r.hallnr=0;
	r.preserveDir=-1;
	for (i=NORTH;i<=DOWN;i++) r.exits[i]=0;
	r.symbol=0;
	return r;
}

int readOnRoom(FILE *fl, room *r){
	char buf[MAXBUF];
	int dir,tildes,i;
	rotsint f1,f2,f3,f4;
	bzero(buf,MAXBUF);
	r->preserveDir=-1;
	do
		{ 
		WLDfgets(buf,MAXBUF,fl);
		if (strstr(buf,"$")) return 0;
		if (strstr(buf,"#")) 
			{ 
			i=strlen(buf)+2;
			fseek(fl,ftell(fl)-i,SEEK_SET);
			return 1;
			}
		if ((strlen(buf)>1)&&(buf[1]=='D')&&(ISNUM(buf[2])))
			{
			sscanf(buf,"\rD%d",&dir);
			tildes=0;
			do
				{
				fgets(buf,MAXBUF,fl);
				if (ferror(fl))	{ fprintf(stderr,"readOnRoom:Error while reading file!\n"); exit(1); }
					if (feof(fl))	{ fprintf(stderr,"readOnRoom:Unexpeced end of file!\n"); exit(1); }
					if (strstr(buf,"$")) return 0;
					if (strstr(buf,"~")) tildes++;
					} while (tildes<2);
				fscanf(fl,"%ld %ld %ld %ld",&f1,&f2,&f3,&f4);
				if ((ferror(fl))&&(!feof(fl))) { fprintf(stderr,"readOnRoom:Error while reading file!\n"); exit(1); }
				r->exits[dir]=f3;
			};

		} 
	while (!feof(fl));
	return 1;
}


int WLDreadNextRoom(FILE *fl, room *r){
	char buf[MAXBUF];
	char c;
	bzero(buf,MAXBUF);
	*r=WLDemptyRoom();
	if (feof(fl)) return 0;
	do
		{
		c=getc(fl);
		}
	while ((!ferror(fl))&&(!feof(fl))&&((c!='#')));
	if (feof(fl))	
		{
		fprintf(stderr,"WLDreadNextRoom:Unexpected end of file!\n");
		exit(1);
		}
	if (ferror(fl))	
		{
		fprintf(stderr,"WLDreadNextRoom:Error reading file!\n");
		exit(1);
		}
	fscanf(fl,"%ld",&r->pnum);
	return readOnRoom(fl,r);
}

long WLDcached(wldcache *wc, rotsint r){
	cacheLine *cl;
	if (!wc) 
		{
		fprintf(stderr,"WLDcached: wc is NULL\n");
		exit(1);
		}
	cl=wc->head;
	while ((cl)&&(cl->rnum!=r)) cl=cl->next;
	if (!cl) return -1;
	return cl->position;
}

void WLDaddLine(wldcache *wc, cacheLine *cl){

	if (!wc->head) 
		{
		wc->tail=wc->head=cl;
		return;
		}
	wc->tail->next=cl;
	wc->tail=cl;
}
	
cacheLine *WLDtoCacheLine(rotsint rnum, long position){
	cacheLine *cl=(cacheLine *)malloc(sizeof (cacheLine));
	cl->rnum=rnum;
	cl->position=position;
	cl->next=NULL;
	return cl;
}
	
int WLDTranslateRoom(FILE *out, room *r, hall *h, level *lv, wldcache *wc){
	char buf[MAXBUF];
	int dir,tildes;
	long p=0;
	int mustBeCached=0;
	rotsint i,f1,f2,f3,f4;
	rotsint dirs[6]={1,1,1,1,1,1};
	char *c;
	FILE *fl;
	bzero(buf,MAXBUF);
	if (!(fl=fopen(lv->source_world_name,"rb"))) 
		{
		fprintf(stderr,"WLDTranslateRoom:Error opening %s!\n",lv->source_world_name);
		exit(1);
		}
	if ((p=WLDcached(wc,r->pnum))==-1) mustBeCached=1; else
		{
		fseek(fl,p,SEEK_SET);
		goto in_loop;
		};
	fseek(fl,0,SEEK_SET);
	if (feof(fl)) return 1;
	while (!feof(fl)) 
		{
		WLDfgets(buf,MAXBUF,fl);
		if ((c=strstr(buf,"#")))
			{
			sscanf(c,"#%ld",&i);
			if (i==r->pnum) // found prototype room
				{
				if (mustBeCached) 
						WLDaddLine(wc,WLDtoCacheLine(r->pnum,ftell(fl)));
in_loop:
				fprintf(out,"#%ld\n\r",r->inum);
				if (ferror(out))	
					{
					fprintf(stderr,"WLDTranslateRoom:Error writing file!\n");
					exit(1);
					}
			do
				{
				WLDfgets(buf,MAXBUF,fl);
				if (strstr(buf,"$")) goto quit; 
				if (strstr(buf,"#")) goto quit;
				if ((buf[0]=='S')&&(buf[1]=='\0')) goto quit;
				if (*buf) fprintf(out,"%s\n\r",buf);
				if ((strlen(buf)>1)&&(buf[0]=='D')&&(ISNUM(buf[1])))
					{
					sscanf(buf,"D%d",&dir);
					tildes=0;
					do
						{
						WLDfgets(buf,MAXBUF,fl);
						if (strstr(buf,"$")) goto quit;
						if (*buf!=0) fprintf(out,"%s\n\r",buf);
						if (strstr(buf,"~")) tildes++;
						}
					while (tildes<2);
					if (fscanf(fl,"\r%ld %ld %ld %ld",&f1,&f2,&f3,&f4)!=4) 
						{
						fprintf(stderr,"WLDTranslateRoom:Wrong format in wld file!\n"); 
						exit(1); 
						}
					if (ferror(fl))	
						{
						fprintf(stderr,"WLDTranslateRoom:Error reading file!\n");
						exit(1);
						}
					fprintf(out,"%ld %ld %ld %ld\n\r",f1,f2,r->exits[dir],f4);
					if (ferror(out))	
						{
						fprintf(stderr,"WLDTranslateRoom:Error writing file!\n"); 
						exit(1); 
						}
					dirs[dir]=0;
					}

				}
			while (!feof(fl));
		
			}
		}
	}
quit:
	fclose(fl);
	if (r->preserveDir>-1) dirs[r->preserveDir]=0;
	for (i=NORTH;i<=WEST;i++) if ((dirs[i])&&(r->exits[i]))
		{
		// here we have to 'invent' a generic exit to another hall
		fprintf(out,"D%ld\n\r~\n\r~\n\r0 0 %ld 0\n\r",i,r->exits[i]);
		if (ferror(out))	
			{
			fprintf(stderr,"WLDTranslateRoom:Error writing file!\n");
			exit(1); 
			}
		}
	if (r->preserveDir>-1) fprintf(out,"D%ld\n\r~\n\r~\n\r0 0 %ld 0\n\r",r->preserveDir,r->exits[r->preserveDir]);
	fprintf(out,"S\n\r");
	if (ferror(out))	
		{
		fprintf(stderr,"WLDTranslateRoom:Error writing file!\n");
		exit(1);
		}
	return 1;
}


room *WLDduplicateRoom(room R){
	room *r=(room*)malloc(sizeof R);
	*r=R;
	return r;
}

void WLDloadFile(char *fnm){
	FILE *f;
	room r;
	if (!(f=fopen(fnm,"rb"))) 
		{
		fprintf(stderr,"WLDloadFile:Error opening file %s!\n",fnm);
		exit(1);
		}
	while (WLDreadNextRoom(f,&r)) 
		{
		WLDregisterRoom(r);
		}
	fclose(f);
}
