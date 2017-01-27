#include "char_utils.h"
#include "object_utils.h"
#include "environment_utils.h"

#include "handler.h" // for fname and other_side

#include "structs.h"
#include <algorithm>

struct race_bodypart_data;

//TODO(dgurley):  Move these tables elsewhere or provide accessors or something.
extern sh_int square_root[];
//extern race_bodypart_data bodyparts[MAX_BODYTYPES]; // Due to where this is located, this currently isn't possible to support here.

namespace char_utils
{
	//============================================================================
	bool is_npc(const char_data& character)
	{
		return base_utils::is_set(character.specials2.act, (long)MOB_ISNPC);
	}

	//============================================================================
	bool is_mob(const char_data& character)
	{
		return is_npc(character) && character.nr >= 0;
	}

	//============================================================================
	bool is_retired(const char_data& character)
	{
		return base_utils::is_set(character.specials2.act, (long)PLR_RETIRED);
	}

	//============================================================================
	bool is_mob_flagged(const char_data& mob, long flag)
	{
		return is_npc(mob) && base_utils::is_set(mob.specials2.act, flag);
	}

	//============================================================================
	bool is_player_flagged(const char_data& character, long flag)
	{
		return !is_npc(character) && base_utils::is_set(character.specials2.act, flag);
	}

	//============================================================================
	bool is_preference_flagged(const char_data& character, long flag)
	{
		return base_utils::is_set(character.specials2.pref, flag);
	}
	
	//============================================================================
	// Apparently this isn't used.
	//============================================================================
	bool is_player_mode_on(const char_data& character, long flag)
	{
		return false;
		// there is no character.specials2.mode
		//return base_utils::is_set(character.specials2.mode, flag);
	}

	//============================================================================
	bool is_affected_by(const char_data& character, long skill_id)
	{
		return base_utils::is_set(character.specials.affected_by, skill_id);
	}

	//============================================================================
	const char* his_or_her(const char_data& character)
	{
		switch (character.player.sex)
		{
		case 0:
			return "its";
			break;
		case 1:
			return "his";
			break;
		case 2:
			return "her";
			break;
		default:
			return "its";
			break;
		}
	}

	//============================================================================
	const char* he_or_she(const char_data& character)
	{
		switch (character.player.sex)
		{
		case 0:
			return "it";
			break;
		case 1:
			return "he";
			break;
		case 2:
			return "she";
			break;
		default:
			return "it";
			break;
		}
	}

	//============================================================================
	const char* him_or_her(const char_data& character)
	{
		switch (character.player.sex)
		{
		case 0:
			return "it";
			break;
		case 1:
			return "him";
			break;
		case 2:
			return "her";
			break;
		default:
			return "it";
			break;
		}
	}

	//============================================================================
	int get_tactics(const char_data& character)
	{
		if (is_npc(character))
			return 0;

		return character.specials.tactics;
	}

	//============================================================================
	void set_tactics(char_data& character, int value)
	{
		if (is_npc(character))
		{
			character.specials.tactics = value;
		}
	}

	//============================================================================
	int get_condition(const char_data& character, int index)
	{
		if (index > 2 || index < 0)
			return 0;

		return character.specials2.conditions[index];
	}

	//============================================================================
	void set_condition(char_data& character, int index, sh_int value)
	{
		if (index >= 0 && index <= 2)
		{
			character.specials2.conditions[index] = value;
		}
	}

	//============================================================================
	int get_index(const char_data& character)
	{
		if (is_npc(character))
			return -1;

		return character.player_index;
	}

	//============================================================================
	const char* get_name(const char_data& character)
	{
		if (is_npc(character))
			return character.player.short_descr;

		return character.player.name;
	}
	
	//============================================================================
	int get_level_a(const char_data& character)
	{
		if (is_npc(character))
			return character.player.level;

		return std::min(character.player.level, LEVEL_MAX);
	}

	//============================================================================
	int get_level_legend_cap(const char_data& character)
	{
		return get_level_a(character);
	}

