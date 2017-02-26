#include "big_brother.h"

#include "object_utils.h"
#include "char_utils.h"
#include "structs.h"
#include "spells.h"
#include "comm.h"

#include <assert.h>

/********************************************************************
* Singleton Implementation
*********************************************************************/
template<>
game_rules::big_brother* world_singleton<game_rules::big_brother>::m_pInstance(0);

template<>
bool world_singleton<game_rules::big_brother>::m_bDestroyed(false);

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
		if (looter == NULL || corpse == NULL || !item)
			return true;

		typedef corpse_map::iterator iter;
		iter corpse_iter = m_corpse_map.find(corpse);

		// If we aren't tracking this corpse - you can loot from it.
		if (corpse_iter == m_corpse_map.end())
			return true;

		// Players on the same side as the race war as you can loot your corpse.
		player_corpse_data& corpse_data = corpse_iter->second;
		if (is_same_side_race_war(looter->player.race, corpse_data.player_race))
		{
			if (!item->next_content)
			{
				// This is the last item from the corpse.  Stop tracking the corpse.
				remove_character_from_looting_set(corpse_data.player_id);
				send_to_char("You feel the protection of the Gods fade from you...", corpse_data.player_id);
				m_corpse_map.erase(corpse_iter);
			}
			return true;
		}

		if (corpse_data.num_items_looted >= 2)
			return false;

		// Containers (other than quivers) can't be looted.
		if (item->obj_flags.type_flag == ITEM_CONTAINER && !item->is_quiver())
			return false;

		// The corpse has had less than 2-items looted from it.  Return that the
		// item can be looted and increment the counter.
		++corpse_data.num_items_looted;

		if (!item->next_content)
		{
			// This is the last item from the corpse.  Stop tracking the corpse.
			remove_character_from_looting_set(corpse_data.player_id);
			send_to_char("You feel the protection of the Gods fade from you...", corpse_data.player_id);
			m_corpse_map.erase(corpse_iter);
		}

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
		const int TYPE_END = 9999;

		static int OFFENSIVE_SKILLS[] = { 
			SKILL_BAREHANDED,
			SKILL_SLASH,
			SKILL_CONCUSSION,
			SKILL_WHIP,
			SKILL_PIERCE,
			SKILL_SPEARS,
			SKILL_AXE,
			SKILL_TWOHANDED,
			SKILL_KICK,
			SKILL_BASH,
			SKILL_SWING,
			SKILL_RIPOSTE,
			SKILL_AMBUSH,
			SPELL_CURING,
			SPELL_RESTLESSNESS,
			SPELL_INSIGHT,
			SPELL_PRAGMATISM,
			SPELL_HAZE,
			SPELL_FEAR,
			SPELL_POISON,
			SPELL_TERROR,
			SKILL_ARCHERY,
			SPELL_HALLUCINATE,
			SPELL_CURSE,
			SPELL_MAGIC_MISSILE,
			SPELL_CHILL_RAY,
			SPELL_FREEZE,
			SPELL_LIGHTNING_BOLT,
			SPELL_EARTHQUAKE,
			SPELL_DARK_BOLT,
			SPELL_MIST_OF_BAAZUNGA,
			SPELL_BLAZE,
			SPELL_FIREBOLT,
			SPELL_CONE_OF_COLD,
			SPELL_FIREBALL,
			SPELL_FIREBALL2,
			SPELL_SEARING_DARKNESS,
			SPELL_LIGHTNING_STRIKE,
			SPELL_WORD_OF_PAIN,
			SPELL_WORD_OF_AGONY,
			SPELL_WORD_OF_SHOCK,
			SPELL_LEACH,
			SPELL_BLACK_ARROW,
			SPELL_CONFUSE,
			SKILL_TRAP,
			TYPE_HIT,
			TYPE_BLUDGEON,
			TYPE_PIERCE,
			TYPE_SLASH,
			TYPE_STAB,
			TYPE_WHIP,
			TYPE_SPEARS,
			TYPE_CLEAVE,
			TYPE_FLAIL,
			TYPE_SMITE,
			TYPE_CRUSH,
			TYPE_SUFFERING,
			TYPE_END
		};

		int skill_index = 0;
		while (OFFENSIVE_SKILLS[skill_index] != TYPE_END)
		{
			if (OFFENSIVE_SKILLS[skill_index] == skill_id)
			{
				return true;
			}
			++skill_index;
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
		return m_looting_characters.find(victim->abs_number) != m_looting_characters.end();
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
	char_data* big_brother::get_valid_target(char_data* attacker, const char_data* victim, const char* argument) const
	{
		// TODO(drelidan):  Implement logic here.
		return NULL;
	}

	//============================================================================
	// Data Structure Management Code:
	//   This code is responsible for ensuring that our data structures stay up-to-date.
	//============================================================================ 
	
	//============================================================================
	void big_brother::on_character_died(char_data* character, char_data* killer, obj_data* corpse)
	{
		assert(character);

		// Spirits don't leave corpses behind.  If we have 'shadow' mode for players again,
		// this could create some issues.
		if (corpse == NULL)
			return;

		// Big Brother doesn't track NPC corpses that are not orc followers.
		if (utils::is_npc(*character))
		{
			if (utils::is_affected_by(*character, AFF_CHARM))
			{
				// Protect the mob in the corpse map, but don't give anyone looting protection.
				m_corpse_map[corpse] = player_corpse_data(character, killer);
			}

			return;
		}

		// Each corpse will be unique, so even if a player dies multiple times,
		// each corpse will be protected.
		m_corpse_map[corpse] = player_corpse_data(character, killer);
		m_looting_characters.insert(character->abs_number);
	}

	//============================================================================
	void big_brother::on_character_attacked_player(const char_data* attacker, const char_data* victim)
	{
		assert(attacker);
		assert(victim);

		// Get the current time.
		time_t current_time;
		time(&current_time);

		tm* time_info = localtime(&current_time);
		m_last_engaged_pk_time[attacker] = *time_info; // copy tm_struct into map
		m_last_engaged_pk_time[victim] = *time_info; // copy tm_struct into map

		// If you attack someone in PK, you are no longer considered looting.
		remove_character_from_looting_set(attacker->abs_number);
	}

	//============================================================================
	void big_brother::on_character_afked(const char_data* character)
	{
		assert(character);

		// This is one possible implementation.  Another is to do the time-check in the
		// "is-character-afk" test.
		const double CUTOFF_SECS = 900;
		typedef time_map::iterator map_iter;

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
		assert(character);
		remove_character_from_afk_set(character);
	}

	//============================================================================
	void big_brother::on_character_disconnected(const char_data* character)
	{
		assert(character);
		remove_character_from_afk_set(character);
		remove_character_from_looting_set(character->abs_number);

		typedef time_map::iterator map_iter;

		map_iter char_map_iter = m_last_engaged_pk_time.find(character);
		if (char_map_iter != m_last_engaged_pk_time.end())
		{
			m_last_engaged_pk_time.erase(char_map_iter);
		}
	}

	//============================================================================
	void big_brother::remove_character_from_afk_set(const char_data* character)
	{
		assert(character);
		typedef character_set::iterator iter;

		iter char_iter = m_afk_characters.find(character);
		if (char_iter != m_afk_characters.end())
		{
			m_afk_characters.erase(character);
		}
	}

	//============================================================================
	void big_brother::remove_character_from_looting_set(int char_id)
	{
		typedef character_id_set::iterator iter;

		iter char_iter = m_looting_characters.find(char_id);
		if (char_iter != m_looting_characters.end())
		{
			m_looting_characters.erase(char_iter);
		}
	}

	//============================================================================
	void big_brother::on_corpse_decayed(obj_data* corpse)
	{
		assert(corpse);
		typedef corpse_map::iterator map_iter;

		map_iter corpse_iter = m_corpse_map.find(corpse);
		if (corpse_iter == m_corpse_map.end())
			return;

		remove_character_from_looting_set(corpse_iter->second.player_id);
		m_corpse_map.erase(corpse_iter);
	}

	//============================================================================
	// Private Data Structure implementation
	//============================================================================ 
	//============================================================================
	big_brother::player_corpse_data::player_corpse_data(char_data* dead_man) : num_items_looted(0), killer_id(-1)
	{
		assert(dead_man);
		player_race = dead_man->player.race;
		player_id = dead_man->abs_number;

		is_npc = utils::is_npc(*dead_man);
	}

	//============================================================================
	big_brother::player_corpse_data::player_corpse_data(char_data* dead_man, char_data* killer) : num_items_looted(0)
	{
		assert(dead_man);
		player_race = dead_man->player.race;
		player_id = dead_man->abs_number;
		
		is_npc = utils::is_npc(*dead_man);

		if (killer)
		{
			killer_id = killer->abs_number;
		}
		else
		{
			killer_id = -1;
		}
	}
}

