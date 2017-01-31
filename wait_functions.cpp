#include "base_utils.h"
#include "char_utils.h"
#include "wait_functions.h"
#include "structs.h"

#include "comm.h" // for send-to-char
#include <algorithm>

namespace wait_functions
{
	//============================================================================
	void WaitList::wait_state(char_data* character, int cycle)
	{
		if (!character)
			return;

		int command = 0;
		int sub_command = 0;
		int priority = 50;
		int delay_flag = 0;
		sh_int ch_num = 0;
		void* argument = NULL;
		
		wait_state_full(character, cycle, command, sub_command, priority, 
			delay_flag, ch_num, argument, AFF_WAITING, TARGET_IGNORE);
	}

	//============================================================================
	void WaitList::wait_state_brief(char_data* character, int cycle, int command, int sub_command, 
		int priority, long affect_flag)
	{
		if (!character)
			return;

		wait_state_full(character, cycle, command, sub_command, priority, affect_flag,
			0, NULL, affect_flag, TARGET_IGNORE);
	}

	// Adding this namespace to hold on to previous code that I plan
	// on overwriting as a reference.
	namespace CODE_SAVER
	{
		void add_char_to_waitlist(char_data* character, char_data* waiting_list)
		{
			/*
			* bsantosus:  I kidnapped computer.
			*  I love ROTS! It's the best game EVERS! <3
			*  Dave and Brenda 4/14/2017! <3
			*/

			//TODO(dgurley):  Parse the function below to figure out what it is.
			//                  I think it's a forward list just looking at it, but we'll see.
			//TODO(dgurley):  Figure out a better way to handle waiting.
			//
			// It looks like it's a queue implementation.
			// Keep track of last.
			// This file will take over complete_delay and abort_delay.
			// Make use of std::find to see if the character is in the waiting list.
			char_data* temp_character = waiting_list;

			if (temp_character == character)
			{
				temp_character = character->delay.next;
				waiting_list = temp_character;
			}

			// If there is no waiting list, the character is moved into the waiting list.
			// If there is a waiting list, ensure that the character is at the end of it.

			// All of this list wait stuff held in the character->.. ugh.
			// This will be replaced by an appropriate data structure.
			if (!temp_character)
			{
				waiting_list = character;
			}
			else
			{
				while (temp_character->delay.next)
				{
					if (temp_character->delay.next == character)
					{
						temp_character->delay.next = character->delay.next;
					}
					else
					{
						temp_character = temp_character->delay.next;
					}
				}

				if (temp_character != character)
				{
					temp_character->delay.next = character;
				}
			}

			character->delay.next = NULL;
		}
	}

	//============================================================================
	void WaitList::wait_state_full(char_data* character, int cycle, int command, int sub_command,
		int priority, int delay_flag, signed short ch_num, void* argument, long affect_flag,
		signed char data_type)
	{
		if (!character)
			return;

		waiting_type& delay = character->delay;

		// If the character is already waiting, do a priority test.
		// If this wait is a higher priority, complete the previous
		// priority for the character.
		if (delay.wait_value != 0)
		{
			if (priority > delay.priority)
			{
				delay.subcmd = -1;
				complete_delay(character);
				abort_delay(character);
			}
			else
			{
				send_to_char("Possible bug - double delay. Please notify Imps.\n", character);
				log("double delay?\n");
				return;
			}
		}

		delay.wait_value = cycle;
		delay.cmd = command;
		delay.subcmd = sub_command;
		delay.targ1.ptr.other = argument;
		
		delay.priority = priority;
		delay.flg = delay_flag;
		delay.targ1.ch_num = ch_num;

		delay.targ1.type = data_type;
		delay.targ2.type = TARGET_IGNORE;
		base_utils::set_bit(character->specials.affected_by, affect_flag);

		// This replicates the "add_char_to_waitlist" code, which is based on the waiting macro.
		
		// If the character is in the waiting list or the pending delete list, remove him.
		// Then insert the character at the end of the waiting list.
		m_pendingDeletes.erase(std::remove(m_pendingDeletes.begin(), m_pendingDeletes.end(), character));
		m_waitingList.erase(std::find(m_waitingList.begin(), m_waitingList.end(), character));
		m_waitingList.push_back(character);
	}

