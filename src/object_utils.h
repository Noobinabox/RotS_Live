#pragma once

#include "base_utils.h"

struct obj_data;

namespace utils {
bool is_artifact(const obj_data& object);
int get_item_type(const obj_data& object);
bool can_wear(const obj_data& object, int body_part);
int get_object_weight(const obj_data& object);

bool is_object_stat(const obj_data& object, int stat);
int get_item_bulk(const obj_data& object);

double get_weapon_damage(const obj_data& weapon);
}
