#pragma once

#include <list>
#include <vector>

struct char_data;

class WaitList
{
public:
	// Create a new Singleton and store a pointer to it in m_pInstance.
	static void Create()
	{
		// Task: initialize m_pInstance.  Ensure there is only one.
		static WaitList theInstance;
		m_pInstance = &theInstance;
	}

	/// Gets the instance of the Global Library Manager.
	static WaitList& Instance()
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
	std::vector<char_data*> m_pendingDeletes;

	// TODO(dgurley):  Switch m_waitingList to a std::vector and m_pendingDeletes
	// to a set and implement an algorithm using 'remove-if' on the vector.
	// It's just a pain to do it atm without lambdas.
	/*
	std::vector<char_data*> m_waitingList;
	std::unordered_set<char_data> m_pendingDeletes;
	*/

	// Deleted functions.
	// Don't put definitions in here so trying to do them will cause a compile error.
	WaitList(const WaitList&);
	WaitList& operator=(const WaitList&);

	/// Constructor/Destructor
	WaitList();
	~WaitList();

	/// Instance of the singleton, and lifetime management.
	static WaitList* m_pInstance;
	static bool m_bDestroyed;
};

