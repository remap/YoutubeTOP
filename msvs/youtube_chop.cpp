//
//	youtube_chop.cpp is part of YouTubeTOP.dll
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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <map>
#include <vector>
#include <sstream>

#include "youtube_chop.h"
#include "youtube_top.h"
#include "shared_data.h"

enum class InfoDatIndex {
	State,
	Binding
};

static std::map<InfoDatIndex, std::string> RowNames = {
	{ InfoDatIndex::State, "state" },
	{ InfoDatIndex::Binding, "binding" }
};

enum class InfoChopIndex {
	ExecuteCount,
	Samples
};

static std::map<InfoChopIndex, std::string> ChanNames = {
	{ InfoChopIndex::ExecuteCount, "executeCount" },
	{ InfoChopIndex::Samples, "nSamples" }
};

enum class TouchInputName {
	TopPath
};

static std::map<TouchInputName, TouchInput> TouchInputs = {
	{ TouchInputName::TopPath,{ "string0", 0, 0 } }
};

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

	DLLEXPORT
		int
		GetCHOPAPIVersion(void)
	{
		// Always return CHOP_CPLUSPLUS_API_VERSION in this function.
		return CHOP_CPLUSPLUS_API_VERSION;
	}

	DLLEXPORT
		CHOP_CPlusPlusBase*
		CreateCHOPInstance(const CHOP_NodeInfo *info)
	{
		// Return a new instance of your class every time this is called.
		// It will be called once per CHOP that is using the .dll
		return new YouTubeCHOP(info);
	}

	DLLEXPORT
		void
		DestroyCHOPInstance(CHOP_CPlusPlusBase *instance)
	{
		// Delete the instance here, this will be called when
		// Touch is shutting down, when the CHOP using that instance is deleted, or
		// if the CHOP loads a different DLL
		delete (YouTubeCHOP*)instance;
	}

};


YouTubeCHOP::YouTubeCHOP(const CHOP_NodeInfo *info) : myNodeInfo(info),
status_(NotBinded)
{
	myExecuteCount = 0;
}

YouTubeCHOP::~YouTubeCHOP()
{

}

void
YouTubeCHOP::getGeneralInfo(CHOP_GeneralInfo *ginfo)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = true;
	ginfo->timeslice = true;
	ginfo->inputMatchIndex = 0;
}

bool
YouTubeCHOP::getOutputInfo(CHOP_OutputInfo *info)
{
	// If there is an input connected, we are going to match it's channel names etc
	// otherwise we'll specify our own.
	if (info->inputArrays->numCHOPInputs > 0)
	{
		return false;
	}
	else
	{
		info->numChannels = 1;

		// Since we are outputting a timeslice, the system will dictate
		// the length and startIndex of the CHOP data
		//info->length = 1;
		//info->startIndex = 0

		// For illustration we are going to output 120hz data
		info->sampleRate = 120;
		return true;
	}
}

const char*
YouTubeCHOP::getChannelName(int index, void* reserved)
{
	return "chan1";
}

void
YouTubeCHOP::execute(const CHOP_Output* output,
	const CHOP_InputArrays* inputs,
	void* reserved)
{
	updateParameters(inputs);
	myExecuteCount++;

	// In this case we'll just take the first input and re-output it with it's
	// value divivded by two
	if (inputs->numCHOPInputs > 0)
	{
		// We know the first CHOP has the same number of channels
		// because we retuend false from getOutputInfo. 

		int ind = 0;
		for (int i = 0; i < output->numChannels; i++)
		{
			for (int j = 0; j < output->length; j++)
			{
				output->channels[i][j] = inputs->CHOPInputs[0].channels[i][ind] / 2.0f;
				ind++;
				// Make sure we don't read past the end of the CHOP input
				ind = ind % inputs->CHOPInputs[0].length;
			}
		}

	}
	else // If not input is connected, lets output a sine wave instead
	{
		// Notice that startIndex and the output->length is used to output a smooth
		// wave by ensuring that we are outputting a value for each sample
		// Since we are outputting at 120, for each frame that has passed we'll be
		// outputing 2 samples (assuming the timeline is running at 60hz).
		for (int i = 0; i < output->numChannels; i++)
		{
			for (int j = 0; j < output->length; j++)
			{
				output->channels[i][j] = sin(((float)output->startIndex + j) / 100.0f);
			}
		}
	}
}

