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
	// Construction Code:  For building Big Brother.
	//============================================================================ 

#if USE_BIG_BROTHER
	void big_brother::populate_skill_sets()
	{
		m_can_be_helpful_skills.insert(SPELL_CURING);
		m_can_be_helpful_skills.insert(SPELL_RESTLESSNESS);
		m_can_be_helpful_skills.insert(SPELL_INSIGHT);
		m_can_be_helpful_skills.insert(SPELL_PRAGMATISM);
		m_can_be_helpful_skills.insert(SPELL_DISPEL_REGENERATION);

		m_harmful_skills.insert(SKILL_BAREHANDED);
		m_harmful_skills.insert(SKILL_SLASH);
		m_harmful_skills.insert(SKILL_CONCUSSION);
		m_harmful_skills.insert(SKILL_WHIP);
		m_harmful_skills.insert(SKILL_PIERCE);
		m_harmful_skills.insert(SKILL_SPEARS);
		m_harmful_skills.insert(SKILL_AXE);
		m_harmful_skills.insert(SKILL_TWOHANDED);
		m_harmful_skills.insert(SKILL_KICK);
		m_harmful_skills.insert(SKILL_BASH);
		m_harmful_skills.insert(SKILL_SWING);
		m_harmful_skills.insert(SKILL_RIPOSTE);
		m_harmful_skills.insert(SKILL_AMBUSH);
		m_harmful_skills.insert(SPELL_HAZE);
		m_harmful_skills.insert(SPELL_FEAR);
		m_harmful_skills.insert(SPELL_POISON);
		m_harmful_skills.insert(SPELL_TERROR);
		m_harmful_skills.insert(SKILL_ARCHERY);
		m_harmful_skills.insert(SPELL_HALLUCINATE);
		m_harmful_skills.insert(SPELL_CURSE);
		m_harmful_skills.insert(SPELL_MAGIC_MISSILE);
		m_harmful_skills.insert(SPELL_CHILL_RAY);
		m_harmful_skills.insert(SPELL_FREEZE);
		m_harmful_skills.insert(SPELL_LIGHTNING_BOLT);
		m_harmful_skills.insert(SPELL_EARTHQUAKE);
		m_harmful_skills.insert(SPELL_DARK_BOLT);
		m_harmful_skills.insert(SPELL_MIST_OF_BAAZUNGA);
		m_harmful_skills.insert(SPELL_BLAZE);
		m_harmful_skills.insert(SPELL_FIREBOLT);
		m_harmful_skills.insert(SPELL_CONE_OF_COLD);
		m_harmful_skills.insert(SPELL_FIREBALL);
		m_harmful_skills.insert(SPELL_FIREBALL2);
		m_harmful_skills.insert(SPELL_SEARING_DARKNESS);
		m_harmful_skills.insert(SPELL_LIGHTNING_STRIKE);
		m_harmful_skills.insert(SPELL_WORD_OF_PAIN);
		m_harmful_skills.insert(SPELL_WORD_OF_AGONY);
		m_harmful_skills.insert(SPELL_SHOUT_OF_PAIN);
		m_harmful_skills.insert(SPELL_WORD_OF_SHOCK);
		m_harmful_skills.insert(SPELL_LEACH);
		m_harmful_skills.insert(SPELL_BLACK_ARROW);
		m_harmful_skills.insert(SPELL_CONFUSE);
		m_harmful_skills.insert(SKILL_TRAP);
		m_harmful_skills.insert(TYPE_HIT);
		m_harmful_skills.insert(TYPE_BLUDGEON);
		m_harmful_skills.insert(TYPE_PIERCE);
		m_harmful_skills.insert(TYPE_SLASH);
		m_harmful_skills.insert(TYPE_STAB);
		m_harmful_skills.insert(TYPE_WHIP);
		m_harmful_skills.insert(TYPE_SPEARS);
		m_harmful_skills.insert(TYPE_CLEAVE);
		m_harmful_skills.insert(TYPE_FLAIL);
		m_harmful_skills.insert(TYPE_SMITE);
		m_harmful_skills.insert(TYPE_CRUSH);
		m_harmful_skills.insert(TYPE_SUFFERING);
	}