	//============================================================================
	int get_level_b(const char_data& character)
	{
		if (is_npc(character))
			return character.player.level;

		return std::min(character.player.level, character.player.level / 3 + (LEVEL_MAX * 2 / 3));
	}

	//============================================================================
	int get_prof_level(int prof, const char_data& character)
	{
		if (prof == PROF_GENERAL || is_npc(character) || prof > MAX_PROFS)
			return character.player.level;

		return character.profs->prof_level[prof];
	}

	//============================================================================
	int get_max_race_prof_level(int prof, const char_data& character)
	{
		int max_prof_level = 30;
		
		if (character.player.race == RACE_ORC)
		{
			max_prof_level = 20;
		}
		else if (character.player.race == RACE_URUK && prof == PROF_MAGE)
		{
			max_prof_level = 27;
		}

		return max_prof_level;
	}

	//============================================================================
	void set_prof_level(int prof, char_data& character, sh_int value)
	{
		if (prof == PROF_GENERAL || prof > MAX_PROFS || is_npc(character))
			return;

		character.profs->prof_level[prof] = value;
	}
	
	//============================================================================
	int get_prof_coof(int prof, const char_data& character)
	{
		if (prof > MAX_PROFS)
			return 0; // should add assert functionality for this

		if (prof == PROF_GENERAL)
			return 1000;

		int prof_coof = character.profs->prof_coof[prof];
		//TODO(dgurley):  square_root is an externed array; fix this crap.
		int return_prof_coof = square_root[prof_coof];

		if (character.player.race == RACE_ORC)
		{
			return_prof_coof = (return_prof_coof * 2 + 2) / 3;
		}
		else if(character.player.race == RACE_URUK && prof == PROF_MAGE)
		{
			return_prof_coof -= 100;
		}

		return return_prof_coof;
	}

	//============================================================================
	int get_prof_points(int prof, const char_data& character)
	{
		// Added a safety check.
		if (prof > MAX_PROFS)
			return 0;

		// Add is_npc check like above?  Not in the current macro, so I won't be adding new functionality.
		return character.profs->prof_coof[prof];
	}

	//============================================================================
	int get_bal_strength(const char_data& character)
	{
		// dgurley:  I agree with the intent behind this function, but not it's implementation.
		// I don't think that there should be a racial normalization factor.  However, we could
		// treat all high strength scores with diminishing returns.  This function wouldn't be
		// the correct place to do that though - it would be in whatever function is using strength.

		int max_race_str[] = 
		{
			20,            // IMM
			20,            // HUMAN
			20,            // DWARF
			18,            // WOOD
			16,            // HOBBIT
			18,            // HIGH ELF
			20,
			20,
			20,
			20,
			20,
			20,           // URUK
			20,           // HARAD
			18,	   	// COMMON ORC
			20,	  	// EASTERLING
			18,	  	// LHUTH
			20,
			20,
			20,
			20,
			20,   // TROLL
			20
		};

		// If the character's strength is within normal bounds for their race, just return it.
		int race = character.player.race;
		if(race > 21)
			return character.tmpabilities.str;

		int race_strength_cap = max_race_str[race];
		if (character.tmpabilities.str <= race_strength_cap)
			return character.tmpabilities.str;

		// Otherwise, only even points of strength count.
		int bounded_strength = race_strength_cap + ((character.tmpabilities.str - race_strength_cap) / 2);
		return bounded_strength;
	}

	//============================================================================
	bool is_evil_race(const char_data& character)
	{
		int race = character.player.race;
		return race == RACE_URUK || race == RACE_ORC || race == RACE_MAGUS;
	}

	//============================================================================
	int get_skill_penalty(const char_data& character)
	{
		const int encumb_multiplier = 25;
		const int encumb_weight_divisor = 50;

		// TODO(dgurley):  See comment about get_bal_strength here.
		int character_strength = get_bal_strength(character);

		int raw_encumb_factor = character.points.encumb * encumb_multiplier;
		int encumb_weight_factor = character.specials.encumb_weight / character_strength / encumb_weight_divisor;
		int skill_penalty = raw_encumb_factor + encumb_weight_factor;
		return skill_penalty;
	}

