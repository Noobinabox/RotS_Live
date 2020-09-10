#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "wld.h"

/* The configuration section of a .lev file is rather simple :
It starts at the begining of the file and ends with a ~ character.
It consists of entries like :
keyname=value
where each entry occupies a \n terminated line. Blank spaces or
any other characters will be _included_ in the keyname/value definition.
*/

struct config CONFread(char *fn){
  int line = 1;
  char buf[80];
  char *c;
  FILE *f;
  config p;
  p.items=0;
  if (!(f=fopen(fn,"rb"))) {
    fprintf(stderr, "CONFread:Error opening %s.\n",fn);
    exit(1);
  }

  while (!feof(f)) {
    WLDfgets(buf,80,f);
    if(buf[0]=='~')
      goto quit;
    c = strstr(buf,"=");
    if(!c) {
      fprintf(stderr,"CONFread: %d: format convention violation, missing =\n",
	      line);
      exit(1);
    }
    if((buf[0]=='#') || (buf[0]=='\n') || (buf[0]=='\r'))
      continue;

    *c='\0'; c++;
    p.fields[p.items].field=strdup(buf);
    p.fields[p.items].value=strdup(c);
    p.items++;
    line++;
  };

 quit:
  fclose(f);
  return p;
}

char *CONFvalue(char *f, struct config *c){
	int i;
// Search through the configuration for a key 'f'
	for (i=0;(i<c->items)&&(strcmp(f,c->fields[i].field));i++);
// If not found, return NULL
	if (i==c->items) {fprintf(stderr,"CONFvalue:Value %s not registered!\n",f); exit(1); }
// Return the key value. Note that this value must not be altered or free()'d
	return c->fields[i].value;
}

