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
		bool on_loot_item(char_data* looter, obj_data* corpse, obj_data* item);

		// Called before any character attempts to damage or attack another character.
		// This enforces our PK engagement rules.
		bool is_target_valid(char_data* attacker, const char_data* victim, int skill_id) const;

		// Redirects in an attempt to find a suitable target for the attacker in the case
		// that their original target was not valid.  If no suitable target is found, NULL
		// is returned.
		char_data* get_valid_target(char_data* attacker, const char_data* victim, const char* argument) const;

		// Called when a character dies to create information about loot rules.
		void on_character_died(char_data* character, char_data* killer, obj_data* corpse);

		// Called when a character auto-AFKs so that they can [potentially] be protected.
		void on_character_afked(const char_data* character);

		// Called when a character successfully attacks another player.  This is used to
		// track whether or not a character should get AFK protection.
		void on_character_attacked_player(const char_data* attacker, const char_data* victim);

		// When a character disconnects, let the Big Brother system know so that it can
		// clean up any references.
		void on_character_disconnected(const char_data* character);

		// When a corpse decays, let the Big Brother system know so that it will no longer
		// track it.
		void on_corpse_decayed(obj_data* corpse);

		// Alert the Big Brother system that a character has returned to their keyboard.
		void on_character_returned(const char_data* character);
	

	private:

		friend class world_singleton<big_brother>;

		// Private constructor that we friend with our parent to grant access.
		big_brother(const weather_data* weather, const room_data* world)
			: world_singleton<big_brother>(weather, world) { }

		// Is the spell being cast or skill being used offensive in nature.
		bool is_skill_offensive(int skill_id) const;

		// Returns true if the victim has the 'looting' flag.
		bool is_target_looting(const char_data* victim) const;

		// Returns true if the victim is within an appropriate level of the attacker.
		bool is_level_range_appropriate(const char_data* attacker, const char_data* victim) const;

		// Returns true if the victim has been auto-afk'd.
		bool is_target_afk(const char_data* victim) const;

		// Returns true if two targets are on the same side of the race war.
		bool is_same_side_race_war(int attacker_race, int victim_race) const;

		// Removes a character from our afk_characters set.
		void remove_character_from_afk_set(const char_data* character);

		// Removes a character from our looting characters set.
		void remove_character_from_looting_set(int char_id);

		struct player_corpse_data
		{
			player_corpse_data() : num_items_looted(0), player_race(0), killer_id(-1), player_id(0) { };
			player_corpse_data(char_data* dead_man);
			player_corpse_data(char_data* dead_man, char_data* killer);

			int num_items_looted;
			int player_race;
			int killer_id;
			int player_id;
		};

		typedef std::map<obj_data*, player_corpse_data> corpse_map;
		corpse_map m_corpse_map;

		typedef std::set<const char_data*> character_set;
		character_set m_afk_characters;

		typedef std::set<int> character_id_set;
		character_id_set m_looting_characters;
		
		// For tracking when people engaged in PK can get AFK protection.
		typedef std::map<const char_data*, tm> time_map;
		time_map m_last_engaged_pk_time;
	};
}

