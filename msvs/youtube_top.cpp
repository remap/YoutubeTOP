//
//	youtube_top.cpp is part of YouTubeTOP.dll
//
//	Copyright 2015 Regents of the University of California
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

#include "youtube_top.h"

#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <gl/gl.h>
#include <sstream>
#include <fstream>
#include <chrono>

#include "touch_helpers.h"
#include "shared_data.h"

using namespace vlc;
using namespace std::placeholders;

#define GetError( )\
{\
for ( GLenum Error = glGetError( ); ( GL_NO_ERROR != Error ); Error = glGetError( ) )\
{\
switch ( Error )\
{\
case GL_INVALID_ENUM:      throw std::exception("GL_INVALID_ENUM"); break;\
case GL_INVALID_VALUE:     throw std::exception("GL_INVALID_VALUE"     ); break;\
case GL_INVALID_OPERATION: throw std::exception("GL_INVALID_OPERATION" ); break;\
case GL_OUT_OF_MEMORY:     throw std::exception("GL_OUT_OF_MEMORY"     ); break;\
default:                                                                              break;\
}\
}\
}


static int nTOPInstances = 0;

/**
 * This enum identifies output DAT's different fields
 * The output DAT is a table that contains two 
 * columns: name (identified by this enum) and value 
 * (either float or string)
 */
enum class InfoDatIndex {
	ExecuteCount,
	URL,
	Looping,
	Paused,
	State,
	TopStatus,
	Duration,
	PlaybackProgress,
	BufferingProgress,
	VideoWidth,
	VideoHeight,
	HandoverStatus,
	HandoverState,
	SwitchOnCue,
	SwitchCue,
	PlaybackSpeed,
	StarTimeSec,
	EndTimeSec,
	Blackout,
	Thumbnail,
	ThumbnailOn,
	LibVersion,
	FPS,
	CurrentTime
};

/**
* This enum identifies output DAT's different fields
* The output DAT is a table that contains two
* columns : name(identified by this enum) and value
* (either float or string)
*/
enum class InfoChopIndex {
	ExecuteCount,
	Looping,
	Paused,
	Duration,
	PlaybackProgress,
	BufferingProgress,
	VideoWidth,
	VideoHeight,
	HandoverStatus,
	SwitchOnCue,
	SwitchCue,
	PlaybackSpeed,
	StarTimeSec,
	EndTimeSec,
	Blackout,
	ThumbnailOn,
	FPS,
	CurrentTime,
	nInstances
};

/**
 * This maps output DAT's field onto their textual representation
 * (table caption)
 */
static std::map<InfoDatIndex, std::string> RowNames = {
		{ InfoDatIndex::URL, "URL" },
		{ InfoDatIndex::State, "state" },
		{ InfoDatIndex::TopStatus, "TOPstatus" },
		{ InfoDatIndex::HandoverState, "handoverState" },
		{ InfoDatIndex::Thumbnail, "thumbnail" },
		{ InfoDatIndex::LibVersion, "libVersion" }
};

/**
* This maps output CHOP's channel names
*/
static std::map<InfoChopIndex, std::string> ChanNames = {
	{ InfoChopIndex::ExecuteCount, "executeCount" },
	{ InfoChopIndex::Looping, "isLooping" },
	{ InfoChopIndex::Paused, "isPaused" },
	{ InfoChopIndex::Duration, "duration" },
	{ InfoChopIndex::PlaybackProgress, "playbackProgress" },
	{ InfoChopIndex::BufferingProgress, "bufferingProgress" },
	{ InfoChopIndex::VideoWidth, "videoWidth" },
	{ InfoChopIndex::VideoHeight, "videoHeight" },
	{ InfoChopIndex::HandoverStatus, "handover" },
	{ InfoChopIndex::SwitchOnCue, "switchOnCue" },
	{ InfoChopIndex::SwitchCue, "switchCue" },
	{ InfoChopIndex::PlaybackSpeed, "playbackSpeed" },
	{ InfoChopIndex::StarTimeSec, "startTime" },
	{ InfoChopIndex::EndTimeSec, "endTime" },
	{ InfoChopIndex::Blackout, "blackout" },
	{ InfoChopIndex::ThumbnailOn, "thumbnailOn" },
	{ InfoChopIndex::FPS, "framerate" },
	{ InfoChopIndex::CurrentTime, "currentTime" },
	{ InfoChopIndex::nInstances, "nInstances" }
};

