/*			config.h

config.h and config.c process the head of a .lev file, by reading it's
configuration settings into a memory structure. This informationcan be
retrieved at anytime.
*/

#ifndef __config__
#define __config__
#include "structs.h"

struct config CONFread(char *fn);	// Read file 'fn' into a config struct.

char *CONFvalue(char *f, struct config *c);	
// Return value of key 'f' in 'c', return NULL if no matching key could be found

#endif 