int
YouTubeCHOP::getNumInfoCHOPChans()
{
	return ChanNames.size();
}

void
YouTubeCHOP::getInfoCHOPChan(int index,
	CHOP_InfoCHOPChan *chan)
{
	InfoChopIndex idx = (InfoChopIndex)index;
	
	if (ChanNames.find(idx) != ChanNames.end())
	{
		chan->name = (char*)ChanNames[idx].c_str();

		switch (idx)
		{
		case InfoChopIndex::ExecuteCount:
			chan->value = myExecuteCount;
			break;
		case InfoChopIndex::Samples:
			chan->value = 0;
			break;
		default:
			break;
		}
	}
	else
		chan->name = "n_a";
}

bool
YouTubeCHOP::getInfoDATSize(CHOP_InfoDATSize *infoSize)
{
	infoSize->rows = RowNames.size();
	infoSize->cols = 2;
	infoSize->byColumn = false;
	return true;
}

void
YouTubeCHOP::getInfoDATEntries(int index,
	int nEntries,
	CHOP_InfoDATEntries *entries)
{
	static char tempBuffer1[4096];
	static char tempBuffer2[4096];
	memset(tempBuffer1, 0, 4096);
	memset(tempBuffer2, 0, 4096);

	InfoDatIndex idx = (InfoDatIndex)index;

	if (RowNames.find(idx) != RowNames.end())
	{
		strcpy(tempBuffer1, RowNames[idx].c_str());
		switch (idx)
		{
		case InfoDatIndex::State:
			sprintf(tempBuffer2, "%s", getStatusString(status_).c_str());
			break;
		case InfoDatIndex::Binding:
			sprintf(tempBuffer2, "%s", parameters_.topFullPath_.c_str());
			break;
		default:
			break;
		}
	}

	entries->values[0] = tempBuffer1;
	entries->values[1] = tempBuffer2;
}

void YouTubeCHOP::updateParameters(const CHOP_InputArrays * inputArrays)
{
	TouchInputHelper<CHOP_InputArrays, TouchInputName> inputHelper(TouchInputs);

	inputHelper.getStringValue(inputArrays, TouchInputName::TopPath, parameters_.topFullPath_);
	const YouTubeTOP *top = loadTop(parameters_.topFullPath_);

	if (!top)
	{
		if (parameters_.topFullPath_ == "")
			status_ = NotBinded;
		else
			status_ = BindingError;
	}
	else
		status_ = Binded;
}

std::string YouTubeCHOP::getMyPath()
{
	std::stringstream fullPath(myNodeInfo->nodeFullPath);
	std::string segment;
	std::vector<std::string> seglist;
	std::stringstream path;

	while (std::getline(fullPath, segment, '/'))
	{
		if (segment == "")
			continue;

		seglist.push_back(segment);
		if (!fullPath.eof())
		{
			path << "/" << segment;
		}
	}

	return path.str();
}

const YouTubeTOP * YouTubeCHOP::loadTop(const std::string& topPath)
{
	// try whatever we have in full path as a full path...
	const YouTubeTOP *top = SharedData::getTop(topPath);

	if (!top)
	{ // now try using our path and topFullPath_ as TOP name...
		std::string fullTopPath = getMyPath() + "/" + topPath;
		top = SharedData::getTop(fullTopPath);
	}

	return top;
}

std::string YouTubeCHOP::getStatusString(Status s)
{
	switch (s)
	{
	case NotBinded:
		return "Not bound";
	case Binded:
		return "Bound";
	case BindingError:
		return "Binding error";
	default:
		break;
	}

	return "N/A";
}