/**
 * This enum identifies all TouchDesigner's inputs that are
 * used in this CPlusPlusTOP
 */
enum class TouchInputName {
	URL,
	Pause,
	Loop,
	SeekPosition,
	SwitchOnCue,
	SwitchCue,
	PlaybackSpeed,
	StartTime,
	EndTime,
	Blackout,
	Thumbnail,
	ThumbnailOn
};

/**
 * This maps touch inputs to their textual names in TouchDesigner
 */
 static std::map<TouchInputName, TouchInput> TouchInputs = {
		 { TouchInputName::URL, { "string0", 0, 0 } },
		 { TouchInputName::Loop, { "value0", 0, 0 } },
		 { TouchInputName::Pause, { "value0", 0, 1 } },
		 { TouchInputName::Blackout, { "value0", 0, 2 } },
		 { TouchInputName::SeekPosition, { "value2", 2, 0 } },
		 { TouchInputName::SwitchOnCue, { "value3", 3, 0 } },
		 { TouchInputName::SwitchCue, { "value3", 3, 1 } },
		 { TouchInputName::PlaybackSpeed, { "value4", 4, 0 } },
		 { TouchInputName::StartTime, { "value5", 5, 0 } },
		 { TouchInputName::EndTime, { "value5", 5, 1 } },
		 { TouchInputName::Thumbnail, { "string1", 1, 0 } },
		 { TouchInputName::ThumbnailOn, { "value6", 6, 0 } }
};

int createVideoTexture(unsigned width, unsigned height, void *frameBuffer);
void renderTexture(GLuint texId, unsigned width, unsigned height, void* data);
bool fileExist(const char *fileName);

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

DLLEXPORT
int
GetTOPAPIVersion(void)
{
	// Always return TOP_CPLUSPLUS_API_VERSION in this function.
	return TOP_CPLUSPLUS_API_VERSION;
}

DLLEXPORT
TOP_CPlusPlusBase*
CreateTOPInstance(const TOP_NodeInfo *info)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per TOP that is using the .dll
	return new YouTubeTOP(info);
}

DLLEXPORT
void
DestroyTOPInstance(TOP_CPlusPlusBase *instance)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the TOP using that instance is deleted, or
	// if the TOP loads a different DLL
	delete (YouTubeTOP*)instance;
}

};

#pragma mark - static
std::string YouTubeTOP::getStatusString(YouTubeTOP::Status status)
{
	switch (status)
	{
	case None:
		return "None";
	case ReadyToRun:
		return "ReadyToRun";
	case Running:
		return "Running";
	default:
		break;
	}

	return "N/A";
}

std::string YouTubeTOP::getHandoverStatusString(YouTubeTOP::HandoverStatus status)
{
	switch (status)
	{
	case NoHandover:
		return "No handover";
	case Initiated:
		return "Initiated";
	case Ready:
		return "Ready";
	default:
		break;
	}

	return "N/A";
}

#pragma mark - public
YouTubeTOP::YouTubeTOP(const TOP_NodeInfo *info) : 
streamControllers_("main", "spare"),
myNodeInfo(info), 
videoFormatReady_(false),
status_(Status::None), 
handoverStatus_(HandoverStatus::NoHandover), 
parameters_({ "", "", false, false, false, false, 0., 0., false, false, 0., false, 0., false, 0., false}), 
activeController_(streamControllers_.getFirst()),
handoverController_(streamControllers_.getSecond()),
thumbnailController_(new vlc::StreamController("thumbnail")),
needAdjustStartTimeActive_(false),
needAdjustStartTimeHandover_(false),
activeInfoStaled_(false),
handoverInfoStaled_(false),
cookNextFrames_(1),
thumbnailReady_(false)
{
	SharedData::addTop(this);

	myExecuteCount = 0;
	nTOPInstances++;
	//logFile_ = initLogFile();
}

YouTubeTOP::~YouTubeTOP()
{
	SharedData::removeTop(this);
	nTOPInstances--;
}

