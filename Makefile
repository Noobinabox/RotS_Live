#compiler name
CC = g++

#compiler flags you want to use (other than profiling, such as -Wall)
MYFLAGS = -fstrict-aliasing -Wall -funsigned-char

#flags for profiling (see hacker.doc for more information)
PROFILE = -g

#remove the hash marks below if compiling under AIX
#CC = g++
#COMMFLAGS = -D_BSD

#remove the hash mark below if compiling under SVR4
#OSFLAGS = -DSVR4
#LIBS = -lsocket -lnsl

#remove the has mark below if compiling under IRIX
#LIBS = -lmalloc

#############################################################################

CFLAGS = $(MYFLAGS) $(PROFILE) $(OSFLAGS)

OBJFILES = act_comm.o act_info.o act_move.o act_obj1.o act_obj2.o act_offe.o \
	act_othe.o act_soci.o act_wiz.o ban.o boards.o char_utils.o clerics.o color.o comm.o \
	config.o consts.o db.o fight.o graph.o handler.o interpre.o environment_utils.o  limits.o\
	mail.o mystic.o mage.o mobact.o modify.o mudlle.o mudlle2.o obj2html.o object_utils.o objsave.o \
	pkill.o profs.o ranger.o script.o shapemdl.o shapemob.o shapeobj.o shaperom.o shapezon.o shapescript.o shop.o \
	signals.o spec_ass.o spec_pro.o spell_pa.o utility.o wait_functions.o weather.o zone.o


ageland:        /bin/ageland

/bin/ageland:	$(OBJFILES)

all:	ageland

clean:
	rm -f *.o

# Dependencies for the main mud

comm.o : comm.cc structs.h utils.h comm.h interpre.h handler.h db.h \
	limits.h
	$(CC) -c $(CFLAGS) $(COMMFLAGS) comm.cc
char_utils.o : char_utils.cpp base_utils.h char_utils.h object_utils.h \
	environment_utils.h structs.h handler.h
	$(CC) -c $(CFLAGS) char_utils.cpp
environment_utils.o : environment_utils.cpp structs.h environment_utils.h
	$(CC) -c $(CFLAGS) environment_utils.cpp
object_utils.o : object_utils.cpp structs.h object_utils.h 
	$(CC) -c $(CFLAGS) object_utils.cpp
wait_functions.o : wait_functions.cpp structs.h base_utils.h char_utils.h object_utils.h  comm.h
	$(CC) -c $(CFLAGS) wait_functions.cpp
act_comm.o : act_comm.cc structs.h utils.h comm.h interpre.h handler.h \
	db.h
	$(CC) -c $(CFLAGS) act_comm.cc
act_info.o : act_info.cc structs.h utils.h comm.h interpre.h \
	handler.h db.h spells.h limits.h
	$(CC) -c $(CFLAGS) act_info.cc
act_move.o : act_move.cc structs.h utils.h comm.h interpre.h \
	handler.h db.h spells.h
	$(CC) -c $(CFLAGS) act_move.cc
act_obj1.o : act_obj1.cc structs.h utils.h comm.h interpre.h handler.h \
	db.h spells.h
	$(CC) -c $(CFLAGS) act_obj1.cc
act_obj2.o : act_obj2.cc structs.h utils.h comm.h interpre.h handler.h \
	db.h spells.h limits.h
	$(CC) -c $(CFLAGS) act_obj2.cc
act_offe.o : act_offe.cc structs.h utils.h comm.h interpre.h \
	handler.h db.h spells.h limits.h
	$(CC) -c $(CFLAGS) act_offe.cc
act_othe.o : act_othe.cc structs.h utils.h comm.h interpre.h handler.h \
	db.h spells.h limits.h
	$(CC) -c $(CFLAGS) act_othe.cc
act_soci.o : act_soci.cc structs.h utils.h comm.h interpre.h \
	handler.h db.h spells.h
	$(CC) -c $(CFLAGS) act_soci.cc
act_wiz.o : act_wiz.cc structs.h utils.h comm.h interpre.h \
	handler.h db.h spells.h limits.h profs.h
	$(CC) -c $(CFLAGS) act_wiz.cc
handler.o : handler.cc structs.h utils.h comm.h db.h handler.h interpre.h
	$(CC) -c $(CFLAGS) handler.cc 
db.o : db.cc structs.h utils.h db.h comm.h handler.h limits.h spells.h \
        interpre.h
	$(CC) -c $(CFLAGS) db.cc
ban.o : ban.cc structs.h utils.h comm.h interpre.h handler.h db.h
	$(CC) -c $(CFLAGS) ban.cc
interpre.o : interpre.cc structs.h comm.h interpre.h db.h utils.h \
	limits.h spells.h handler.h profs.h
	$(CC) -c $(CFLAGS) interpre.cc 
utility.o : utility.cc structs.h utils.h comm.h
	$(CC) -c $(CFLAGS) utility.cc
spec_ass.o : spec_ass.cc structs.h db.h interpre.h utils.h
	$(CC) -c $(CFLAGS) spec_ass.cc
