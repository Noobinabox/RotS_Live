#pragma once

#include "base_utils.h"
#include "singleton.h"

#include <map>
#include <set>
#include <time.h>

struct obj_data;
struct char_data;

namespace game_rules
{
	class big_brother : public world_singleton<big_brother>
	{
	public:

		// Called before any character loots an item.  This enforces our PK loot rules.
		bool on_loot_item(char_data* looter, obj_data* item);

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
		void on_character_afked(const char_data* character);

		// Called when a character successfully attacks another player.  This is used to
		// track whether or not a character should get AFK protection.
		void on_character_attacked_player(const char_data* attacker);

		// When a character disconnects, let the Big Brother system know so that it can
		// clean up any references.
		void on_character_disconnected(const char_data* character);

		// When a corpse decays, let the Big Brother system know so that it will no longer
		// track it.
		void on_corpse_decayed(obj_data* corpse);

		// Alert the Big Brother system that a character has returned to their keyboard.
		void on_character_returned(const char_data* character);

	private:

		// Is the spell being cast or skill being used offensive in nature.
		bool is_skill_offensive(int skill_id);

		// Returns true if the victim has the 'looting' flag.
		bool is_target_looting(const char_data* victim);

		// Returns true if the victim is within an appropriate level of the attacker.
		bool is_level_range_appropriate(const char_data* attacker, const char_data* victim);

		// Returns true if the victim has been auto-afk'd.
		bool is_target_afk(const char_data* victim);

		// Removes a character from our afk_characters set.
		void remove_character_from_set(const char_data* character);

		// TODO(drelidan):
		// There will be a map between obj_data* corpses and a struct containing information
		// about them - who they belong to, who has looting rights, etc.
		std::map<obj_data*, char_data*> m_corpse_map;

		std::set<const char_data*> m_afk_characters;
		
		// For tracking when people engaged in PK can get AFK protection.
		std::map<const char_data*, tm> m_last_engaged_pk_time;
	};
}