void
YouTubeTOP::getGeneralInfo(TOP_GeneralInfo *ginfo)
{
	//log("getGeneralInfo() cook next %d", cookNextFrames_);
	// Uncomment this line if you want the TOP to cook every frame even
	// if none of it's inputs/parameters are changing.
	ginfo->cookEveryFrame = true; // cookNextFrames_ > 0; // !parameters_.isPaused_ && !thumbnailReady_;
	ginfo->cookEveryFrameIfAsked = true;
	activeControllerStatus_ = activeController_->getStatus();
	handoverControllerStatus_ = handoverController_->getStatus();
	thumbnailControllerStatus_ = thumbnailController_->getStatus();

	if (status_ > None)
	{
		ginfo->clearBuffers = false;
	}

	cookNextFrames_--;
}

bool
YouTubeTOP::getOutputFormat(TOP_OutputFormat *format)
{
	//log("getOutputFormat()");
	// In this function we could assign variable values to 'format' to specify
	// the pixel format/resolution etc that we want to output to.
	// If we did that, we'd want to return true to tell the TOP to use the settings we've
	// specified.
	// In this example we'll return false and use the TOP's settings

	{
		if (activeControllerStatus_.isVideoInfoReady_)
		{
			if (!activeInfoStaled_)
			{
				log("active video info ready");

				activeInfoStaled_ = true;

				if (status_ == None)
				{
					log("status None. creating texture for active");

					initTexture();
					status_ = ReadyToRun;
				}
			}

			format->width = (int)activeControllerStatus_.videoInfo_.width_;
			format->height = (int)activeControllerStatus_.videoInfo_.height_;
			format->aspectX = (float)activeControllerStatus_.videoInfo_.width_;
			format->aspectY = (float)activeControllerStatus_.videoInfo_.height_;
		}

		if (needAdjustStartTimeActive_ &&
			activeControllerStatus_.isVideoInfoReady_)
		{
			if (startTimeMs_ < activeControllerStatus_.videoInfo_.totalTime_)
			{
				log("seek active to %d. buffer %.2f", startTimeMs_, activeControllerStatus_.videoInfo_.bufferLevel_);

				activeController_->seekMs(startTimeMs_);
			}
			else
				log("startTime (%d) exceeds video length (%d). ignore seeking for active", startTimeMs_, activeControllerStatus_.videoInfo_.totalTime_);

			needAdjustStartTimeActive_ = false;
		}


		if (handoverControllerStatus_.isVideoInfoReady_ &&
			!handoverInfoStaled_)
		{
			log("handover video info ready. pausing");

			handoverInfoStaled_ = true;
			handoverController_->pause(true);
		}

		bool adjustedHandover = false;

		if (needAdjustStartTimeHandover_ &&
			handoverControllerStatus_.isVideoInfoReady_)
		{
			if (startTimeMs_ < handoverControllerStatus_.videoInfo_.totalTime_)
			{
				log("seek handover to %d. buffr %.2f", startTimeMs_, handoverControllerStatus_.videoInfo_.bufferLevel_);

				handoverController_->seekMs(startTimeMs_);
				adjustedHandover = true;
			}
			else
				log("startTime (%d) exceeds video length (%d). ignore seeking for handover", startTimeMs_, handoverControllerStatus_.videoInfo_.totalTime_);

			needAdjustStartTimeHandover_ = false;
		}


		if (handoverStatus_ == HandoverStatus::Initiated &&
			handoverControllerStatus_.videoInfo_.bufferLevel_ >= 90 &&
			!adjustedHandover)
		{
			log("finishing up handover. buffer %.2f", handoverControllerStatus_.videoInfo_.bufferLevel_);

			handoverStatus_ = HandoverStatus::Ready;
		}
	}
	
	// if thumbnail is on, set format
	if (thumbnailControllerStatus_.isVideoInfoReady_ &&
		parameters_.thumbnailOn_ &&
		!thumbnailReady_)
	{
		status_ = Running;
		activeController_->pause(true);
		initThumbnailTexture();

		log("thumbnail info ready. swapped with active controller");
	}

	if (thumbnailReady_)
	{
		format->width = (int)thumbnailControllerStatus_.videoInfo_.width_;
		format->height = (int)thumbnailControllerStatus_.videoInfo_.height_;
		format->aspectX = (float)thumbnailControllerStatus_.videoInfo_.width_;
		format->aspectY = (float)thumbnailControllerStatus_.videoInfo_.height_;
	}

	return true;
}

