#include "../structs.h"
#include "builders/ObjFlagDataBuilder.h"
#include <gtest/gtest.h>

TEST(ObjectFlagData, get_ob_coef) {
    builders::ObjFlagDataBuilder builder;
    builder.setObCoef(10);

    obj_flag_data objFlagData = builder.build();

    EXPECT_EQ(objFlagData.get_ob_coef(), 10);
}

TEST(ObjectFlagData, is_chain) {
    obj_flag_data objFlagData{};
    objFlagData.material = 3;
    EXPECT_EQ(objFlagData.is_chain(), true);
}

TEST(ObjectFlagData, is_metal) {
    obj_flag_data objFlagData{};
    objFlagData.material = 4;
    EXPECT_EQ(objFlagData.is_metal(), true);
}

TEST(ObjectFlagData, WeaponType) {
    builders::ObjFlagDataBuilder builder;
    builder.setWeaponType(game_types::weapon_type::WT_SLASHING);
    obj_flag_data objFlagData = builder.build();

    EXPECT_EQ(objFlagData.get_weapon_type(), game_types::weapon_type::WT_SLASHING);
}