spec_pro.o : spec_pro.cc structs.h utils.h comm.h interpre.h \
	handler.h db.h spells.h limits.h
	$(CC) -c $(CFLAGS) spec_pro.cc
limits.o : limits.cc structs.h limits.h utils.h spells.h comm.h db.h handler.h          profs.h
	$(CC) -c $(CFLAGS) limits.cc
fight.o	: fight.cc structs.h utils.h comm.h handler.h interpre.h db.h spells.h limits.h
	$(CC) -c $(CFLAGS) fight.cc
weather.o : weather.cc structs.h utils.h comm.h handler.h interpre.h db.h
	$(CC) -c $(CFLAGS) weather.cc
shop.o : shop.cc structs.h comm.h handler.h db.h interpre.h utils.h
	$(CC) -c $(CFLAGS) shop.cc
mystic.o : mystic.cc structs.h utils.h comm.h spells.h handler.h limits.h \
	interpre.h db.h
	$(CC) -c $(CFLAGS) mystic.cc
mage.o  : mage.cc structs.h utils.h comm.h spells.h handler.h limits.h \
        interpre.h db.h
	${CC} -c ${CFLAGS} mage.cc 
spell_pa.o : spell_pa.cc structs.h utils.h comm.h db.h interpre.h \
	spells.h handler.h
	$(CC) -c $(CFLAGS) spell_pa.cc 
mobact.o : mobact.cc utils.h structs.h db.h comm.h interpre.h handler.h
	$(CC) -c $(CFLAGS) mobact.cc
modify.o : modify.cc structs.h utils.h interpre.h handler.h db.h comm.h 
	$(CC) -c $(CFLAGS) modify.cc
consts.o : consts.cc structs.h limits.h
	$(CC) -c $(CFLAGS) consts.cc
objsave.o : objsave.cc structs.h comm.h handler.h db.h interpre.h \
	utils.h spells.h
	$(CC) -c $(CFLAGS) objsave.cc
boards.o : boards.cc structs.h utils.h comm.h db.h boards.h interpre.h \
	handler.h
	$(CC) -c $(CFLAGS) boards.cc
signals.o : signals.cc utils.h structs.h
	$(CC) -c $(CFLAGS) signals.cc
graph.o : graph.cc structs.h utils.h comm.h interpre.h handler.h db.h \
	spells.h
	$(CC) -c $(CFLAGS) graph.cc
config.o : config.cc structs.h
	$(CC) -c $(CFLAGS) config.cc
ranger.o : ranger.cc structs.h utils.h comm.h interpre.h handler.h db.h\
	 spells.h script.h
	 $(CC) -c $(CFLAGS) ranger.cc  
script.o : script.cc structs.h utils.h comm.h interpre.h protos.h script.h
	$(CC) -c $(CFLAGS) script.cc
shapemob.o : shapemob.cc structs.h utils.h comm.h interpre.h protos.h
	$(CC) -c $(CFLAGS) shapemob.cc
shapeobj.o : shapeobj.cc structs.h utils.h comm.h interpre.h protos.h
	$(CC) -c $(CFLAGS) shapeobj.cc
shaperom.o : shaperom.cc structs.h utils.h comm.h interpre.h protos.h
	$(CC) -c $(CFLAGS) shaperom.cc
shapezon.o : shapezon.cc structs.h utils.h comm.h interpre.h protos.h
	$(CC) -c $(CFLAGS) shapezon.cc
shapemdl.o : shapemdl.cc structs.h utils.h comm.h interpre.h protos.h
	$(CC) -c $(CFLAGS) shapemdl.cc
shapescript.o : shapescript.cc structs.h utils.h comm.h interpre.h protos.h
	$(CC) -c $(CFLAGS) shapescript.cc
mudlle.o   : mudlle.cc structs.h utils.h comm.h interpre.h protos.h mudlle.h
	$(CC) -c $(CFLAGS) mudlle.cc
mudlle2.o   : mudlle2.cc structs.h utils.h comm.h interpre.h protos.h mudlle.h
	$(CC) -c $(CFLAGS) mudlle2.cc
profs.o   : profs.cc structs.h utils.h comm.h interpre.h limits.h profs.h comm.h
	$(CC) -c $(CFLAGS) profs.cc
clerics.o : clerics.cc structs.h utils.h comm.h handler.h interpre.h db.h spells.h limits.h
	$(CC) -c $(CFLAGS) clerics.cc
mail.o    : mail.cc structs.h utils.h comm.h interpre.h db.h handler.h
	$(CC) -c $(CFLAGS) mail.cc 
zone.o: zone.cc zone.h structs.h utils.h
	$(CC) -c $(CFLAGS) zone.cc
color.o: color.cc color.h
	$(CC) -c $(CFLAGS) color.cc
obj2html.o: obj2html.cc
	$(CC) -c $(CFLAGS) obj2html.cc
pkill.o: pkill.c pkill.h
	$(CC) -c $(CFLAGS) pkill.c

/bin/ageland:
	if [ -f ../bin/ageland ]; \
	  then mv -f ../bin/ageland ../bin/ageland~; \
	fi
	$(CC) -o ../bin/ageland $(PROFILE) $(OBJFILES) $(LIBS)