void
YouTubeTOP::execute(const TOP_OutputFormatSpecs* outputFormat, const TOP_InputArrays* arrays, void* reserved)
{
	updateParameters(arrays);
	//log("execute()");

	myExecuteCount++;

	bool needLoad = false;

	needLoad = (parameters_.currentUrl_ != activeControllerStatus_.videoUrl_) && (parameters_.currentUrl_ != handoverControllerStatus_.videoUrl_);

	if (parameters_.isNewStartTime_)
	{
		parameters_.isNewStartTime_ = false;
		startTimeMs_ = 0;
		startTimeMs_ = (int)round(parameters_.lastStartTimeSec_ * 1000.);
		needAdjustStartTimeActive_ = (startTimeMs_ > activeControllerStatus_.videoInfo_.currentTime_) || !activeControllerStatus_.isVideoInfoReady_;
		needAdjustStartTimeHandover_ = true;

		log("new start time %d adjust active %d adjust handover %d", startTimeMs_, needAdjustStartTimeActive_, needAdjustStartTimeHandover_);
	}

	if (parameters_.thumbnailUrl_ != thumbnailController()->getStatus().videoUrl_)
	{
		if (parameters_.thumbnailUrl_ == "")
			thumbnailController()->stop();
		else
		{
			thumbnailController()->play(parameters_.thumbnailUrl_, 
				std::bind(&YouTubeTOP::onThumbnailRendering, this, _1, _2),
				nullptr,
				thumbnailController());
			log("requested thumbnail - %s", parameters_.thumbnailUrl_.c_str());
		}
	}

	if (!parameters_.thumbnailOn_)
	{
		if (thumbnailReady_)
		{
			log("thumbnail disabled. swapping thumbnail and active controllers");

			thumbnailReady_ = false;
		}
	}
	else
	{
		if (!thumbnailReady_)
		{
			thumbnailController()->seek(0);
			thumbnailController()->play();
			cookNextFrames_ = INT_MAX;
		}
		else
		{
			status_ = ReadyToRun;
			thumbnailController()->pause(true);
			cookNextFrames_ = 0;
		}
	}

	if (needLoad)
	{
		if (parameters_.currentUrl_ == "")
		{
			status_ = Status::None;
			handoverStatus_ = HandoverStatus::NoHandover;
			isFrameUpdated_ = false;
			activeController_->stop();
			handoverController_->stop();
			renderBlackFrame();
			cookNextFrames_ = 0;

			log("empty url - rendering black frame");
		}
		else
		{
			if (status_ == Running)
			{
				if (handoverStatus_ != Initiated ||
					(handoverStatus_ == Initiated && parameters_.currentUrl_ != handoverControllerStatus_.videoUrl_))
				{
					handoverStatus_ = Initiated;
					handoverController_->play(parameters_.currentUrl_,
						std::bind(&YouTubeTOP::onFrameRendering, this, _1, _2),
						std::bind(&YouTubeTOP::onAudioData, this, _1, _2),
						handoverController_);
					handoverInfoStaled_ = false;

					log("initiated handover for URL %s", parameters_.currentUrl_.c_str());
				}
			}
			else
			{
				status_ = Status::None;
				isFrameUpdated_ = false;
				activeController_->play(parameters_.currentUrl_, 
					std::bind(&YouTubeTOP::onFrameRendering, this, _1, _2), 
					std::bind(&YouTubeTOP::onAudioData, this, _1, _2),
					activeController_);
				handoverController_->play(parameters_.currentUrl_,
					std::bind(&YouTubeTOP::onFrameRendering, this, _1, _2), 
					std::bind(&YouTubeTOP::onAudioData, this, _1, _2),
					handoverController_);
				needAdjustStartTimeActive_ = (startTimeMs_ != 0);
				activeInfoStaled_ = false;
				handoverInfoStaled_ = false;
				cookNextFrames_ = INT_MAX;

				log("initiated playback for active and handover: %s", parameters_.currentUrl_.c_str());
			}
			
			needAdjustStartTimeHandover_ = (startTimeMs_ != 0);

			log("need adjust active: %d handover %d", needAdjustStartTimeActive_, needAdjustStartTimeHandover_);
		}

		return;
	}
	else if (!thumbnailReady_ && !parameters_.thumbnailOn_)
	{
		bool canSwitch = parameters_.seamlessModeOn_ || 
						!parameters_.seamlessModeOn_ && parameters_.switchCue_;

		if (handoverStatus_ == Ready && canSwitch)
		{
			log("handover ready. switching...");

			performTransition();
			handoverInfoStaled_ = false;
			needAdjustStartTimeHandover_ = true;
		}

		if (status_ > None)
		{
			activeController_->pause(parameters_.isPaused_);

			if (parameters_.isNewSeekValue_)
			{
				parameters_.isNewSeekValue_ = false;
				activeController_->seek(parameters_.lastSeekPosition_);
			}

			if (parameters_.isNewPlaybackSpeed_)
			{
				parameters_.isNewPlaybackSpeed_ = false;
				activeController_->setPlaybackSpeed(parameters_.lastPlaybackSpeed_);
				handoverController_->setPlaybackSpeed(parameters_.lastPlaybackSpeed_);
			}

			status_ = (parameters_.isPaused_) ? ReadyToRun : Running;
			cookNextFrames_ = (status_ == ReadyToRun) ? 0 : INT_MAX;

			if (status_ == Running)
			{
				switch (activeControllerStatus_.state_)
				{
				case libvlc_Error:{
					log("player has encountered error");
					status_ = ReadyToRun;
					cookNextFrames_ = 0;
				}
					break;
				case libvlc_Ended:
				{
					if (parameters_.isLooping_)
					{
						log("active ended");

						if (handoverControllerStatus_.isVideoInfoReady_)
						{
							log("performing transition...");

							performTransition();
							handoverInfoStaled_ = false;
							needAdjustStartTimeHandover_ = true;
							activeController_->pause(parameters_.isPaused_);
						}
						else
							log("ooops! handover is not ready. postponing...");
					}
				}
					break;

				default:
					break;
				}
			} // Running

			if (status_ == Running)
			{
				if (parameters_.blackout_)
					renderBlackFrame();
				else
				{
					ScopedLock lock(frameBufferAcces_);
					if (isFrameUpdated_)
					{
						isFrameUpdated_ = false;
						renderTexture(texture_, activeControllerStatus_.videoInfo_.width_, activeControllerStatus_.videoInfo_.height_, frameData_);
					}
				}
			}
		} // status > None
		else
		{
			if (activeControllerStatus_.state_ == libvlc_Error)
			{
				log("player encountered error before URL was opened.");
				cookNextFrames_ = 0;
			}
		}
	}
	else
	{
		if (thumbnailReady_)
		{
			if (parameters_.blackout_)
				renderBlackFrame();
			else
			{
				ScopedLock lock(thumbnailBufferAcces_);
				renderTexture(thumbnail_, thumbnailControllerStatus().videoInfo_.width_, thumbnailControllerStatus().videoInfo_.height_, thumbnailFrameData_);
			}
		}
	}
}