	//============================================================================
	int get_dodge_penalty(const char_data& character)
	{
		const int dodge_multiplier = 20;

		// TODO(dgurley):  See comment about get_bal_strength here.
		int character_strength = get_bal_strength(character);

		int raw_encumb_factor = character.specials2.leg_encumb * dodge_multiplier;
		int worn_weight_factor = character.specials.worn_weight / character_strength / dodge_multiplier;
		int dodge_penalty = raw_encumb_factor + worn_weight_factor;
		return dodge_penalty;
	}

	//============================================================================
	int get_idnum(const char_data& character)
	{
		if (is_npc(character))
			return -1;

		return character.specials2.idnum;
	}

	//============================================================================
	bool is_awake(const char_data& character)
	{
		return character.specials.position > POSITION_SLEEPING;
	}

	//============================================================================
	// Functions in this namespace do not belong in this file and need to be moved
	// elsewhere.
	//============================================================================
	namespace TEMPORARY
	{

#include "spells.h"

		int get_confuse_modifier(const char_data& character)
		{
			int modifier = 0;
			const affected_type* status_effect = character.affected;

			// Iterate through the character affects, look for confuse, and
			// use its duration to determine its strength.
			while (modifier == 0 && status_effect)
			{
				if (status_effect->type == SPELL_CONFUSE)
				{
					modifier = (status_effect->duration * 2) - 10;
				}
				
				status_effect = status_effect->next;
			}

			// dgurley:  Clamp the value of the modifier to 0.  This is a 
			// functionality change.  Feel free to remove this.
			modifier = std::max(modifier, 0);
			return modifier;
		}
	}

	//============================================================================
	int get_raw_skill(const char_data& character, int skill_index)
	{
		if (character.player.bodytype != 2)
			return 0;

		// TODO(dgurley):  This is the GET_RAW_SKILL macro as written.
		// Ensure it is correct.
		return get_knowledge(character, skill_index);
	}

	//============================================================================
	int get_skill(const char_data& character, int skill_index)
	{
		int raw_skill = get_raw_skill(character, skill_index);
		
		if (is_affected_by(character, AFF_CONFUSE))
		{
			raw_skill -= TEMPORARY::get_confuse_modifier(character);
		}

		return raw_skill;
	}

	//============================================================================
	void set_skill(char_data& character, int skill_index, byte value)
	{
		if (character.skills)
		{
			character.skills[skill_index] = value;
		}
	}

	//============================================================================
	int get_raw_knowledge(const char_data& character, int skill_index)
	{
		if (!character.knowledge)
			return 80;

		return character.knowledge[skill_index];
	}

	//============================================================================
	int get_knowledge(const char_data& character, int skill_index)
	{
		int raw_knowledge = get_raw_knowledge(character, skill_index);
		
		if (is_affected_by(character, AFF_CONFUSE))
		{
			raw_knowledge -= TEMPORARY::get_confuse_modifier(character);
		}

		return raw_knowledge;
	}
	
	//============================================================================
	void set_knowledge(char_data& character, int skill_index, byte value)
	{
		if (character.knowledge)
		{
			character.knowledge[skill_index] = value;
		}
	}

	//============================================================================
	bool get_carry_weight_limit(const char_data& character)
	{
		const int base_carry_weight = 2000;
		const int strength_multiplier = 1000;

		return base_carry_weight + strength_multiplier * character.tmpabilities.str;
	}
	
	//============================================================================
	bool get_carry_item_limit(const char_data& character)
	{
		const int base_items = 5;

		return base_items + (character.tmpabilities.dex / 2) + (character.player.level / 2);
	}

	//============================================================================
	bool is_twohanded(const char_data& character)
	{
		return base_utils::is_set(character.specials.affected_by, (long)AFF_TWOHANDED);
	}

