#Makefile for levgen
CC = gcc
FLAGS =  -g -w -Wall -Wformat -fstrict-aliasing -pedantic  -Wunused -Wuninitialized
OBJFILES = config.o maz.o zon.o wld.o

# Remove comment for MS Windows
# FLAGS = -DWIN32


all:	levgen
clean:	
	rm -f *.o


# Dependencies for levgen

wld.o :	structs.h wld.c wld.h
	$(CC) -c $(FLAGS) wld.c
config.o : structs.h config.c config.h wld.h
	$(CC) -c $(FLAGS) config.c
maz.o : structs.h maz.c maz.h zon.h
	$(CC) -c $(FLAGS) maz.c
zon.o : structs.h zon.c zon.h wld.c wld.h maz.h
	$(CC) -c $(FLAGS) zon.c
levgen : structs.h levgen.c wld.c wld.h config.c config.h maz.c maz.h $(OBJFILES)
	$(CC) $(FLAGS) levgen.c $(OBJFILES) -o levgen
