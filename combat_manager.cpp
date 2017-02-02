#include "combat_manager.h"

namespace game_rules
{
	/********************************************************************
	* Singleton Implementation
	*********************************************************************/
	combat_manager* combat_manager::m_pInstance(0);
	bool combat_manager::m_bDestroyed(false);

	combat_manager::combat_manager()
	{

	}

	combat_manager::~combat_manager()
	{

	}
}