int
YouTubeTOP::getNumInfoCHOPChans()
{
	return ChanNames.size();
}

void
YouTubeTOP::getInfoCHOPChan(int index, TOP_InfoCHOPChan *chan)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	InfoChopIndex idx = (InfoChopIndex)index;
	if (ChanNames.find(idx) != ChanNames.end())
	{
		chan->name = (char*)ChanNames[idx].c_str();

		switch (idx)
		{
		case InfoChopIndex::ExecuteCount:
			chan->value = myExecuteCount;
			break;
		case InfoChopIndex::Looping:
			chan->value = (float)parameters_.isLooping_;
			break;
		case InfoChopIndex::Paused:
			chan->value = (float)parameters_.isPaused_;
			break;
		case InfoChopIndex::Duration:
			chan->value = (float)activeControllerStatus_.videoInfo_.totalTime_ / 1000.;
			break;
		case InfoChopIndex::PlaybackProgress:
			chan->value = (float)activeControllerStatus_.videoInfo_.currentTime_ / (float)activeControllerStatus_.videoInfo_.totalTime_;
			break;
		case InfoChopIndex::BufferingProgress:
			chan->value = activeControllerStatus_.videoInfo_.bufferLevel_;
			break;
		case InfoChopIndex::VideoWidth:
			chan->value = activeControllerStatus_.videoInfo_.width_;
			break;
		case InfoChopIndex::VideoHeight:
			chan->value = activeControllerStatus_.videoInfo_.height_;
			break;
		case InfoChopIndex::HandoverStatus:
			chan->value = handoverControllerStatus_.videoInfo_.bufferLevel_;
			break;
		case InfoChopIndex::SwitchOnCue:
			chan->value = !parameters_.seamlessModeOn_;
			break;
		case InfoChopIndex::SwitchCue:
			chan->value = parameters_.switchCue_;
			break;
		case InfoChopIndex::PlaybackSpeed:
			chan->value = parameters_.lastPlaybackSpeed_;
			break;
		case InfoChopIndex::StarTimeSec:
			chan->value = parameters_.lastStartTimeSec_;
			break;
		case InfoChopIndex::EndTimeSec:
			chan->value = parameters_.lastEndTimeSec_;
			break;
		case InfoChopIndex::Blackout:
			chan->value = parameters_.blackout_;
			break;
		case InfoChopIndex::ThumbnailOn:
			chan->value = parameters_.thumbnailOn_;
			break;
		case InfoChopIndex::FPS:
			chan->value = activeControllerStatus_.videoInfo_.fps_;
			break;
		case InfoChopIndex::CurrentTime:
			chan->value = (float)activeControllerStatus_.videoInfo_.currentTime_ / 1000.;
			break;
		case InfoChopIndex::nInstances:
			chan->value = nTOPInstances;
			break;
		default:
			chan->value = -1;
			break;
		}
	}
	else
		chan->name = "n_a";
}

