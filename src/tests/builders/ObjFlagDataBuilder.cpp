#include "ObjFlagDataBuilder.h"

namespace builders {
    ObjFlagDataBuilder &ObjFlagDataBuilder::setObCoef(int value) {
        data.value[0] = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setParryCoef(int value) {
        data.value[1] = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setBulk(int value) {
        data.value[2] = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setLevel(unsigned char value) {
        data.level = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setWeight(int value) {
        data.weight = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setWeaponType(game_types::weapon_type value) {
        data.value[3] = static_cast<int>(value);
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setMaterial(signed char value) {
        data.material = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setWearFlags(int value) {
        data.wear_flags = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setExtraFlags(int value) {
        data.extra_flags = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setCost(int value) {
        data.cost = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setCostPerDay(short value) {
        data.cost_per_day = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setTimer(int value) {
        data.timer = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setBitVector(int value) {
        data.bitvector = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setRarity(unsigned char value) {
        data.rarity = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setButcherItem(short value) {
        data.butcher_item = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setProgramNumber(int value) {
        data.prog_number = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setScriptNumber(int value) {
        data.script_number = value;
        return *this;
    }

    ObjFlagDataBuilder &ObjFlagDataBuilder::setScriptInfo(info_script *script_info) {
        data.script_info = script_info;
        return *this;
    }
}