	//============================================================================
	bool can_carry_object(const char_data& character, const obj_data& object)
	{
		bool can_lift_object = character.specials.carry_weight + object.obj_flags.weight < get_carry_weight_limit(character);
		bool has_hands_free = character.specials.carry_items + 1 < get_carry_item_limit(character);
		
		return can_lift_object && has_hands_free;
	}
	
	//============================================================================
	bool can_see(const char_data& character, const weather_data& weather, const room_data& room)
	{
		if (character.in_room == NOWHERE)
			return false;

		if (is_affected_by(character, AFF_BLIND))
			return false;

		if(is_player_flagged(character, PLR_WRITING))
			return false;

		if (is_shadow(character))
			return true;

		// need the current room for the character as well.
		if (environment_utils::is_light(room, weather))
			return true;

		if (is_affected_by(character, AFF_INFRARED) || is_preference_flagged(character, PRF_HOLYLIGHT))
			return true;

		if (weather.moonlight > 0 && is_affected_by(character, AFF_MOONVISION) && environment_utils::is_room_outside(room))
			return true;
		
		return true;
	}

	//============================================================================
	bool can_see_object(const char_data& character, const obj_data& object, const weather_data& weather, const room_data& room)
	{
		int item_flags = object.obj_flags.extra_flags;

		if (is_shadow(character))
		{
			return base_utils::is_set(item_flags, ITEM_MAGIC) 
				|| base_utils::is_set(item_flags, ITEM_WILLPOWER);
		}
		else
		{
			return can_see(character, weather, room)
				&& (!base_utils::is_set(item_flags, ITEM_INVISIBLE)
				|| is_affected_by(character, AFF_DETECT_INVISIBLE));

		}
	}

	//============================================================================
	bool can_get_object(const char_data& character, const obj_data& object, const weather_data& weather, const room_data& room)
	{
		return object_utils::can_wear(object, ITEM_TAKE)
			&& can_carry_object(character, object)
			&& can_see_object(character, object, weather, room);
	}

	//============================================================================
	bool is_shadow(const char_data& character)
	{
		if (is_npc(character))
		{
			return base_utils::is_set(character.specials2.act, (long)MOB_SHADOW);
		}
		else
		{
			return base_utils::is_set(character.specials2.act, (long)PLR_ISSHADOW);
		}
	}

	//============================================================================
	bool is_race_good(const char_data& character)
	{
		int race = character.player.race;
		return race > 0 && race < 10;
	}

	//============================================================================
	bool is_race_evil(const char_data& character)
	{
		return character.player.race > 10;
	}

	//============================================================================
	bool is_race_easterling(const char_data& character)
	{
		return character.player.race == 14;
	}

	//============================================================================
	bool is_race_magi(const char_data& character)
	{
		return character.player.race == 15;
	}

	//============================================================================
	const char* get_object_string(const char_data& character, const obj_data& object, 
		const weather_data& weather, const room_data& room)
	{
		if (can_see_object(character, object, weather, room))
		{
			return object.short_description;
		}
		else
		{
			return "something";
		}
	}

	//============================================================================
	const char* get_object_name(const char_data& character, const obj_data& object, 
		const weather_data& weather, const room_data& room)
	{
		if (can_see_object(character, object, weather, room))
		{
			//dgurley: fname isn't thread safe
			return fname(object.name);
		}
		else
		{
			return "something";
		}
	}

	//============================================================================
	bool is_good(const char_data& character)
	{
		return character.specials2.alignment >= 100;
	}
	
	//============================================================================
	bool is_evil(const char_data& character)
	{
		return character.specials2.alignment <= -100;
	}
	
	//============================================================================
	bool is_neutral(const char_data& character)
	{
		return !is_good(character) && !is_evil(character);
	}

	//============================================================================
	bool is_hostile_to(const char_data& character, const char_data& victim)
	{
		if (!is_npc(character))
			return false;

		// An NPC from the other side of the race war than the victim will be hostile.
		if (character.player.race != 0 && !is_npc(victim) && other_side(&character, &victim))
			return true;

		// An NPC is hostile if it has a preference for the victim's race set.
		if (is_preference_flagged(character, (long)1 << victim.player.race))
			return true;

		return false;
	}

