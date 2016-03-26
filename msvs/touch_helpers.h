//
//	touch_helpers.h is part of YouTubeTOP.dll
//
//	Copyright 2016 Regents of the University of California
//
//	This program is free software : you can redistribute it and / or modify
//	it under the terms of the GNU Lesser General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//	GNU Lesser General Public License for more details.
//
//	You should have received a copy of the GNU Lesser General Public License
//	along with this program.If not, see <http://www.gnu.org/licenses/>.
// 
//	Author: Peter Gusev, peter@remap.ucla.edu

#ifndef __touch_helpers_h__
#define __touch_helpers_h__

#include <map>
#include <string.h>
#include <algorithm>

struct TouchInput {
	std::string name_;
	unsigned int index_, subIndex_;
};

template<class T1, typename T2>
class TouchInputHelper {
public:
	TouchInputHelper(std::map<T2, TouchInput> inputWiring):
		inputWiring_(inputWiring){}

	bool getStringValue(const T1* arrays, T2 inputName,
		std::string &value)
	{
		if (arrays->numStringInputs > inputWiring_[inputName].index_ &&
			std::string(arrays->stringInputs[inputWiring_[inputName].index_].name) == inputWiring_[inputName].name_)
		{
			std::string str = std::string(arrays->stringInputs[inputWiring_[inputName].index_].value);
			str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
			str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
			value = str;
			return true;
		}
		return false;
	}

	bool getFloatValue(const T1* arrays, T2 inputName,
		float &value)
	{
		if (arrays->numFloatInputs > inputWiring_[inputName].index_ &&
			std::string(arrays->floatInputs[inputWiring_[inputName].index_].name) == inputWiring_[inputName].name_)
		{
			int idx = inputWiring_[inputName].index_;
			int subIdx = inputWiring_[inputName].subIndex_;
			value = arrays->floatInputs[idx].values[subIdx];
			return true;
		}
		return false;
	}

	bool updateFloatValue(const T1* arrays, T2 inputName,
		bool &updated, float &value)
	{
		float newValue = 0;
		//updated = false;

		if (getFloatValue(arrays, inputName, newValue))
		{
			if (value != newValue)
			{
				updated = true;
				value = newValue;
			}

			return true;
		}
		return false;
	}

	bool getBoolValue(const T1* arrays, T2 inputName,
		bool &value)
	{
		if (arrays->numFloatInputs > inputWiring_[inputName].index_ &&
			std::string(arrays->floatInputs[inputWiring_[inputName].index_].name) == inputWiring_[inputName].name_)
		{
			int idx = inputWiring_[inputName].index_;
			int subIdx = inputWiring_[inputName].subIndex_;
			value = arrays->floatInputs[idx].values[subIdx] > 0.5;
			return true;
		}
		return false;
	}

private:
	std::map<T2, TouchInput> inputWiring_;
};

#endif