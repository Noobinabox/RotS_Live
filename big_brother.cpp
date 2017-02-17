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
		typedef std::map<const char_data*, tm>::iterator map_iter;

		map_iter attack_time = m_last_engaged_pk_time.find(character);
		if (attack_time != m_last_engaged_pk_time.end())
		{
			// The character has engaged in PK.  How long ago was it?
			time_t current_time;
			time(&current_time);

			tm cooldown;
			cooldown.tm_min = 15;
			cooldown.tm_sec = attack_time->second;


		}

		m_afk_characters.insert(character);
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

		typedef std::map<const char_data*, time_t>::iterator map_iter;

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
}

