//
//	stream_controller.h is part of YouTubeTOP.dll
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

#ifndef __streamcontroller_h__
#define __streamcontroller_h__

#include <string>
#include <memory>
#include <functional>
#include <mutex>

#include <vlc/vlc.h>

typedef std::lock_guard<std::mutex> ScopedLock;

namespace vlc {
	namespace internal {
		struct StreamControllerPrivate;
	}

	class StreamController {
	public:
		class Status {
		public:
			struct VideoInfo {
				unsigned width_, height_;
				int64_t totalTime_, currentTime_;
				double bufferLevel_;
				size_t frameSize_;
				double fps_;
			};

			struct AudioInfo {
				unsigned rate_, channels_;
				char format_[4];
			};

			libvlc_state_t state_;
			std::string videoUrl_;
			bool isVideoInfoReady_, isAudioInfoReady_;
			VideoInfo videoInfo_;
			AudioInfo audioInfo_;
			std::string warningMessage_, errorMessage_, infoString_;
		};

		typedef int16_t sample_type;
		static sample_type MaxSampleValue;

		struct AudioData {
			AudioData():nSamples_(0), bufferSize_(0), buffer_(nullptr) {}
			Status::AudioInfo audioInfo_;
			unsigned nSamples_;
			unsigned bufferSize_;
			sample_type* buffer_;
		};

		typedef std::function<void(const void*, const void* userData)> OnRendering;
		typedef std::function<void(const AudioData, const void* userData)> OnAudioData;

		StreamController(std::string name = "StreamController");
		~StreamController();

		void play(const std::string& url, OnRendering onRendering, 
			OnAudioData onAudioData, const void* userData);
		void play();
		void pause(bool isOn);
		void stop();
		void seek(float pos);
		void seekMs(int64_t timeMs);
		void setPlaybackSpeed(float speed);

		void setVolume(int volume);

		libvlc_state_t getState() const;
		const Status getStatus() const;
		
		static std::string getStateString(libvlc_state_t state);
	private:
		std::shared_ptr<internal::StreamControllerPrivate> d_;
	};
};

#endif