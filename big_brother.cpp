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
	bool big_brother::is_target_valid(char_data* attacker, const char_data* victim, int skill_id) const
	{
		// Not Big Brother's job to check these pointers.
		if (!attacker || !victim)
			return true;

		// Big Brother only protects PCs.
		if (utils::is_npc(*attacker) || utils::is_npc(*victim))
			return true;

		// The ability being used isn't offensive - all good.
		if (!is_skill_offensive(skill_id))
			return true;

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
	bool big_brother::is_same_side_race_war(const char_data* attacker, const char_data* victim)
	{
		/*
		* Decide if `character' and `other' are on the same side of the race
		* war.  Return 0 if they are, return 1 if they aren't.
		*/
		if (IS_NPC(other) && !IS_AFFECTED(other, AFF_CHARM))
			return 0;
		if (IS_NPC(character) && !IS_AFFECTED(character, AFF_CHARM))
			return 0;
		if ((GET_RACE(character) == RACE_GOD) || (GET_RACE(other) == RACE_GOD))
			return 0;
		if (RACE_EAST(other) && !(RACE_EAST(character)))
			return 1;
		if (!(RACE_EAST(other)) && RACE_EAST(character))
			return 1;
		if (RACE_MAGI(other) && !(RACE_MAGI(character)))
			return 1;
		if (!(RACE_MAGI(other)) && RACE_MAGI(character))
			return 1;
		if (RACE_EVIL(other) && RACE_GOOD(character))
			return 1;
		if (RACE_GOOD(other) && RACE_EVIL(character))
			return 1;

		return 0;
	}

	//============================================================================
	// Data Structure Management Code:
	//   This code is responsible for ensuring that our data structures stay up-to-date.
	//============================================================================ 
	
	//============================================================================
	void big_brother::on_character_attacked_player(const char_data* attacker)
	{
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
}

