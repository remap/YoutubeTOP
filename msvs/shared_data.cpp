#include "shared_data.h"
//
//	shared_data.cpp is part of YouTubeTOP.dll
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

#include <map>
#include <vector>
#include <set>

#include "youtube_chop.h"
#include "youtube_top.h"
#include "stream_controller.h"

using namespace std;

typedef set<const YouTubeTOP*> TopArrayType;
typedef map<string, const YouTubeTOP*> TopMapType;

static TopArrayType TopArray;
static TopMapType TopMap;
static mutex TopAccess;

void SharedData::addTop(const YouTubeTOP * top)
{
	ScopedLock lock(TopAccess);
	TopArray.insert(top);
	TopMap[top->getNodeFullPath()] = top;
}

void SharedData::removeTop(const YouTubeTOP * top)
{
	ScopedLock lock(TopAccess);
	if (TopArray.find(top) != TopArray.end())
	{
		TopArray.erase(top);
		if (TopMap.find(top->getNodeFullPath()) != TopMap.end())
			TopMap.erase(top->getNodeFullPath());
	}
}

const YouTubeTOP * SharedData::getTop(const std::string & topNodeName)
{
	ScopedLock lock(TopAccess);

	if (TopMap.find(topNodeName) != TopMap.end())
		return TopMap[topNodeName];

	return nullptr;
}