bool		
YouTubeTOP::getInfoDATSize(TOP_InfoDATSize *infoSize)
{
	infoSize->rows = RowNames.size();
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
YouTubeTOP::getInfoDATEntries(int index, int nEntries, TOP_InfoDATEntries *entries)
{
	// It's safe to use static buffers here because Touch will make it's own
	// copies of the strings immediately after this call returns
	// (so the buffers can be reuse for each column/row)
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
		case InfoDatIndex::LibVersion:
			sprintf(tempBuffer2, "%s", libVersion_.c_str());
			break;
		case InfoDatIndex::TopStatus:
		{
			std::string status = YouTubeTOP::getStatusString(status_);
			sprintf(tempBuffer2, "%s", status.c_str());
		}
			break;
		case InfoDatIndex::State:
		{
			std::string state = StreamController::getStateString(activeControllerStatus_.state_);
			sprintf(tempBuffer2, "%s", state.c_str());
		}
			break;
		case InfoDatIndex::URL:
			sprintf(tempBuffer2, "%s", activeControllerStatus_.videoUrl_.c_str());
			break;

		case InfoDatIndex::HandoverState:
		{
			std::string state = getHandoverStatusString(handoverStatus_);
			sprintf(tempBuffer2, "%s", state.c_str());
		}
			break;

		case InfoDatIndex::Thumbnail:
			sprintf(tempBuffer2, "%s", parameters_.thumbnailUrl_.c_str());
			break;

		default:
			sprintf(tempBuffer2, "%s", "unknown");
			break;
		}
	}

	entries->values[0] = tempBuffer1;
	entries->values[1] = tempBuffer2;
}

const char*
YouTubeTOP::getWarningString()
{
	return (activeControllerStatus_.warningMessage_ != "") ? activeControllerStatus_.warningMessage_.c_str() : NULL;
}

const char*
YouTubeTOP::getErrorString()
{
	return (activeControllerStatus_.errorMessage_ != "") ? activeControllerStatus_.errorMessage_.c_str() : NULL;
}

const char*
YouTubeTOP::getInfoPopupString()
{
	return (activeControllerStatus_.infoString_ != "") ? activeControllerStatus_.infoString_.c_str() : NULL;
}

std::string YouTubeTOP::getNodeFullPath() const
{
	return std::string(myNodeInfo->nodeFullPath);
}

void YouTubeTOP::registerAudioCallback(AudioCallback callback)
{
	ScopedLock lock(audioCallbackMutex_);

	audioCallback_ = callback;
}

bool YouTubeTOP::getIsPlaying()
{
	return (status_ == Running);
}

#pragma mark - private
void
YouTubeTOP::onFrameRendering(const void* frameData, const void* userData)
{
	if (userData == activeController_)
	{
		if (status_ == Running && frameData_)
		{
			//log("copy frame");
			ScopedLock lock(frameBufferAcces_);
			memcpy(frameData_, frameData, activeControllerStatus_.videoInfo_.frameSize_);
			isFrameUpdated_ = true;
		}
	}
}

