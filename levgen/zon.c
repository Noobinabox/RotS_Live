#include "zon.h"
#include <malloc.h>
#include <string.h>

#ifdef WIN32
#include <process.h>
#endif

#include "wld.h"

ZONcmd ZONreadCmd(FILE *fl){
	ZONcmd cmd;
	char buf[MAXBUF];
	fscanf(fl,"%c ",&cmd.cmd);
	if (feof(fl))	{ fprintf(stderr,"Unexpected end of file!\n"); exit(1); }
	if (ferror(fl)) { fprintf(stderr,"Error reading file!\n"); exit(1); }
	if (feof(fl))	{ fprintf(stderr,"Unexpected end of file!\n"); exit(1); }
	if (cmd.cmd=='S') goto quit;
	if (cmd.cmd=='$') { cmd.cmd='S'; goto quit;}
	fscanf(fl,"%d ",&cmd.if_flag);
	switch (cmd.cmd){
	// Additional command, affects last loaded mobile
	case 'A': fscanf(fl,"%ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4]); break;
	// Set state of door in room 0, exit 1, state 2
	case 'D': fscanf(fl,"%ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2]); break;
	// Object 0 to eq list of last 'M' loaded mob, EqPos 1, MaxExst 2, Prb 3, MaxOnMob 4, MaxObjMob 5
	case 'E': fscanf(fl,"%ld %ld %ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4],&cmd.args[5],&cmd.args[6]); break;
	// Give object 0 to last 'M' loaded mob, MxExst 1, Prb 2, MaxOnMob 3, MxObjMob 4
	case 'G': fscanf(fl,"%ld %ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4],&cmd.args[5]); break;
	// Give random object to last 'M' loaded mob, type 0, prb 1, maxOnMob 2, MaxObjMob 3, LvlGen 4, Good 5
	case 'H': fscanf(fl,"%ld %ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4],&cmd.args[5]); break;
	// Load objects 0,1,2,3,4,5,6 to last 'M' mob?
	case 'K': fscanf(fl,"%ld %ld %ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4],&cmd.args[5],&cmd.args[6]); break;
	// Set last_mob 2 or last_obj 2 in room 1 to num 3 with mode 0...?!
	case 'L': fscanf(fl,"%ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4]); break;
	// Load mobile 0 to room 1, maxexisting:2, prb:3, Difficulty:4, MaxLine:5, Trophy 6 
	case 'M': fscanf(fl,"%ld %ld %ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4],&cmd.args[5],&cmd.args[6]); break;
	// Load random mobile of type 0 to room 1, level 2, prob 3, dif 4, maxline 5, Trophy 6
	case 'N': fscanf(fl,"%ld %ld %ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4],&cmd.args[5],&cmd.args[6]); break;
	// Load Object 0 to room 1, MaxEsist 2, prb 3, MaxInRoom 4
	case 'O': fscanf(fl,"%ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4]); break;
	// Put obj 1 to obj 2 in room 0, maxext: 3, prb 4, maxinobj 5
	case 'P': fscanf(fl,"%ld %ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4],&cmd.args[5]); break;
	// random object to eq list of last 'M' mob, type:0, EqPos 1, prb 2, MaxOnMob 3, MaxObjMob 4, LvlGen 5, Good 6
	case 'Q': fscanf(fl,"%ld %ld %ld %ld %ld %ld %ld",&cmd.args[0],&cmd.args[1],&cmd.args[2],&cmd.args[3],&cmd.args[4],&cmd.args[5],&cmd.args[6]); break;
	// Remove Object 1 from room 0
	case 'R': fscanf(fl,"%ld %ld",&cmd.args[0],&cmd.args[1]); break;
	};
	buf[0]=0;
	WLDfgets(buf,MAXBUF,fl);
	if (strlen(buf)) cmd.comments=strdup(buf); else cmd.comments=NULL;
quit:
//	WLDfgets(buf,MAXBUF,fl);
	return cmd;
}

void ZONtraceZone(FILE *fl, zone *z, room *r){

#define CN (cmd.comments?cmd.comments:"\0")
#define LR (last_room==r->pnum)

	int i, last_room=0;
	ZONcmd cmd;
//	printf("Changing all commands for prototype room %d into commands for room %d.\n",r->pnum,r->inum);
	for (i=0;i<z->cmds;i++){
	cmd=z->cmd[i];
	switch (z->cmd[i].cmd){
	// Additional command, affects last loaded mobile
	case 'A': if LR fprintf(fl,"%c %d %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],cmd.args[1],cmd.args[2],cmd.args[3],cmd.args[4],CN); break;
	// Set state of door in room 0, exit 1, state 2
	case 'D': if (cmd.args[0]==r->pnum) fprintf(fl,"%c %d %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,r->inum,cmd.args[1],cmd.args[2],cmd.args[3],cmd.args[4],CN); break;
	// Object 0 to eq list of last 'M' loaded mob, EqPos 1, MaxExst 2, Prb 3, MaxOnMob 4, MaxObjMob 5
	case 'E': if LR fprintf(fl,"%c %d %ld %ld %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],cmd.args[1],cmd.args[2],cmd.args[3],cmd.args[4],cmd.args[5],cmd.args[6],CN); break;
	// Give object 0 to last 'M' loaded mob, MxExst 1, Prb 2, MaxOnMob 3, MxObjMob 4
	case 'G': if LR fprintf(fl,"%c %d %ld %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],cmd.args[1],cmd.args[2],cmd.args[3],cmd.args[4],cmd.args[5],CN); break;
	// Give random object to last 'M' loaded mob, type 0, prb 1, maxOnMob 2, MaxObjMob 3, LvlGen 4, Good 5
	case 'H': if LR fprintf(fl,"%c %d %ld %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],cmd.args[1],cmd.args[2],cmd.args[3],cmd.args[4],cmd.args[5],CN); break;
	// Load objects 0,1,2,3,4,5,6 to last 'M' loaded mob
	case 'K': if LR {
				last_room=r->pnum;
				fprintf(fl,"%c %d %ld %ld %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],cmd.args[1],cmd.args[2],cmd.args[3],cmd.args[4],cmd.args[5],cmd.args[6],CN); break;
			  }
	// Set last_mob 2 or last_obj 2 in room 1 to num 3 with mode 0...?!
	case 'L': if LR fprintf(fl,"%c %d %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],cmd.args[1],cmd.args[2],cmd.args[3],cmd.args[4],CN); break;
	// Load mobile 0 to room 1, maxexisting:2, prb:3, Difficulty:4, MaxLine:5, Trophy 6 
	case 'M': last_room=cmd.args[1];
			  if LR fprintf(fl,"%c %d %ld %ld %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],r->inum,cmd.args[2],cmd.args[3],cmd.args[4],cmd.args[5],cmd.args[6],CN);
			  break;
	// Load random mobile of type 0 to room 1, level 2, prob 3, dif 4, maxline 5, Trophy 6
	case 'N': last_room=cmd.args[1];
			  if LR fprintf(fl,"%c %d %ld %ld %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],r->inum,cmd.args[2],cmd.args[3],cmd.args[4],cmd.args[5],cmd.args[6],CN); break;
			  break;
	// Load Object 0 to room 1, MaxExist 2, prb 3, MaxInRoom 4
	case 'O': if (cmd.args[1]==r->pnum){
			  last_room=r->pnum;
			  fprintf(fl,"%c %d %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],r->inum,cmd.args[2],cmd.args[3],cmd.args[4],CN);
			  }; break;
	// Put obj 1 to obj 2 in room 0, maxext: 3, prb 4, maxinobj 5
	case 'P': if LR{
			  last_room=r->pnum;
			  fprintf(fl,"%c %d %ld %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,r->inum,cmd.args[1],cmd.args[2],cmd.args[3],cmd.args[4],cmd.args[5],CN);
			  }; break;
	// random object to eq list of last 'M' mob, type:0, EqPos 1, prb 2, MaxOnMob 3, MaxObjMob 4, LvlGen 5, Good 6
	case 'Q': if LR fprintf(fl,"%c %d %ld %ld %ld %ld %ld %ld %ld %s\n",cmd.cmd,cmd.if_flag,cmd.args[0],cmd.args[1],cmd.args[2],cmd.args[3],cmd.args[4],cmd.args[5],cmd.args[6],CN); break;
	// Remove Object 1 from room 0
	case 'R': if (cmd.args[0]==r->pnum) fprintf(fl,"%c %d %ld %ld %s\n",cmd.cmd,cmd.if_flag,r->inum,cmd.args[1],CN); break;
	// End of zonefile
	case 'S': fprintf(fl,"S\n#99999\n$~\n\n"); break;
	};
	};	
}

zone ZONload_zone(char *fn){
	zone z;
	int	tmp;
	char	buf[MAXBUF];
	FILE *fl;
		fl=fopen(fn,"rb");
		z.cmds=0;
		z.name=z.owners=z.description=NULL;
		z.num=0;
		fscanf(fl, " #%ld\n", &z.num);
		WLDfgets(buf,MAXBUF,fl); z.name=strdup(buf);
		if (z.name[0] == '$') goto quit;		/* end of file */
		WLDfgets(buf,MAXBUF,fl); z.description=strdup(buf);
		buf[0]=0;
		while (!strstr(buf,"~")){
		WLDfgets(buf,MAXBUF,fl); // read placeholder for map
		}
		WLDfgets(buf,MAXBUF,fl); z.owners=strdup(buf);
		fgets(buf,80,fl);
		z.level=0;
		sscanf(buf, "%c %d %d %d", &z.symbol,&z.x,&z.y, &tmp);
	    fscanf(fl, " %ld ", &z.top);
		fscanf(fl, " %ld ", &z.lifespan);
		fscanf(fl, " %ld ", &z.reset_mode);
		z.cmds=-1;
		do{
			z.cmds++;
			z.cmd[z.cmds]=ZONreadCmd(fl);
		} while (z.cmd[z.cmds].cmd!='S');
quit:
	fclose(fl);
	return z;
}

#include "maz.h"
	
void ZONwrite_zoneHead(FILE *fl, zone *z, level *lv){
	fprintf(fl,"#%ld ",z->num);
	fprintf(fl,"%s~\n%s\n",z->name,z->description);
	MAZdrawLevel(fl,lv);
	fprintf(fl,"~\n");
	fprintf(fl,"%s\n",z->owners);
	fprintf(fl,"%c %d %d %d\n",z->symbol,z->x,z->y,z->level);
	fprintf(fl,"%ld\n%ld\n%ld\n",z->top,z->lifespan,z->reset_mode);
}
