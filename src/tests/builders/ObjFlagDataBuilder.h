#ifndef OBJ_FLAG_DATA_BUILDER_H_
#define OBJ_FLAG_DATA_BUILDER_H_

#include "../../structs.h"


namespace builders {

    class ObjFlagDataBuilder {

    private:
        obj_flag_data data;

    public:
        ObjFlagDataBuilder() {
        }

        ObjFlagDataBuilder &setObCoef(int value);

        ObjFlagDataBuilder &setParryCoef(int value);

        ObjFlagDataBuilder &setBulk(int value);

        ObjFlagDataBuilder &setLevel(unsigned char value);

        ObjFlagDataBuilder &setWeight(int value);

        ObjFlagDataBuilder &setWeaponType(game_types::weapon_type value);

        ObjFlagDataBuilder &setMaterial(signed char value);

        ObjFlagDataBuilder &setWearFlags(int value);

        ObjFlagDataBuilder &setExtraFlags(int value);

        ObjFlagDataBuilder &setCost(int value);

        ObjFlagDataBuilder &setCostPerDay(signed short int value);

        ObjFlagDataBuilder &setTimer(int value);

        ObjFlagDataBuilder &setBitVector(int value);

        ObjFlagDataBuilder &setRarity(unsigned char value);

        ObjFlagDataBuilder &setButcherItem(signed short int value);

        ObjFlagDataBuilder &setProgramNumber(int value);

        ObjFlagDataBuilder &setScriptNumber(int value);

        ObjFlagDataBuilder &setScriptInfo(info_script *script_info);

        obj_flag_data build() const {
            return data;
        }
    };
}

#endif //OBJ_FLAG_DATA_BUILDER_H_