void YouTubeTOP::onAudioData(const StreamController::AudioData ad, const void * userData)
{	
#if 1
	if (userData == activeController_)
	{
		ScopedLock lock(audioCallbackMutex_);

		if (audioCallback_)
			audioCallback_(ad);
	}
#endif
}

void 
YouTubeTOP::onThumbnailRendering(const void* frameData, const void* userData)
{
	if (parameters_.thumbnailOn_ && thumbnail_)
	{
		ScopedLock lock(thumbnailBufferAcces_);
		memcpy(thumbnailFrameData_, frameData, thumbnailControllerStatus().videoInfo_.frameSize_);
		thumbnailReady_ = true;
	}
}

void 
YouTubeTOP::initTexture()
{
	ScopedLock lock(frameBufferAcces_);

	if (frameData_)
	{
		log("deallocating texture data");

		free(frameData_);
	}

	frameData_ = malloc(activeControllerStatus_.videoInfo_.frameSize_);
	memset(frameData_, 0, activeControllerStatus_.videoInfo_.frameSize_);

	log("new texture allocated - %d bytes (%dX%d)", activeControllerStatus_.videoInfo_.frameSize_, 
		activeControllerStatus_.videoInfo_.width_, activeControllerStatus_.videoInfo_.height_);

	if (glIsTexture(texture_))
	{
		log("delete texture");

		glDeleteTextures(1, (const GLuint*)&texture_);
		GetError();
		texture_ = 0;
	}

	log("creating new texture (%dX%d)...", activeControllerStatus_.videoInfo_.width_, activeControllerStatus_.videoInfo_.height_);
	texture_ = createVideoTexture(activeControllerStatus_.videoInfo_.width_, activeControllerStatus_.videoInfo_.height_, frameData_);
	log("new texture created");
}

void
YouTubeTOP::initThumbnailTexture()
{

	if (thumbnailFrameData_)
	{
		log("deallocating thumbnail texture data");

		free(thumbnailFrameData_);
	}

	thumbnailFrameData_ = malloc(thumbnailControllerStatus().videoInfo_.frameSize_);
	memset(thumbnailFrameData_, 0, thumbnailControllerStatus().videoInfo_.frameSize_);

	log("new thumbnail texture allocated - %d bytes (%dX%d)", thumbnailControllerStatus().videoInfo_.frameSize_,
		thumbnailControllerStatus().videoInfo_.width_, thumbnailControllerStatus().videoInfo_.height_);

	if (glIsTexture(thumbnail_))
	{
		log("delete thumbnail texture");

		glDeleteTextures(1, (const GLuint*)&thumbnail_);
		GetError();
		thumbnail_ = 0;
	}

	log("creating new texture (%dX%d)...", thumbnailControllerStatus_.videoInfo_.width_, thumbnailControllerStatus_.videoInfo_.height_);
	thumbnail_ = createVideoTexture(thumbnailControllerStatus_.videoInfo_.width_, thumbnailControllerStatus_.videoInfo_.height_, thumbnailFrameData_);
	log("new texture created");
}

void
YouTubeTOP::updateParameters(const TOP_InputArrays* arrays)
{
	TouchInputHelper<TOP_InputArrays, TouchInputName> inputHelper(TouchInputs);

	inputHelper.getStringValue(arrays, TouchInputName::URL, parameters_.currentUrl_);
	inputHelper.getStringValue(arrays, TouchInputName::Thumbnail, parameters_.thumbnailUrl_);
	inputHelper.getBoolValue(arrays, TouchInputName::Pause, parameters_.isPaused_);
	inputHelper.getBoolValue(arrays, TouchInputName::Loop, parameters_.isLooping_);
	inputHelper.getBoolValue(arrays, TouchInputName::Blackout, parameters_.blackout_);
	inputHelper.getBoolValue(arrays, TouchInputName::ThumbnailOn, parameters_.thumbnailOn_);
	inputHelper.updateFloatValue(arrays, TouchInputName::SeekPosition, parameters_.isNewSeekValue_, parameters_.lastSeekPosition_);
	inputHelper.updateFloatValue(arrays, TouchInputName::PlaybackSpeed, parameters_.isNewPlaybackSpeed_, parameters_.lastPlaybackSpeed_);
	inputHelper.updateFloatValue(arrays, TouchInputName::StartTime, parameters_.isNewStartTime_, parameters_.lastStartTimeSec_);
	inputHelper.updateFloatValue(arrays, TouchInputName::EndTime, parameters_.isNewEndTime_, parameters_.lastEndTimeSec_);

	bool switchOnCue = false;
	inputHelper.getBoolValue(arrays, TouchInputName::SwitchOnCue, switchOnCue);
	parameters_.seamlessModeOn_ = !switchOnCue;

	if (!parameters_.seamlessModeOn_)
		inputHelper.updateFloatValue(arrays, TouchInputName::SwitchCue, parameters_.switchCue_, parameters_.lastSwitchCueValue_);
	else
		parameters_.switchCue_ = 0;
}

