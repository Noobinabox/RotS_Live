#include "combat_manager.h"

#include "char_utils_combat.h"
#include "char_utils.h"
#include "object_utils.h"

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
	double combat_manager::offense_if_weapon_hits(char_data* attacker, char_data* victim, bool hit_accurate)
	{
		ob_roll rolled_ob = roll_ob(attacker);
		
		double attacker_ob = rolled_ob.get_ob_value();

		int victim_position = victim->specials.position;
		if (victim_position < POSITION_FIGHTING)
		{
			attacker_ob += 10 * (POSITION_FIGHTING - victim_position);
		}

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

				if (does_victim_riposte(attacker, victim))
				{
					do_riposte(attacker, victim);
				}

				check_grip(attacker, attacker->equipment[WIELD]);
				check_grip(victim, victim->equipment[WIELD]);
				
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
	bool combat_manager::does_victim_riposte(char_data* attacker, char_data* riposter)
	{
		if (utils::is_npc(*riposter))
			return false;

		int riposte_skill = utils::get_skill(*riposter, SKILL_RIPOSTE);
		if (riposte_skill == 0)
			return false;

		if (riposter->specials.position != POSITION_FIGHTING)
			return false;

		if (utils::is_affected_by(*riposter, AFF_BASH))
			return false;

		obj_data* weapon = riposter->equipment[WIELD];
		if (!weapon)
			return false;

		if (weapon->get_bulk() > 3)
			return false;

		int prob = riposte_skill;
		prob += utils::get_skill(*riposter, SKILL_STEALTH);
		prob += riposter->tmpabilities.dex * 5;
		prob *= utils::get_prof_level(PROF_RANGER, *riposter);
		prob /= 200;

		int roll = number(0, 99);
		return roll <= prob;
	}

	//============================================================================
	void combat_manager::do_riposte(char_data* attacker, char_data* riposter)
	{
		act("$n's riposte catches $N off guard.", FALSE, attacker, 0, riposter, TO_NOTVICT, FALSE);
		act("Your riposte catches $N off guard.", FALSE, attacker, 0, riposter, TO_CHAR, FALSE);
		act("$n's riposte catches you off guard.", FALSE, attacker, 0, riposter, TO_VICT, FALSE);

		obj_data* weapon = riposter->equipment[WIELD];
		double rip_damage = utils::get_weapon_damage(*weapon);
		rip_damage *= std::min((int)riposter->tmpabilities.dex, 20);
		rip_damage /= number_d(50.0, 100.0);

		//TODO(drelidan):  Include skill type and weapon type.
		apply_damage(attacker, rip_damage);

		//if (damage(victim, ch, dam,
			//weapon_hit_type(wielded->obj_flags.value[3]), 21))
			//return 1;
	}

	//============================================================================
	// Basically, if a character is wielding a one-handed weapon
	// with both hands, we cause them to lose energy, and thus slow
	// them down.
	//
	// NOTE: A weapon is considered one handed if it has a bulk of
	// 4 or less.
	//============================================================================
	void combat_manager::check_grip(char_data* character, obj_data* weapon)
	{
		if (!weapon || !character)
			return;

		int bulk = weapon->get_bulk();
		if (bulk <= 4 && utils::is_twohanded(*character))
		{
			int roll = number(0, 99);
			int target = 30 + 5 - bulk;
			if (roll < target)
			{
				character->specials.ENERGY -= 300 + ((5 - bulk) * 100);
				act("You struggle to maintain your grip on $p.", FALSE, character, weapon, NULL, TO_CHAR);
			}
		}
	}

	//============================================================================
	namespace
	{
		//============================================================================
		int weapon_hit_type(int weapon_type)
		{
			int w_type = TYPE_HIT;
			switch (weapon_type) {
			case 0:
			case 1:
			case 2:
				w_type = TYPE_WHIP; /* whip */
				break;
			case 3:
			case 4:
				w_type = TYPE_SLASH;
				break;
			case 5:
				w_type = TYPE_FLAIL;  /* flail */
				break;
			case 6:
				w_type = TYPE_CRUSH;
				break;
			case 7:
				w_type = TYPE_BLUDGEON;
				break;
			case 8:
			case 9:
				w_type = TYPE_CLEAVE;
				break;
			case 10:
				w_type = TYPE_SPEARS;
				break;
			case 11:
				w_type = TYPE_PIERCE;
				break;
			case 12:
				w_type = TYPE_SMITE;
				break;
			default:
				w_type = TYPE_HIT;
				break;
			}
			return w_type;
		}
	} // end anonymous namespace

	//============================================================================
	void combat_manager::on_weapon_hit(char_data* attacker, char_data* victim, int hit_type, bool hit_accurate, double remaining_ob)
	{
		int weapon_type = 0;


		double damage = 0.0;
		obj_data* weapon = attacker->equipment[WIELD];
		if (weapon && weapon->obj_flags.type_flag == ITEM_WEAPON)
		{
			weapon_type = weapon_hit_type(weapon->get_weapon_type());
			damage = utils::get_weapon_damage(*weapon);
		}
		else
		{
			weapon = NULL;

			if (utils::is_npc(*attacker) && attacker->specials.attack_type >= TYPE_HIT)
			{
				weapon_type = attacker->specials.attack_type;
			}
			else
			{
				weapon_type = TYPE_HIT;
				if (utils::is_pc(*attacker))
				{
					damage = BAREHANDED_DAMAGE * 10.0;
				}
			}
		}
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

