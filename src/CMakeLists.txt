#usage : cmake -S . -B /../build
# this will create a build solution in the build directory
# .gitignore exludes everything under /build.

cmake_minimum_required(VERSION 3.2)
project(RotS)

# There's totally a better way to do this than simply listing every file
add_executable(ageland
act_comm.cc
act_info.cc
act_move.cc
act_obj1.cc
act_obj2.cc
act_offe.cc
act_othe.cc
act_soci.cc
act_wiz.cc
ban.cc
base_utils.h
big_brother.cpp
big_brother.h
boards.cc
boards.h
color.cc
color.h
combat_manager.cpp
combat_manager.h
comm.cc
comm.h
config.cc
consts.cc
db.cc
db.h
delayed_command_interpreter.cpp
delayed_command_interpreter.h
environment_utils.cpp
environment_utils.h
fight.cc

graph.cc
handler.cc
handler.h
interpre.cc
interpre.h
limits.cc
limits.h
mage.cc
magic.cpp
magic.h
mail.cc
mail.h

mobact.cc
modify.cc
mudlle.cc
mudlle.h
mudlle2.cc
mystic.cc
obj2html.cc
object_utils.cpp
object_utils.h
objsave.cc
pkill.c
pkill.h
platdef.h
profs.cc
profs.h
protos.h
ranger.cc
script.cc
script.h
shapemdl.cc
shapemob.cc
shapeobj.cc
shaperom.cc
shapescript.cc
shapezon.cc
shop.cc
signals.cc
singleton.h
spec_ass.cc
spec_pro.cc
spells.h
spell_pa.cc
structs.h

utility.cc
utils.h
wait_functions.cpp
wait_functions.h
weather.cc
zone.cc
zone.h


char_utils.cpp
char_utils.h
char_utils_combat.cpp
char_utils_combat.h
clerics.cc
clock.cpp
clock.h


)