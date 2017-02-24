#include "big_brother.h"

#include "object_utils.h"
#include "char_utils.h"
#include "structs.h"

/********************************************************************
* Singleton Implementation
*********************************************************************/
game_rules::big_brother* game_rules::big_brother::m_pInstance(NULL);
bool game_rules::big_brother::m_bDestroyed(false);

namespace game_rules
{
	//============================================================================
	// Query Code:
	//   This code is responsible for answering yes/no to questions.
	//============================================================================ 
	
	//============================================================================
	// Returns true if the item can be looted from the corpse.
	//============================================================================
	bool big_brother::on_loot_item(char_data* looter, obj_data* corpse, obj_data* item)
	{
		// Something's not right.  Go for it, we won't stop you.
		if (looter == NULL || corpse == NULL)
			return true;

		typedef corpse_map::iterator iter;
		iter corpse_iter = m_corpse_map.find(corpse);

		// If we aren't tracking this corpse - you can loot from it.
		if (corpse_iter == m_corpse_map.end())
			return true;

		player_corpse_data& corpse_data = corpse_iter->second;
		if (is_same_side_race_war(looter->player.race, corpse_data.player_race))
			return true;

		if (corpse_data.num_items_looted >= 2)
			return false;

		// Containers can't be looted.
		// TODO(drelidan):  Add 'abuse' protection by players that unequip a bunch of
		// gear and stuff it in containers.
		if (item->obj_flags.type_flag == ITEM_CONTAINER)
			return false;

		// The corpse has had less than 2-items looted from it.  Return that the
		// item can be looted and increment the counter.
		++corpse_data.num_items_looted;
		return true;
	}

	//============================================================================
	bool big_brother::is_target_valid(char_data* attacker, const char_data* victim, int skill_id) const
	{
		// Not Big Brother's job to check these pointers.
		if (!attacker || !victim)
			return true;

		// Big Brother does not protect against NPCs.
		if (utils::is_npc(*attacker))
		{
			return true;
		}

		// The ability being used isn't offensive - all good.
		if (!is_skill_offensive(skill_id))
			return true;

		// You cannot attack a character that is writing.
		if (utils::is_player_flagged(*victim, PLR_WRITING))
			return false;

		// Big Brother protects PCs and their mounts.
		if (utils::is_npc(*victim))
		{
			// The victim is a mount.  Test if his rider is a valid target.
			if (utils::is_ridden(*victim))
			{
				return is_target_valid(attacker, victim->mount_data.rider, skill_id);
			}
			else
			{
				return true;
			}
		}

		// Can't attack AFK targets.
		if (is_target_afk(victim))
			return false;

		// Can't attack looting targets.
		if (is_target_looting(victim))
			return false;

		// Can't kill lowbies.
		if (!is_level_range_appropriate(attacker, victim))
			return false;

		return true;
	}

	//============================================================================
	bool big_brother::is_skill_offensive(int skill_id) const
	{
		// TODO(drelidan):  Add spells to the list.
		static int OFFENSIVE_SKILLS[128] = { 0 };

		for (int i = 0; i < 128; ++i)
		{
			if (OFFENSIVE_SKILLS[i] == skill_id)
			{
				return true;
			}
		}

		return false;
	}

	//============================================================================
	bool big_brother::is_target_afk(const char_data* victim) const
	{
		return m_afk_characters.find(victim) != m_afk_characters.end();
	}

	//============================================================================
	bool big_brother::is_target_looting(const char_data* victim) const
	{
		return m_looting_characters.find(victim) != m_looting_characters.end();
	}

	//============================================================================
	bool big_brother::is_level_range_appropriate(const char_data* attacker, const char_data* victim) const
	{
		int attacker_level = utils::get_level_legend_cap(*attacker);
		int defender_level = utils::get_level_legend_cap(*victim);

		// Attacker is 50% higher than defender; no go.
		if (attacker_level > (defender_level * 3 / 2))
		{
			return false;
		}

		// Defender is 50% higher than attacker; no go.  This is to prevent abuse.
		if (defender_level > (attacker_level * 3 / 2))
		{
			return false;
		}

		return true;
	}