#endif

	//============================================================================
	// Query Code:
	//   This code is responsible for answering yes/no to questions.
	//============================================================================ 
	
	//============================================================================
	// Returns true if the item can be looted from the corpse.
	//============================================================================
	bool big_brother::on_loot_item(char_data* looter, obj_data* corpse, obj_data* item)
	{
#if USE_BIG_BROTHER
		// Something's not right.  Go for it, we won't stop you.
		if (looter == NULL || corpse == NULL || !item)
			return true;

		typedef corpse_map::iterator iter;
		iter corpse_iter = m_corpse_map.find(corpse);

		// If we aren't tracking this corpse - you can loot from it.
		if (corpse_iter == m_corpse_map.end())
			return true;

		// Once a player loots an item from his corpse, his looting protection fades.
		player_corpse_data& corpse_data = corpse_iter->second;
		if (looter->abs_number == corpse_iter->second.player_id)
		{
			on_last_item_removed_from_corpse(corpse_data.player_id, corpse_iter);
			return true;
		}

		// Players on the same side as the race war as you can loot your corpse.
		if (is_same_side_race_war(looter->player.race, corpse_data.player_race))
		{
			if (!item->next_content)
			{
				on_last_item_removed_from_corpse(corpse_data.player_id, corpse_iter);
			}
			return true;
		}

		if (item->obj_flags.type_flag == ITEM_MONEY)
		{
			if (!item->next_content)
			{
				on_last_item_removed_from_corpse(corpse_data.player_id, corpse_iter);
			}
			return true;
		}

		if (corpse_data.num_items_looted >= corpse_data.max_num_items_looted)
			return false;

		// Containers (other than quivers) can't be looted.
		if (item->obj_flags.type_flag == ITEM_CONTAINER && !item->is_quiver())
			return false;

		// The corpse has had less than the max number of items looted from it.  Return that the
		// item can be looted and increment the counter.
		++corpse_data.num_items_looted;

		if (!item->next_content)
		{
			on_last_item_removed_from_corpse(corpse_data.player_id, corpse_iter);
		}

		return true;
#else
		return true;
#endif
	}

	//============================================================================
	void big_brother::on_last_item_removed_from_corpse(int char_id, corpse_map::iterator& corpse_iter)
	{
#if USE_BIG_BROTHER
		// This is the last item from the corpse.  Stop tracking the corpse.
		if (!corpse_iter->second.is_npc)
		{
			remove_character_from_looting_set(char_id);
			send_to_char("You feel the protection of the Gods fade from you...\r\n", char_id);
		}
		m_corpse_map.erase(corpse_iter);
#endif
	}

	//============================================================================
	bool big_brother::is_target_valid(char_data* attacker, const char_data* victim) const
	{
#if USE_BIG_BROTHER
		// Not Big Brother's job to check these pointers.
		if (!attacker || !victim)
			return true;

		// Casting something on yourself is always valid to BB.
		if (attacker == victim)
			return true;

		// Big Brother does not protect against NPC that aren't charmed.
		if (utils::is_npc(*attacker))
		{
			if (utils::is_mob_flagged(*attacker, MOB_PET) || utils::is_mob_flagged(*attacker, MOB_ORC_FRIEND))
			{
				return is_target_valid(attacker->master, victim);
			}
			else
			{
				return true;
			}
		}

		// You cannot attack a character that is writing.
		if (utils::is_player_flagged(*victim, PLR_WRITING))
			return false;

		// Big Brother protects PCs and their mounts.
		if (utils::is_npc(*victim))
		{
			// The victim is a mount.  Test if his rider is a valid target.
			if (utils::is_ridden(*victim))
			{
				return is_target_valid(attacker, victim->mount_data.rider);
			}
			else if (utils::is_mob_flagged(*victim, MOB_PET) || utils::is_mob_flagged(*victim, MOB_ORC_FRIEND))
			{
				return is_target_valid(attacker, victim->master);
			}
			else
			{
				return true;
			}
		}

		// Players cannot attack Gods.
		if (victim->player.level >= LEVEL_MINIMM)
			return false;

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
#else
		return true;
#endif
	}

	//============================================================================
	bool big_brother::is_target_valid(char_data* attacker, const char_data* victim, int skill_id) const
	{
#if USE_BIG_BROTHER
		// If the target isn't protected, just return true.
		if (is_target_valid(attacker, victim))
			return true;

		// The target isn't valid and the skill is offensive.  No go.
		if (is_skill_offensive(skill_id))
			return false;

		// Some skills can be helpful, and are considered to be if cast on a player of the same side.
		// If cast on a player of the opposite side, they are considered harmful.
		bool potentially_helpful = m_can_be_helpful_skills.find(skill_id) != m_can_be_helpful_skills.end();
		if (potentially_helpful)
		{
			return is_same_side_race_war(attacker->player.race, victim->player.race);
		}

		// Skill isn't flagged at all.  It's all good.
		return true;
#else
		return true;
#endif
	}

	//============================================================================
	bool big_brother::is_skill_offensive(int skill_id) const
	{
#if USE_BIG_BROTHER
		return m_harmful_skills.find(skill_id) != m_harmful_skills.end();
#else
		return false;
#endif
	}

	//============================================================================
	bool big_brother::is_target_afk(const char_data* victim) const
	{
#if USE_BIG_BROTHER
		return m_afk_characters.find(victim) != m_afk_characters.end();
#else
		return false;
#endif
	}

	//============================================================================
	bool big_brother::is_target_looting(const char_data* victim) const
	{
#if USE_BIG_BROTHER
		return m_looting_characters.find(victim->abs_number) != m_looting_characters.end();
#else
		return false;
#endif
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
#if USE_BIG_BROTHER
		assert(character);

		// Spirits don't leave corpses behind.  If we have 'shadow' mode for players again,
		// this could create some issues.
		if (corpse == NULL)
			return;

		// The corpse is empty.  No sense in adding loot protection.
		if (corpse->contains == NULL)
			return;

		// Big Brother doesn't track NPC corpses that are not orc followers.
		if (utils::is_npc(*character))
		{
			if (utils::is_mob_flagged(*character, MOB_ORC_FRIEND))
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
#endif
	}

	//============================================================================
	void big_brother::on_character_attacked_player(const char_data* attacker, const char_data* victim)
	{
#if USE_BIG_BROTHER
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
		remove_character_from_looting_set(victim->abs_number);
#endif
	}

	//============================================================================
	void big_brother::on_character_afked(const char_data* character)
	{
#if USE_BIG_BROTHER
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
		else
		{
			send_to_char("You have engaged in PK too recently to benefit from the Gods protection.\r\n", character->abs_number);
		}
#endif
	}

	//============================================================================
	void big_brother::on_character_returned(const char_data* character)
	{
#if USE_BIG_BROTHER
		assert(character);
		remove_character_from_afk_set(character);
#endif
	}

	//============================================================================
	void big_brother::on_character_disconnected(const char_data* character)
	{
#if USE_BIG_BROTHER
		assert(character);
		remove_character_from_afk_set(character);
		remove_character_from_looting_set(character->abs_number);

		typedef time_map::iterator map_iter;

		map_iter char_map_iter = m_last_engaged_pk_time.find(character);
		if (char_map_iter != m_last_engaged_pk_time.end())
		{
			m_last_engaged_pk_time.erase(char_map_iter);
		}
#endif
	}

	//============================================================================
	void big_brother::remove_character_from_afk_set(const char_data* character)
	{
#if USE_BIG_BROTHER
		assert(character);
		typedef character_set::iterator iter;

		iter char_iter = m_afk_characters.find(character);
		if (char_iter != m_afk_characters.end())
		{
			m_afk_characters.erase(character);
		}
#endif
	}

	//============================================================================
	void big_brother::remove_character_from_looting_set(int char_id)
	{
#if USE_BIG_BROTHER
		typedef character_id_set::iterator iter;

		iter char_iter = m_looting_characters.find(char_id);
		if (char_iter != m_looting_characters.end())
		{
			m_looting_characters.erase(char_iter);
		}
#endif
	}

	//============================================================================
	void big_brother::on_corpse_decayed(obj_data* corpse)
	{
#if USE_BIG_BROTHER
		assert(corpse);
		typedef corpse_map::iterator map_iter;

		map_iter corpse_iter = m_corpse_map.find(corpse);
		if (corpse_iter == m_corpse_map.end())
			return;

		remove_character_from_looting_set(corpse_iter->second.player_id);
		m_corpse_map.erase(corpse_iter);
#endif // USE_BIG_BROTHER
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

		// Moved here so that we can change it to grant different corpses different protection if we so choose.
		max_num_items_looted = 2;
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

		// Moved here so that we can change it to grant different corpses different protection if we so choose.
		max_num_items_looted = 2;
	}
}

