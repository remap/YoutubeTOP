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
#include "stream_controller.h"

using namespace std::placeholders;

enum class InfoDatIndex {
	State,
	Binding,
	Format
};

static std::map<InfoDatIndex, std::string> RowNames = {
	{ InfoDatIndex::State, "state" },
	{ InfoDatIndex::Binding, "binding" },
	{ InfoDatIndex::Format, "sampleFormat" }
};

enum class InfoChopIndex {
	ExecuteCount,
	Samples,
	SampleRate,
	nChannels
};

static std::map<InfoChopIndex, std::string> ChanNames = {
	{ InfoChopIndex::ExecuteCount, "executeCount" },
	{ InfoChopIndex::Samples, "nSamples" },	
	{ InfoChopIndex::SampleRate, "sampleRate" },
	{ InfoChopIndex::nChannels, "nChannels" }
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
status_(NotBinded), top_(nullptr), audioBuffer_(nullptr),
bufferSize_(0), bufferWriterPtr_(0), bufferReadPtr_(0)
{
	myExecuteCount = 0;
}

YouTubeCHOP::~YouTubeCHOP()
{
	if (audioBuffer_)
		free(audioBuffer_);
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
	info->numChannels = 2;

	if (!top_)
		info->sampleRate = 44100;
	else
	{
		ScopedLock lock(bufferAccess_);
		info->sampleRate = audioInfo_.rate_;
	}
	
	return true;
}

const char*
YouTubeCHOP::getChannelName(int index, void* reserved)
{
	return (index == 0 ? "chan1" : "chan2" ); 
}

void
YouTubeCHOP::execute(const CHOP_Output* output,
	const CHOP_InputArrays* inputs,
	void* reserved)
{
	updateParameters(inputs);
	myExecuteCount++;

	unsigned nSamplesPerChannel = output->length;
	unsigned nSamples = 2*nSamplesPerChannel;

	if (bufferWriterPtr_ - bufferReadPtr_ > nSamples*sizeof(uint16_t))
	{
		ScopedLock lock(bufferAccess_);

		for (int i = 0; i < nSamplesPerChannel; i++)
		{
			unsigned sampleIdx = (bufferReadPtr_%bufferSize_) / sizeof(uint16_t);
			output->channels[0][i] = audioBuffer_[sampleIdx++];
			output->channels[1][i] = audioBuffer_[sampleIdx];
			bufferReadPtr_ += 2*sizeof(uint16_t);
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
		case InfoChopIndex::SampleRate:
			chan->value = audioInfo_.rate_;
			break;
		case InfoChopIndex::nChannels:
			chan->value = audioInfo_.channels_;
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
		case InfoDatIndex::Format:
			if (top_)
			{
				sprintf(tempBuffer2, "%s", audioInfo_.format_);
			}
			break;
		default:
			break;
		}
	}

	entries->values[0] = tempBuffer1;
	entries->values[1] = tempBuffer2;
}

void YouTubeCHOP::onAudioData(vlc::StreamController::AudioData ad)
{
	ScopedLock lock(bufferAccess_);
	audioInfo_ = ad.audioInfo_;
	makeBuffer(ad.audioInfo_.rate_*sizeof(uint16_t));

	unsigned writeOffset = bufferWriterPtr_%bufferSize_;

	// audio buffer is cyclic
	if (writeOffset + ad.bufferSize_ > bufferSize_)
	{
		unsigned excess = (writeOffset + ad.bufferSize_ - bufferSize_);
		unsigned part1 = ad.bufferSize_ - excess, part2 = excess;
		memcpy((uint8_t*)audioBuffer_ + writeOffset, ad.buffer_, part1);
		memcpy((uint8_t*)audioBuffer_, (uint8_t*)ad.buffer_ +part1, part2);
	}
	else
		memcpy((uint8_t*)audioBuffer_ + writeOffset, ad.buffer_, ad.bufferSize_);

	bufferWriterPtr_ += ad.bufferSize_;
}

void YouTubeCHOP::makeBuffer(unsigned size)
{
	if (bufferSize_ < size || !audioBuffer_)
	{
		bufferSize_ = size;
		audioBuffer_ = (uint16_t*)realloc(audioBuffer_, size);
		bufferWriterPtr_ = 0;
	}
}

void YouTubeCHOP::freeBuffer()
{
	if (audioBuffer_)
	{
		free(audioBuffer_);
		bufferSize_ = 0;
		bufferWriterPtr_ = 0;
		bufferReadPtr_ = 0;
	}
}

void YouTubeCHOP::updateParameters(const CHOP_InputArrays * inputArrays)
{
	TouchInputHelper<CHOP_InputArrays, TouchInputName> inputHelper(TouchInputs);

	inputHelper.getStringValue(inputArrays, TouchInputName::TopPath, parameters_.topFullPath_);
	YouTubeTOP* top = loadTop(parameters_.topFullPath_);

	if (!top)
	{
		top_->registerAudioCallback(nullptr);
		top_ = nullptr;
		freeBuffer();
		if (parameters_.topFullPath_ == "")
			status_ = NotBinded;
		else
			status_ = BindingError;
	}
	else
	{
		if (top != top_)
		{
			if (top_) top_->registerAudioCallback(nullptr);
			top_ = top;
			top_->registerAudioCallback(std::bind(&YouTubeCHOP::onAudioData, this, _1));
		}

		status_ = Binded;
	}
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

YouTubeTOP * YouTubeCHOP::loadTop(const std::string& topPath)
{
	// try whatever we have in full path as a full path...
	YouTubeTOP *top = SharedData::getTop(topPath);

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
