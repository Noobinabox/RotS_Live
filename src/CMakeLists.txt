#usage : cmake -S . -B /../build
# this will create a build solution in the build directory
# .gitignore exludes everything under /build.

cmake_minimum_required(VERSION 3.10)
project(RotS)

# There's totally a better way to do this than simply listing every file
add_executable(ageland
act_comm.cpp
act_info.cpp
act_move.cpp
act_obj1.cpp
act_obj2.cpp
act_offe.cpp
act_othe.cpp
act_soci.cpp
act_wiz.cpp
ban.cpp
base_utils.h
battle_mage_handler.cpp
big_brother.cpp
big_brother.h
boards.cpp
boards.h
color.cpp
color.h
combat_manager.cpp
combat_manager.h
comm.cpp
comm.h
config.cpp
consts.cpp
db.cpp
db.h
delayed_command_interpreter.cpp
delayed_command_interpreter.h
environment_utils.cpp
environment_utils.h
fight.cpp

graph.cpp
handler.cpp
handler.h
interpre.cpp
interpre.h
limits.cpp
limits.h
mage.cpp
mail.cpp
mail.h

mobact.cpp
modify.cpp
mudlle.cpp
mudlle.h
mudlle2.cpp
mystic.cpp
obj2html.cpp
object_utils.cpp
object_utils.h
objsave.cpp
olog_hai.cpp
pkill.c
pkill.h
platdef.h
profs.cpp
profs.h
protos.h
ranger.cpp
script.cpp
script.h
shapemdl.cpp
shapemob.cpp
shapeobj.cpp
shaperom.cpp
shapescript.cpp
shapezon.cpp
shop.cpp
signals.cpp
singleton.h
spec_ass.cpp
spec_pro.cpp
spells.h
spell_pa.cpp
structs.h

utility.cpp
utils.h
wait_functions.cpp
wait_functions.h
weather.cpp
zone.cpp
zone.h


char_utils.cpp
char_utils.h
char_utils_combat.cpp
char_utils_combat.h
clerics.cpp
clock.cpp
clock.h

skill_timer.h
skill_timer.cpp

warrior_spec_handlers.h
wild_fighting_handler.cpp
weapon_master_handler.cpp

)


set_target_properties(ageland PROPERTIES CXX_STANDARD 17)
