#include "object_utils.h"
#include "structs.h"

namespace object_utils
{
	//============================================================================
	bool is_artifact(const obj_data& object)
	{
		//dgurley:  This macro always returns false.
		return false;
	}
	
	//============================================================================
	int get_item_type(const obj_data& object)
	{
		return object.obj_flags.type_flag;
	}
	
	//============================================================================
	bool can_wear(const obj_data& object, int body_part)
	{
		return base_utils::is_set(object.obj_flags.wear_flags, body_part);
	}

	//============================================================================
	int get_object_weight(const obj_data& object)
	{
		return object.obj_flags.weight;
	}

	//============================================================================
	bool is_object_stat(const obj_data& object, int stat)
	{
		return base_utils::is_set(object.obj_flags.extra_flags, stat);
	}

	//============================================================================
	int get_item_bulk(const obj_data& object)
	{
		return object.obj_flags.value[2];
	}
}

