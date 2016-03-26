//
//	youtube_chop.h is part of YouTubeTOP.dll
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

#ifndef __youtube_chop_h__
#define __youtube_chop_h__

#include <string>
#include "CHOP_CPlusPlusBase.h"
#include "stream_controller.h"

/*
This class works in conjunction with YouTubeTOP. It retrieves audio data from
corresponding YouTubeTOP that streams Youtube video and outputs it as one or two
channels (depending on video) into TouchDesigner for further processing. One should 
create YouTubeTOP instance first in order to get audio channels. All controls are 
performed on corresponding TOP class.
*/

class YouTubeTOP;

// To get more help about these functions, look at CHOP_CPlusPlusBase.h
class YouTubeCHOP : public CHOP_CPlusPlusBase
{
public:
	YouTubeCHOP(const CHOP_NodeInfo *info);
	virtual ~YouTubeCHOP();

	virtual void		getGeneralInfo(CHOP_GeneralInfo *);
	virtual bool		getOutputInfo(CHOP_OutputInfo*);
	virtual const char*	getChannelName(int index, void* reserved);

	virtual void		execute(const CHOP_Output*,
		const CHOP_InputArrays*,
		void* reserved);


	virtual int			getNumInfoCHOPChans();
	virtual void		getInfoCHOPChan(int index,
		CHOP_InfoCHOPChan *chan);

	virtual bool		getInfoDATSize(CHOP_InfoDATSize *infoSize);
	virtual void		getInfoDATEntries(int index,
		int nEntries,
		CHOP_InfoDATEntries *entries);
private:
	typedef enum _Status {
		NotBinded,
		Binded,
		BindingError
	} Status;

	typedef struct _Parameters {
		std::string topFullPath_;
	} Parameters;

	const CHOP_NodeInfo		*myNodeInfo;
	int						 myExecuteCount;

	Status status_;
	Parameters parameters_;
	vlc::StreamController::Status::AudioInfo audioInfo_;
	unsigned bufferSize_, bufferWriterPtr_, bufferReadPtr_;
	unsigned bufferSampleSize_;
	//unsigned nWroteBytes_, nReadBytes_;
	uint16_t* audioBuffer_;
	YouTubeTOP* top_;
	std::mutex bufferAccess_;

	void onAudioData(vlc::StreamController::AudioData ad);

	void makeBuffer(unsigned size);
	void freeBuffer();

	void updateParameters(const CHOP_InputArrays* inputArrays);
	std::string getMyPath();
	YouTubeTOP* loadTop(const std::string& topPath);

	static std::string getStatusString(Status s);
};

#endif // !__youtube_chop_h__
