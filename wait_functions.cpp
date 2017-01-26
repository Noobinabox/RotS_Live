#include "base_utils.h"
#include "wait_functions.h"
#include "structs.h"
#include "comm.h" // for delay functions

namespace wait_functions
{
	//============================================================================
	void start_wait(char_data* character, int cycle)
	{
		int command = 0;
		int sub_command = 0;
		int priority = 50;
		int delay_flag = 0;
		sh_int ch_num = 0;
		void* argument = NULL;
		int sub_command = 0;
		
		wait_state_full(character, cycle, command, sub_command, priority, 
			delay_flag, ch_num, argument, AFF_WAITING, TARGET_IGNORE);
	}

	//============================================================================
	void wait_state_brief(char_data* character, int cycle, int command, int sub_command, 
		int priority, long affect_flag)
	{
		wait_state_full(character, cycle, command, sub_command, priority, affect_flag,
			0, NULL, affect_flag, TARGET_IGNORE);
	}

	//============================================================================
	void wait_state_full(char_data* character, int cycle, int command, int sub_command,
		int priority, int delay_flag, sh_int ch_num, void* argument, long affect_flag,
		signed char data_type)
	{
		if (character->delay.wait_value != 0)
		{
			if (priority > character->delay.priority)
			{
				character->delay.subcmd = -1;
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

		character->delay.wait_value = cycle;
		character->delay.cmd = command;
		character->delay.subcmd = sub_command;
		character->delay.targ1.ptr.other = argument;
		
		character->delay.priority = priority;
		character->delay.flg = delay_flag;
		character->delay.targ1.ch_num = ch_num;

		character->delay.targ1.type = data_type;
		character->delay.targ2.type = TARGET_IGNORE;
		base_utils::set_bit(character->specials.affected_by, affect_flag);

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

		char_data* waiting_list = NULL;
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