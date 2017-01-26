#pragma once

struct char_data;

namespace wait_functions
{
	void start_wait(char_data& character, int cycle);
	void wait_state_brief(char_data& character, int cycle);
	void wait_state_full(char_data& character, int cycle);



	namespace _INTERNAL
	{

	}
}