void
YouTubeTOP::renderBlackFrame()
{
	if (frameData_)
	{
		ScopedLock lock(frameBufferAcces_);
		memset(frameData_, 0, activeControllerStatus_.videoInfo_.frameSize_);
		renderTexture(texture_, activeControllerStatus_.videoInfo_.width_, activeControllerStatus_.videoInfo_.height_, frameData_);
	}
}

void
YouTubeTOP::performTransition()
{
	int oldWidth = activeControllerStatus_.videoInfo_.width_;
	int oldHeight = activeControllerStatus_.videoInfo_.height_;

	parameters_.switchCue_ = false;
	handoverStatus_ = HandoverStatus::NoHandover;
	activeController_->stop();
	swapControllers();

	if (activeControllerStatus_.videoInfo_.width_ != oldWidth ||
		activeControllerStatus_.videoInfo_.height_ != oldHeight)
		initTexture();

	// set spare controller to prebuffer current video
	handoverController_->play(parameters_.currentUrl_, 
		std::bind(&YouTubeTOP::onFrameRendering, this, _1, _2), 
		std::bind(&YouTubeTOP::onAudioData, this, _1, _2),
		handoverController_);
}

void
YouTubeTOP::swapControllers()
{
	if (activeController_ == streamControllers_.getFirst())
	{
		activeController_ = streamControllers_.getSecond();
		handoverController_ = streamControllers_.getFirst();
	}
	else
	{
		activeController_ = streamControllers_.getFirst();
		handoverController_ = streamControllers_.getSecond();
	}

	activeControllerStatus_ = activeController_->getStatus();
	handoverControllerStatus_ = handoverController_->getStatus();
}

void
YouTubeTOP::swapControllers(vlc::StreamController** controller1, vlc::StreamController** controller2)
{
	void *tmp = *controller1;
	*controller1 = *controller2;
	*controller2 = (vlc::StreamController*)tmp;
}

FILE*
YouTubeTOP::initLogFile()
{
	std::string fileName = "youtubeTop";
	int id = 0;
	std::stringstream ss;

	do{
		id++;
		ss.str("");
		ss << fileName << id << ".log";
	} while (fileExist(ss.str().c_str()));

	return fopen(ss.str().c_str(), "w+");
}

void 
YouTubeTOP::log(const char *fmt, ...)
{
	//FILE* outStream = logFile_;
	//std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	//std::chrono::duration<double> seconds = now.time_since_epoch();

	//va_list args;
	//va_start(args, fmt);

	//fprintf(outStream, "%f\t", seconds);
	//vfprintf(outStream, fmt, args);
	//va_end(args);
	//fprintf(outStream, "\n");
	//fflush(outStream);
}

//********************************************************************************
#pragma mark - functions
bool fileExist(const char *fileName)
{
	std::ifstream infile(fileName);
	return infile.good();
}

int
createVideoTexture(unsigned width, unsigned height, void *frameBuffer)
{
	int texture = 0;

	glGenTextures(1, (GLuint*)&texture);
	GetError();

	glBindTexture(GL_TEXTURE_2D, texture);
	GetError();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	GetError();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	GetError();

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	GetError();

	return texture;
}

void
renderTexture(GLuint texId, unsigned width, unsigned height, void* data)
{
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texId);
	glLoadIdentity();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glBegin(GL_QUADS);
	// the reason why texture coordinates are weird - the texture is flipped horizontally
	glTexCoord2f(0., 1.); glVertex2i(0, 0);
	glTexCoord2f(1., 1.); glVertex2i(width, 0);
	glTexCoord2f(1., 0.); glVertex2i(width, height);
	glTexCoord2f(0., 0.); glVertex2i(0, height);
	glEnd();
}