	//============================================================================
	void WaitList::abort_delay(char_data* character)
	{
		if (!character)
			return;

		// Update the character's wait values and priority.
		base_utils::remove_bit(character->specials.affected_by, (long)AFF_WAITWHEEL);
		base_utils::remove_bit(character->specials.affected_by, (long)AFF_WAITING);
		character->delay.wait_value = 0;
		character->delay.priority = 0;

		// Move the character into our pending delete list.
		m_pendingDeletes.push_back(character);
	}

	//============================================================================
	void WaitList::complete_delay(char_data *ch)
	{
		/*
		SPECIAL(*tmpfunc);

		ch->delay.wait_value = 0;
		base_utils::remove_bit(ch->specials.affected_by, (long)AFF_WAITWHEEL);
		base_utils::remove_bit(ch->specials.affected_by, (long)AFF_WAITING);

		if (ch->delay.cmd == CMD_SCRIPT) 
		{
			continue_char_script(ch);
			return;
		}

		if (ch->delay.cmd == -1 && IS_NPC(ch)) 
		{
			// Here calls special procedure
			if (mob_index[ch->nr].func)
			{
				(*mob_index[ch->nr].func) (ch, 0, -1, "", SPECIAL_NONE, &(ch->delay));
			}
			else if (ch->specials.store_prog_number) 
			{
				tmpfunc = (SPECIAL(*)) virt_program_number(ch->specials.store_prog_number);
				tmpfunc(ch, 0, ch->delay.cmd, "", SPECIAL_DELAY, &(ch->delay));
			}
			else if (ch->specials.union1.prog_number)
			{
				intelligent(ch, 0, -1, "", SPECIAL_DELAY, &(ch->delay));
			}
		}
		else if (ch->delay.cmd > 0)
		{
			command_interpreter(ch, "", &(ch->delay));
		}
		*/
	}

	//============================================================================
	void WaitList::update()
	{
		static char* wait_wheel[8] = { "\r|\r", "\r\\\r", "\r-\r", "\r/\r", "\r|\r", "\r\\\r", "\r-\r", "\r/\r" };
		typedef std::_List_iterator<char_data*> iter;

		// Clean up pending deletes.
		for (size_t i = 0; i < m_pendingDeletes.size(); ++i)
		{
			char_data* removeMe = m_pendingDeletes[i];
			m_waitingList.erase(std::find(m_waitingList.begin(), m_waitingList.end(), removeMe));
		}
		m_pendingDeletes.clear();

		// Process commands in the wait list.
		for (iter char_iter = m_waitingList.begin(); char_iter != m_waitingList.end(); )
		{
			char_data* character = *char_iter;
			int wait_value = character->delay.wait_value;
			if (wait_value > 0)
			{
				wait_value = --character->delay.wait_value;
			}

			if (wait_value > 0)
			{
				if (!char_utils::is_npc(*character) && char_utils::is_affected_by(*character, AFF_WAITWHEEL))
				{
					if (char_utils::is_preference_flagged(*character, PRF_SPINNER))
					{
						// Add this function in somewhere.
						write_to_descriptor(character->desc->descriptor, wait_wheel[wait_value % 8]);
					}
				}

				++char_iter;
			}
			else if (character->delay.wait_value == 0)
			{
				/* here is the block calling actual procedures */
				complete_delay(character);
				if (character->delay.wait_value == 0)
				{
					/* look out for the similar code in raw_kill() */
					//abort_delay(character);
					char_iter = m_waitingList.erase(char_iter);
				}
				else
				{
					++char_iter;
				}
			}
			else
			{
				// Are wait values of less than 0 desired/allowed?
				// Unsure of what the purpose would be.  The character
				// would stay in the list forever.
				++char_iter;
			}
		}
	}

	//============================================================================
	bool WaitList::in_waiting_list(char_data* character)
	{
		return std::find(m_waitingList.begin(), m_waitingList.end(), character) != m_waitingList.end();
	}
}

