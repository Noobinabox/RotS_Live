#pragma once

#include "base_utils.h"

namespace game_rules
{
	class combat_manager
	{
	public:

		// Create a new Singleton and store a pointer to it in m_pInstance.
		static void create()
		{
			// Task: initialize m_pInstance.  Ensure there is only one.
			static combat_manager theInstance;
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

	private:
		// Deleted functions.
		// Don't put definitions in here so trying to do them will cause a compile error.
		combat_manager(const combat_manager&);
		combat_manager& operator=(const combat_manager&);

		/// Constructor/Destructor
		combat_manager();
		~combat_manager();

		/// Instance of the singleton, and lifetime management.
		static combat_manager* m_pInstance;
		static bool m_bDestroyed;
	};
}

