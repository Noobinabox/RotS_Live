#include "char_utils.h"
#include "delayed_command_interpreter.h"
#include "structs.h"

#include "comm.h" // for send-to-char
#include "interpre.h" // for complete-delay functionality
#include "script.h" // for complete-delay functionality
#include "mudlle.h" // for complete-delay functionality
#include "db.h" // for complete-delay functionality

#include <algorithm>

// TODO(drelidan):  Put an accessor on here or something.
extern struct index_data* mob_index; // for complete-delay functionality

namespace game_types
{
	//============================================================================
	delayed_command_interpreter::delayed_command_interpreter(char_data* character) : m_character(character)
	{

	}

	//============================================================================
	void delayed_command_interpreter::run()
	{
		if (!m_character)
			return;

		if (m_character->delay.cmd == CMD_SCRIPT)
		{
			continue_char_script(m_character);
			return;
		}

		typedef int(*special_ptr) (char_data* ch, char_data* victim, int cmd, char* argument, int call_flag, waiting_type* wait_list);

		waiting_type* delay = &m_character->delay;
		char_data* victim = NULL;
		if (m_character->delay.cmd == -1 && utils::is_npc(*m_character))
		{
			special_ptr func_pointer = NULL;
			int index = m_character->nr;

			// Here calls special procedure
			func_pointer = mob_index[index].func;
			if (func_pointer != NULL)
			{
				func_pointer(m_character, victim, -1, "", SPECIAL_NONE, delay);
			}
			else if (m_character->specials.store_prog_number)
			{
				int function_number = m_character->specials.store_prog_number;
				func_pointer = get_special_function(function_number);
				func_pointer(m_character, victim, m_character->delay.cmd, "", SPECIAL_DELAY, delay);
			}
			else if (m_character->specials.union1.prog_number)
			{
				intelligent(m_character, victim, -1, "", SPECIAL_DELAY, delay);
			}
		}
		else if (m_character->delay.cmd > 0)
		{
			command_interpreter(m_character, "", delay);
		}
	}
}