	//============================================================================
	bool big_brother::is_same_side_race_war(int attacker_race, int victim_race) const
	{
		// All players are on the same side as Gods.
		if (attacker_race == RACE_GOD || victim_race == RACE_GOD)
			return true;

		// Good guys are on the same side.
		if (utils::is_race_good(attacker_race))
		{
			return utils::is_race_good(victim_race);
		}

		// Lhuths and easterlings are on the same side.  Note, this must be checked before
		// is_race_evil, since lhuths and easterlings are also considered evil races.
		if (utils::is_race_easterling(attacker_race) || utils::is_race_magi(attacker_race))
		{
			return utils::is_race_easterling(victim_race) || utils::is_race_magi(victim_race);
		}

		// Evil races are only on the side of other evil races that are not lhuths or easterlings.
		if (utils::is_race_evil(attacker_race))
		{
			if (utils::is_race_evil(victim_race))
			{
				return !(utils::is_race_easterling(victim_race) || utils::is_race_magi(victim_race));
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	//============================================================================
	// Data Structure Management Code:
	//   This code is responsible for ensuring that our data structures stay up-to-date.
	//============================================================================ 
	
	//============================================================================
	void big_brother::on_character_died(char_data* character, obj_data* corpse)
	{
		// Spirits don't leave corpses behind.  If we have 'shadow' mode for players again,
		// this could create some issues.
		if (corpse == NULL)
			return;

		// Big Brother doesn't track NPC corpses.
		// TODO(drelidan):  Add protection for orc followers.
		if (utils::is_npc(*character))
			return;

		// Each corpse will be unique, so even if a player dies multiple times,
		// each corpse will be protected.
		m_corpse_map[corpse] = player_corpse_data(character);
	}

	//============================================================================
	void big_brother::on_character_attacked_player(const char_data* attacker)
	{
		// TODO(drelidan):  Track the amount of items the attacker started with equipped.  If this is
		// a significant difference at death, put a flag in so that the looter can still get their
		// appropriate rewards.

		// Get the current time.
		time_t current_time;
		time(&current_time);

		tm* time_info = localtime(&current_time);
		m_last_engaged_pk_time[attacker] = *time_info; // copy tm_struct into map

		typedef std::set<const char_data*>::iterator set_iter;

		// If you attack someone in PK, you are no longer considered looting.
		set_iter attacker_iter = m_looting_characters.find(attacker);
		if (attacker_iter != m_looting_characters.end())
		{
			m_looting_characters.erase(attacker_iter);
		}
	}

	//============================================================================
	void big_brother::on_character_afked(const char_data* character)
	{
		// This is one possible implementation.  Another is to do the time-check in the
		// "is-character-afk" test.
		const double SECONDS_PER_MIN = 60;
		const double CUTOFF_SECS = 900;
		typedef std::map<const char_data*, tm>::iterator map_iter;

		bool insert = true;

		map_iter attack_time = m_last_engaged_pk_time.find(character);
		if (attack_time != m_last_engaged_pk_time.end())
		{
			time_t current_time;
			time(&current_time);

			time_t pk_time = mktime(&attack_time->second);

			// Only give AFK protection if a character hasn't PK'd for 15 minutes.
			double seconds_since = difftime(current_time, pk_time);
			insert = seconds_since >= CUTOFF_SECS;
			if (insert)
			{
				// They're clear from PK - don't track the state anymore.
				m_last_engaged_pk_time.erase(attack_time);
			}
		}

		if (insert)
		{
			m_afk_characters.insert(character);
		}
	}

	//============================================================================
	void big_brother::on_character_returned(const char_data* character)
	{
		remove_character_from_set(character);
	}

	//============================================================================
	void big_brother::on_character_disconnected(const char_data* character)
	{
		remove_character_from_set(character);

		typedef std::map<const char_data*, tm>::iterator map_iter;

		map_iter char_map_iter = m_last_engaged_pk_time.find(character);
		if (char_map_iter != m_last_engaged_pk_time.end())
		{
			m_last_engaged_pk_time.erase(char_map_iter);
		}
	}

	//============================================================================
	void big_brother::remove_character_from_set(const char_data* character)
	{
		typedef std::set<const char_data*>::iterator iter;

		iter char_iter = m_afk_characters.find(character);
		if (char_iter != m_afk_characters.end())
		{
			m_afk_characters.erase(character);
		}
	}

	//============================================================================
	void big_brother::on_corpse_decayed(obj_data* corpse)
	{
		typedef std::map<obj_data*, char_data*>::iterator map_iter;

		map_iter corpse_iter = m_corpse_map.find(corpse);
		if (corpse_iter == m_corpse_map.end())
			return;

		m_corpse_map.erase(corpse_iter);
	}

	//============================================================================
	// Private Data Structure implementation
	//============================================================================ 
	//============================================================================
	big_brother::player_corpse_data::player_corpse_data(char_data* dead_man) : num_items_looted(0), killer_id(-1)
	{
		player_race = dead_man->player.race;
		player_id = dead_man->abs_number;
	}

	//============================================================================
	big_brother::player_corpse_data::player_corpse_data(char_data* dead_man, char_data* killer) : num_items_looted(0)
	{
		player_race = dead_man->player.race;
		player_id = dead_man->abs_number;
		
		killer_id = killer->abs_number;
	}
}

