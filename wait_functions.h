#pragma once

#include <list>

struct char_data;

namespace wait_functions
{
	//TODO(dgurley):  Singleton implementation of this guy.
	class WaitList
	{
	public:
		void wait_state(char_data* character, int cycle);
		void wait_state_brief(char_data* character, int cycle, 
			int command, int sub_command, int priority, long affect_flag);
		
		void wait_state_full(char_data* character, int cycle, int command, 
			int sub_command, int priority, int delay_flag, signed short ch_num, 
			void* argument, long affect_flag, signed char data_type);

		void abort_delay(char_data* wait_ch);
		void complete_delay(char_data *ch);

		/// Called once per game loop to go through the waiting list
		/// and update its values.
		void update();

	private:
		bool in_waiting_list(char_data *ch);

	private:
		//TODO(dgurley):  Use a more efficient data structure once you 
		// figure out what the access pattern is.
		std::list<char_data*> m_waitingList;

	};
}