	//============================================================================
	bool is_rp_race_check(const char_data& character, const char_data& victim)
	{
		return is_npc(character) && character.specials2.rp_flag != 0 ||
			base_utils::is_set(character.specials2.rp_flag, 1 << victim.player.race);
	}

	//============================================================================
	bool is_riding(const char_data& character)
	{
		return character.mount_data.mount && char_exists(character.mount_data.mount_number);
	}
	
	//============================================================================
	bool is_ridden(const char_data& character)
	{
		return character.mount_data.rider && char_exists(character.mount_data.rider_number);
	}

	//============================================================================
	const char* get_prof_abbrev(const char_data& character)
	{
		if (is_npc(character))
			return "--";

		static const char* prof_abbrevs[] =
		{
			"--", "Mu", "Cl", "Ra", "Wa"
		};

		int prof = character.player.prof;
		return prof_abbrevs[prof];
	}
	
	//============================================================================
	const char* get_race_abbrev(const char_data& character)
	{
		if (is_npc(character))
			return "--";

		// dgurley:  Unsure the extra padding is necessary.
		// Just copying the table from consts.
		const char* race_abbrevs[] =
		{
			"Imm",
			"Hum",
			"Dwf",
			"WdE",
			"Hob",
			"HiE",
			"Beo",
			"??",
			"??",
			"??",
			"??",
			"Urk",
			"Har",
			"Orc",
			"Eas",
			"Lhu",
			"??",
			"??",
			"??",
			"??",
			"Trl",
			"??"
		};

		int race = character.player.race;
		return race_abbrevs[race];
	}

	//============================================================================
	int get_race_perception(const char_data& character)
	{
		int race = character.player.race;

		switch (race) 
		{
		case RACE_GOD: return 0;
		case RACE_HUMAN: return 30;
		case RACE_DWARF: return 0;
		case RACE_WOOD:  return 50;
		case RACE_HOBBIT: return 30;
		case RACE_HIGH: return 100;
		//case RACE_BEORN: return 30;
		case RACE_URUK: return 30;
		case RACE_HARAD: return 30;
		case RACE_ORC: return 10;
		case RACE_EASTERLING: return 30;
		case RACE_MAGUS: return 30;
		case RACE_UNDEAD: return 60;
		case RACE_TROLL: return 30;
		default: return 0;
		}

		return 0;
	}

	//============================================================================
	int get_perception(const char_data& character)
	{
		if (is_shadow(character))
			return 100;

		int perception = character.specials2.perception;
		if (character.specials2.perception == -1)
			return get_race_perception(character);

		// perception is clamped between 0 and 100.
		return std::min(std::max(perception, 0), 100);
	}

	//============================================================================
	bool is_mental(const char_data& character)
	{
		return is_shadow(character) || (!is_npc(character) && is_preference_flagged(character, PRF_MENTAL));
	}

	//============================================================================
	int get_specialization(const char_data& character)
	{
		if (is_npc(character) || character.profs == NULL)
			return 0;

		return character.profs->specialization;
	}
	
	//============================================================================
	void set_specialization(char_data& character, int value)
	{
		if (is_npc(character) || character.profs == NULL)
			return;

		character.profs->specialization = value;
	}

	//============================================================================
	bool is_resistant(const char_data& character, int attack_group)
	{
		return (character.specials.resistance & (1 << attack_group)) != 0;
	}
	
	//============================================================================
	bool is_vulnerable(const char_data& character, int attack_group)
	{
		return (character.specials.vulnerability & (1 << attack_group)) != 0;
	}

	//============================================================================
	bool is_guardian(const char_data& character)
	{
		return is_npc(character) && is_affected_by(character, AFF_CHARM)
			&& character.master != NULL && is_mob_flagged(character, MOB_BODYGUARD);
	}
}

