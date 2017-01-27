#pragma once
// This file exists for Game-Specific basic utility functions that can be shared by everything.

namespace base_utils
{
	//============================================================================
	template<class T>
	bool is_set(T flag, T bit)
	{
		return (flag & bit) == bit;
	}

	//============================================================================
	template<class T>
	void set_bit(T var, T bit)
	{
		var |= bit;
	}

	//============================================================================
	template<class T>
	void remove_bit(T var, T bit)
	{
		var &= ~bit;
	}

	//============================================================================
	template<class T>
	void toggle_bit(T var, T bit)
	{
		var = var ^ bit;
	}
}

