#pragma once

#include "base_utils.h"
#include "singleton.h"

#include <map>
#include <set>

struct obj_data;
struct char_data;

namespace game_rules
{
	class big_brother : public world_singleton<big_brother>
	{
	public:

		// Called before any character loots a corpse.  This enforces our PK loot rules.
		bool on_loot_item(char_data* looter, obj_data* corpse);

		// Called before any character attempts to damage or attack another character.
		// This enforces our PK engagement rules.
		bool is_target_valid(char_data* attacker, const char_data* victim);

		// Redirects in an attempt to find a suitable target for the attacker in the case
		// that their original target was not valid.  If no suitable target is found, NULL
		// is returned.
		char_data* get_valid_target(char_data* attacker, const char_data* victim, const char* argument);

		// Called when a character dies to create information about loot rules.
		void on_character_died(char_data* character, obj_data* corpse);

		// Called when a character auto-AFKs so that they can [potentially] be protected.
		void on_character_afked(char_data* character);

		// Called when a character successfully attacks another player.  This is used to
		// track whether or not a character should get AFK protection.
		void on_character_attacked_player(char_data* attacker);

	private:

		// Is the spell being cast or skill being used offensive in nature.
		bool is_skill_offensive(int skill_id);

		// Returns true if the victim has the 'looting' flag.
		bool is_target_looting(const char_data* victim);

		// Returns true if the victim is within an appropriate level of the attacker.
		bool is_level_range_appropriate(const char_data* attacker, const char_data* victim);

		// Returns true if the victim has been auto-afk'd.
		bool is_target_afk(const char_data* victim);

		// TODO(drelidan):
		// There will be a map between obj_data* corpses and a struct containing information
		// about them - who they belong to, who has looting rights, etc.

		// There will be a set of characters that have been auto-afk'd.

	};
}

