//
//	youtube_top.h is part of YouTubeTOP.dll
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

#include <mutex>
#include <chrono>

#include "TOP_CPlusPlusBase.h"
#include "stream_controller.h"
#include "touch_helpers.h"

#define LIB_VERSION "1.1.0"

class YouTubeTOP : public TOP_CPlusPlusBase
{
public:
	typedef std::function<void(const vlc::StreamController::AudioData)> AudioCallback;

	YouTubeTOP(const TOP_NodeInfo *info);
	virtual ~YouTubeTOP();

	virtual void		getGeneralInfo(TOP_GeneralInfo *);
	virtual bool		getOutputFormat(TOP_OutputFormat*);


	virtual void		execute(const TOP_OutputFormatSpecs*,
								const TOP_InputArrays*,
								void* reserved);


	virtual int			getNumInfoCHOPChans();
	virtual void		getInfoCHOPChan(int index,
										TOP_InfoCHOPChan *chan);

	virtual bool		getInfoDATSize(TOP_InfoDATSize *infoSize);
	virtual void		getInfoDATEntries(int index,
											int nEntries,
											TOP_InfoDATEntries *entries);

	const char*			getWarningString();
	const char*			getErrorString();
	const char*			getInfoPopupString();

	std::string			getNodeFullPath() const;
	void				registerAudioCallback(AudioCallback callback);

private:
	typedef enum _Status {
		None,
		ReadyToRun,
		Running
	} Status;

	typedef enum _HandoverStatus {
		NoHandover,
		Initiated,
		Ready
	} HandoverStatus;

	typedef struct _Parameters {
		std::string currentUrl_, thumbnailUrl_;
		bool isLooping_;
		bool isPaused_;
		bool isNewSeekValue_;
		bool blackout_;
		float lastSeekPosition_;
		bool seamlessModeOn_;
		float lastSwitchCueValue_;
		bool switchCue_;
		bool isNewPlaybackSpeed_;
		float lastPlaybackSpeed_;
		bool isNewStartTime_;
		float lastStartTimeSec_;
		bool isNewEndTime_;
		float lastEndTimeSec_;
		bool thumbnailOn_;
	} Parameters;

	Status status_;
	HandoverStatus handoverStatus_;
	Parameters parameters_;

	// We don't need to store this pointer, but we do for the example.
	// The TOP_NodeInfo class store information about the node that's using
	// this instance of the class (like its name).
	const TOP_NodeInfo		*myNodeInfo;
	
	std::string libVersion_ = LIB_VERSION;
	FILE* logFile_;
	std::mutex frameBufferAcces_, thumbnailBufferAcces_;
	std::mutex audioCallbackMutex_;
	void* frameData_ = nullptr;
	void* thumbnailFrameData_ = nullptr;
	bool isFrameUpdated_;
	int startTimeMs_, endTimeSec_;
	bool needAdjustStartTimeHandover_, needAdjustStartTimeActive_;
	bool activeInfoStaled_, handoverInfoStaled_;
	int cookNextFrames_;
	bool thumbnailReady_;
	AudioCallback audioCallback_;

	unsigned texture_, thumbnail_;

	// In this example this value will be incremented each time the execute()
	// function is called, then passes back to the TOP 
	int	 myExecuteCount;

	class StreamControllerPair {
	public:
		StreamControllerPair(std::string name1, std::string name2):
		streamController_(name1), spareController_(name2){}

		vlc::StreamController* getFirst()
		{
			return &streamController_;
		}

		vlc::StreamController* getSecond()
		{
			return &spareController_;
		}

		const vlc::StreamController::Status* getFirstStatus()
		{
			return &streamController_.getStatus();
		}

		const vlc::StreamController::Status* getSecondsStatus()
		{
			return &spareController_.getStatus();
		}

	private:
		vlc::StreamController streamController_;
		vlc::StreamController spareController_;
	};

	StreamControllerPair streamControllers_;

	vlc::StreamController* activeController_;
	vlc::StreamController::Status activeControllerStatus_;
	vlc::StreamController* handoverController_;
	vlc::StreamController::Status handoverControllerStatus_;
	vlc::StreamController* thumbnailController_;
	vlc::StreamController::Status thumbnailControllerStatus_;

	bool videoFormatReady_;

	void onFrameRendering(const void* frameData, const void* userData);
	void onAudioData(const vlc::StreamController::AudioData ad, const void* userData);
	void onThumbnailRendering(const void* frameData, const void* userData);
	void initTexture();
	void initThumbnailTexture();
	void updateParameters(const TOP_InputArrays* arrays);
	void renderBlackFrame();

	void performTransition();
	void swapControllers();
	void swapControllers(vlc::StreamController** controller1, vlc::StreamController** controller2);
	vlc::StreamController* thumbnailController(){
		return thumbnailController_;
	}
	vlc::StreamController::Status thumbnailControllerStatus(){
		return thumbnailControllerStatus_;
	}

	FILE* initLogFile();
	void log(const char *fmt, ...);

	static std::string getStatusString(Status status);
	static std::string getHandoverStatusString(HandoverStatus status);
};
