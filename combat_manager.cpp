#include "combat_manager.h"

#include "char_utils_combat.h"
#include "char_utils.h"

#include "utils.h"
#include "spells.h"
#include "comm.h"

namespace game_rules
{
	/********************************************************************
	* Public Functions
	*********************************************************************/
	//============================================================================
	bool combat_manager::can_attack(char_data* attacker, char_data* victim)
	{
		if (!attacker || !victim)
			return false;

		if (attacker == victim)
			return false;

		// TODO(drelidan):  Put additional checks in here, such as sight, peace room,
		// attacker and victim being colocated, etc.

		return true;
	}

	//============================================================================
	combat_manager::ob_roll combat_manager::roll_ob(char_data* attacker)
	{
		double roll = number(35.0);

		const room_data& attacker_room = m_world[attacker->in_room];
		double attacker_OB = utils::get_real_ob(*attacker, m_weather, attacker_room);
		attacker_OB += number_d(1, 55 + attacker_OB * 0.25);
		attacker_OB += roll;
		attacker_OB = attacker_OB * 7.0 / 8.0;
		attacker_OB -= 40.0;

		bool is_crit = roll > 34.0;
		if (is_crit)
		{
			attacker_OB += 100.0;
		}

		return ob_roll(is_crit, attacker_OB);
	}

	//============================================================================
	bool combat_manager::is_hit_accurate(const char_data& attacker)
	{
		int tactics = utils::get_tactics(attacker);
		
		// Hits can only be accurate on careful or aggressive tactics.
		if (tactics != TACTICS_CAREFUL && tactics != TACTICS_AGGRESSIVE)
			return false;

		int target_number = utils::get_prof_level(PROF_RANGER, attacker);
		target_number -= utils::get_skill_penalty(attacker);
		target_number -= utils::get_dodge_penalty(attacker);
		target_number *= utils::get_skill(attacker, SKILL_ACCURACY) / 100;

		int roll = number(0, 100);
		return roll < target_number;
	}

	//============================================================================
	double combat_manager::offense_if_weapon_hits(char_data* attacker, char_data* victim, int hit_type)
	{
		ob_roll rolled_ob = roll_ob(attacker);
		
		double attacker_ob = rolled_ob.get_ob_value();

		int victim_position = victim->specials.position;
		if (victim_position < POSITION_FIGHTING)
		{
			attacker_ob += 10 * (POSITION_FIGHTING - victim_position);
		}

		bool hit_accurate = is_hit_accurate(*attacker);
		if (hit_accurate)
		{
			act("You manage to find an opening in $N's defense!", TRUE, attacker, NULL, victim, TO_CHAR);
			act("$n notices an opening in your defense!", TRUE, attacker, NULL, victim, TO_VICT);
		}
		else
		{
			// Test for dodge and evasion.
			double dodge_malus = utils::get_real_dodge(*victim);

			attacker_ob -= dodge_malus;
			if (attacker_ob < 0.0 && !rolled_ob.is_crit())
			{
				// Victim dodged the attack.
				act("$N dodges $n's attack.", FALSE, attacker, 0, victim, TO_NOTVICT, TRUE);
				act("$N dodges your attack.", FALSE, attacker, 0, victim, TO_CHAR, TRUE);
				act("You dodge $n's attack.", FALSE, attacker, 0, victim, TO_VICT, TRUE);
				return attacker_ob;
			}

			double evasion_malus = get_evasion_malus(*attacker, *victim);

			attacker_ob -= evasion_malus;
			if (attacker_ob < 0.0 && !rolled_ob.is_crit())
			{
				// Victim evaded the attack.
				act("$N distracts $n into missing $M.", FALSE, attacker, 0, victim, TO_NOTVICT, TRUE);
				act("$N is not where you attack $M.", FALSE, attacker, 0, victim, TO_CHAR, TRUE);
				act("You evade $n's attack.", FALSE, attacker, 0, victim, TO_VICT, TRUE);
				return attacker_ob;
			}
		}

		// Test for parry.
		if (victim_position > POSITION_STUNNED)
		{
			const room_data& room = m_world[victim->in_room];
			double victim_parry = utils::get_real_parry(*victim, m_weather, room);
			victim_parry *= victim->specials.current_parry * 0.01;

			// Apply a parry malus to the victim.
			victim->specials.current_parry = victim->specials.current_parry * 2 / 3;

			attacker_ob -= victim_parry;
			if (rolled_ob.is_crit())
			{
				// Attackers can't get below 0 OB with a roll of 35.
				attacker_ob = std::max(attacker_ob, 0.0);
			}

			if (attacker_ob < 0.0)
			{
				act("$N deflects $n's attack.", FALSE, attacker, 0, victim, TO_NOTVICT, TRUE);
				act("$N deflects your attack.", FALSE, attacker, 0, victim, TO_CHAR, TRUE);
				act("You deflect $n's attack.", FALSE, attacker, 0, victim, TO_VICT, TRUE);

				//TODO(dgurley):  Add Riposte check in here.
				/*
				if (check_riposte(ch, victim))
					return;

				check_grip(ch, wielded);
				check_grip(victim, victim->equipment[WIELD]);
				*/
			}
		}

		return attacker_ob;
	}

	//============================================================================
	double combat_manager::get_evasion_malus(const char_data& attacker, const char_data& victim)
	{
		if (!utils::is_affected_by(victim, AFF_EVASION))
			return 0.0;

		const double BASE_VALUE = 5.0;

		double defender_bonus = utils::get_prof_level(PROF_CLERIC, victim) * 0.5;
		double attacker_offset = utils::get_prof_level(PROF_CLERIC, attacker) * (100 - utils::get_perception(attacker)) * 0.005;
		
		if (utils::is_npc(attacker))
		{
			attacker_offset *= 0.5;
		}

		// Always return at least the base value.
		if (attacker_offset > defender_bonus)
			return BASE_VALUE;

		return BASE_VALUE + defender_bonus - attacker_offset;
	}
	
	//============================================================================
	void combat_manager::on_weapon_hit(char_data* attacker, char_data* victim, int hit_type)
	{

	}

	//============================================================================
	void combat_manager::apply_damage(char_data* victim, double damage)
	{

	}



	/********************************************************************
	* Singleton Implementation
	*********************************************************************/
	combat_manager* combat_manager::m_pInstance(0);
	bool combat_manager::m_bDestroyed(false);

	//============================================================================
	combat_manager::combat_manager(const weather_data& weather, const room_data* world) :
		m_weather(weather), m_world(world)
	{

	}

	//============================================================================
	combat_manager::~combat_manager()
	{

	}
}

