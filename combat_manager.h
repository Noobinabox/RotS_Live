#pragma once

#include "base_utils.h"

struct char_data;
struct weather_data;
struct room_data;

namespace game_rules
{
	class combat_manager
	{
	public:

		// Create a new Singleton and store a pointer to it in m_pInstance.
		static void create(const weather_data& weather, const room_data* world)
		{
			// Task: initialize m_pInstance.  Ensure there is only one.
			static combat_manager theInstance(weather, world);
			m_pInstance = &theInstance;
		}

		/// Gets the instance of the Global Library Manager.
		static combat_manager& instance()
		{
			if (!m_pInstance)
			{
				if (m_bDestroyed)
				{
					// Log that the WaitList has been destroyed.
				}
				else
				{
					// Log that create hasn't been called yet.
				}
			}
			return *m_pInstance;
		}

		// Structure to hold the OB Roll data.
		struct ob_roll
		{
		public:
			ob_roll() : m_rolled_value(0), m_ob_value(0.0) { }
			ob_roll(int roll, double ob) : m_rolled_value(roll), m_ob_value(ob) { }

			int get_rolled_value() { return m_rolled_value; }
			double get_ob_value() { return m_ob_value; }

		private:
			int m_rolled_value;
			double m_ob_value;
		};

		ob_roll roll_ob(char_data* attacker);

		bool can_attack(char_data* attacker, char_data* victim);
		bool is_hit_accurate(const char_data& attacker);

		// This will return the character's 'left-over' OB if their strike hits.  If this value
		// is less than 1, then the character missed.
		double offense_if_weapon_hits(char_data* attacker, char_data* victim, int hit_type);

		double get_evasion_malus(const char_data& attacker, const char_data& victim);

		void on_weapon_hit(char_data* attacker, char_data* victim, int hit_type);

		void apply_damage(char_data* victim, double damage);

	private:
		// Deleted functions.
		// Don't put definitions in here so trying to do them will cause a compile error.
		combat_manager(const combat_manager&);
		combat_manager& operator=(const combat_manager&);

		/// Constructor/Destructor
		combat_manager(const weather_data& weather, const room_data* world);
		~combat_manager();

		/// Instance of the singleton, and lifetime management.
		static combat_manager* m_pInstance;
		static bool m_bDestroyed;

		const weather_data& m_weather;
		const room_data* m_world;
	};
}

