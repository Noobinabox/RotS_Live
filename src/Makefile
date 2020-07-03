#compiler name
CC = g++ -m32 -w

#compiler flags you want to use (other than profiling, such as -Wall)
MYFLAGS = -fstrict-aliasing -funsigned-char

#flags for profiling (see hacker.doc for more information)
PROFILE = -g -rdynamic

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
	act_othe.o act_soci.o act_wiz.o ban.o big_brother.o boards.o char_utils.o char_utils_combat.o clerics.o clock.o color.o combat_manager.o \
	comm.o config.o consts.o db.o delayed_command_interpreter.o fight.o graph.o handler.o interpre.o environment_utils.o \
	limits.o mail.o mystic.o mage.o mobact.o modify.o mudlle.o mudlle2.o obj2html.o object_utils.o objsave.o olog_hai.o\
	pkill.o profs.o ranger.o script.o shapemdl.o shapemob.o shapeobj.o shaperom.o shapezon.o shapescript.o shop.o \
	signals.o skill_timer.o spec_ass.o spec_pro.o spell_pa.o utility.o wait_functions.o weather.o zone.o


ageland:        ../bin/ageland

../bin/ageland:	$(OBJFILES)

all:	ageland

clean:
	rm -f *.o

# Dependencies for the main mud

clock.o : clock.cpp clock.h
	$(CC) -c $(CFLAGS) clock.cpp

comm.o : comm.cc structs.h utils.h comm.h interpre.h handler.h db.h \
	limits.h clock.h
	$(CC) -c $(CFLAGS) $(COMMFLAGS) comm.cc
char_utils.o : char_utils.cpp base_utils.h char_utils.h object_utils.h \
	environment_utils.h structs.h handler.h
	$(CC) -c $(CFLAGS) char_utils.cpp
environment_utils.o : environment_utils.cpp structs.h environment_utils.h
	$(CC) -c $(CFLAGS) environment_utils.cpp
object_utils.o : object_utils.cpp structs.h object_utils.h
	$(CC) -c $(CFLAGS) object_utils.cpp
char_utils_combat.o : char_utils_combat.cpp structs.h base_utils.h char_utils.h object_utils.h  comm.h char_utils_combat.h
	$(CC) -c $(CFLAGS) char_utils_combat.cpp
combat_manager.o : combat_manager.cpp combat_manager.h base_utils.h
	$(CC) -c $(CFLAGS) combat_manager.cpp
big_brother.o : big_brother.cpp big_brother.h base_utils.h singleton.h
	$(CC) -c $(CFLAGS) big_brother.cpp
skill_timer.o : skill_timer.cpp skill_timer.h singleton.h
	$(CC) -c $(CFLAGS) skill_timer.cpp
olog_hai.o : olog_hai.cpp structs.h utils.h comm.h interpre.h handler.h \
	db.h spells.h limits.h 
	$(CC) -c $(CFLAGS) olog_hai.cpp
delayed_command_interpreter.o : delayed_command_interpreter.cpp delayed_command_interpreter.h
	$(CC) -c $(CFLAGS) delayed_command_interpreter.cpp
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
        interpre.h big_brother.h skill_timer.h
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

.PHONY: format
format:
	clang-format -i -style=WebKit *.cpp
	clang-format -i -style=WebKit *.cc
	clang-format -i -style=WebKit *.c
	clang-format -i -style=WebKit *.h

.PHONY: setup
setup:
	@mkdir -p ../bin ../log ../lib/misc \
		../lib/players/A-E \
		../lib/players/F-J \
		../lib/players/K-O \
		../lib/players/P-T \
		../lib/players/U-Z \
		../lib/players/ZZZ \
		../lib/plrobjs/A-E \
		../lib/plrobjs/F-J \
		../lib/plrobjs/K-O \
		../lib/plrobjs/P-T \
		../lib/plrobjs/U-Z \
		../lib/plrobjs/ZZZ \
		../lib/exploits/A-E \
		../lib/exploits/F-J \
		../lib/exploits/K-O \
		../lib/exploits/P-T \
		../lib/exploits/U-Z \
		../lib/exploits/ZZZ
	@touch \
		../log/lastdeath \
		../lib/last_cmds \
		../lib/misc/pklist

.PHONY: run
run: ../bin/ageland setup
	@cd .. && ./bin/ageland

../bin/ageland:
	if [ -f ../bin/ageland ]; \
	  then mv -f ../bin/ageland ../bin/ageland~; \
	fi
	$(CC) -o ../bin/ageland $(PROFILE) $(OBJFILES) $(LIBS)
