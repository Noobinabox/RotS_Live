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
	bool big_brother::is_target_afk(const char_data* victim)
	{
		return m_afk_characters.find(victim) != m_afk_characters.end();
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

