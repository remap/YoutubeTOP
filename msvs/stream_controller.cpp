//
//	stream_controller.cpp is part of YouTubeTOP.dll
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

#include "stream_controller.h"
#include <iostream>
#include <ctime>
#include <chrono>
#include <sstream>

using namespace std;

static int BufferingLevelMs = 20000;

namespace vlc {
	namespace internal {
		struct StreamControllerPrivate
		{
			static std::mutex logMutex_;
			static FILE* logFile_;

			std::mutex accessMutex_;
			libvlc_instance_t* vlcInstance_;
			libvlc_media_player_t *vlcPlayer_;
			const void* userData_;
			std::string name_;

			int volume = -1;
			bool volumeChanged = false;

			unsigned char *buffer_ = nullptr;
			StreamController::OnRendering onRendering_;
			StreamController::Status status_;

			void subscribeToEvents(std::initializer_list<libvlc_event_type_t> events);
			void flushStatus();
		};

		std::mutex StreamControllerPrivate::logMutex_;
		FILE* StreamControllerPrivate::logFile_ = fopen("yt-streamer.log", "w+");
	}

	namespace {
		static chrono::duration<double> lastLogFlush(0);
		void log(internal::StreamControllerPrivate* c, int level, const char *fmt, ...)
		{
			//ScopedLock lock(c->logMutex_);
			//FILE* outStream = (c->logFile_) ? c->logFile_ : stdout;
			//chrono::system_clock::time_point now = chrono::system_clock::now();
			//chrono::duration<double> seconds = now.time_since_epoch();

			//va_list args;
			//va_start(args, fmt);

			//fprintf(outStream, "%f <%s-%x> [%d]\t", seconds, c->name_.c_str(), c, level);
			//vfprintf(outStream, fmt, args);
			//va_end(args);
			//fprintf(outStream, "\n");

			//if (seconds - lastLogFlush >= chrono::milliseconds(300))
			//{
			//	fflush(outStream);
			//	lastLogFlush = seconds;
			//}
		}

		void vlcLogCallback(void *data, int level, const libvlc_log_t *ctx, const char *fmt, va_list args)
		{
			auto c = reinterpret_cast<internal::StreamControllerPrivate*>(data);

			if (level > LIBVLC_DEBUG)
			{
				FILE* outStream = (c->logFile_) ? c->logFile_ : stdout;
				chrono::system_clock::time_point now = chrono::system_clock::now();
				chrono::duration<double> seconds = now.time_since_epoch();

				//{
				//	ScopedLock lock(c->logMutex_);
				//	fprintf(outStream, "%f <%s-%x-vlc> [%d]\t", seconds, c->name_.c_str(), data, level);
				//	vfprintf(outStream, fmt, args);
				//	fprintf(outStream, "\n");
				//	fflush(outStream);
				//}
				{
					ScopedLock accessLock(c->accessMutex_);
					static char buf[4096];
					memset((void*)buf, 0, 256);
					vsprintf(buf, fmt, args);
					c->status_.infoString_ = std::string(buf);
				}
			}
		}

		/** 
		* Called when a video frame needs to be decoded; this must allocate video memory, which is returned
		* in the argument \c pixelPlane. Also returns a pointer that will be passed into
		* unlockCB() and displayCB() as the void* \c picture argument.
		*/
		void *lockCB(void *opaque, void **pixelPlane)
		{
			auto c = reinterpret_cast<internal::StreamControllerPrivate*>(opaque);
			ScopedLock lock(c->accessMutex_);

			*pixelPlane = c->buffer_;
			return c->buffer_;
		}

		/** 
		* Called when a video frame needs to be displayed
		*/
		void displayCB(void *opaque, void *picture)
		{
			auto c = reinterpret_cast<internal::StreamControllerPrivate*>(opaque);
			ScopedLock lock(c->accessMutex_);

			if (c->onRendering_)
				c->onRendering_(picture, c->userData_);
		}

		/**
		* Handle format info callback from VLC
		*/
		unsigned handleFormat(void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines)
		{
			auto c = reinterpret_cast<internal::StreamControllerPrivate*>(*opaque);
			ScopedLock lock(c->accessMutex_);
			log(c, LIBVLC_DEBUG, "received new video format info", NULL);

			if (c->buffer_)
				free(c->buffer_);

			c->status_.videoInfo_.frameSize_ = *width*(*height) * 4;
			c->buffer_ = (unsigned char*)malloc(c->status_.videoInfo_.frameSize_);

			pitches[0] = pitches[1] = pitches[2] = *width * 4;
			lines[0] = lines[1] = lines[2] = *height;
			memcpy((void*)chroma, (void*)"RGBA", 4);

			c->status_.videoInfo_.width_ = *width;
			c->status_.videoInfo_.height_ = *height;
			c->status_.videoInfo_.totalTime_ = libvlc_media_player_get_length(c->vlcPlayer_);
			c->status_.isVideoInfoReady_ = true;

			return 1; // 1 image buffer allocated
		}

		/**
		* Handle events coming from VLC
		*/
		void handleEvent(const libvlc_event_t *e, void *opaque)
		{
			auto c = reinterpret_cast<internal::StreamControllerPrivate*>(opaque);
			ScopedLock lock(c->accessMutex_);

			if (c->volumeChanged) {
				if (c->volume != libvlc_audio_get_volume(c->vlcPlayer_))
					libvlc_audio_set_volume(c->vlcPlayer_, c->volume);
				else
					c->volumeChanged = false;
			}

			libvlc_state_t newState = libvlc_media_player_get_state(c->vlcPlayer_);
			std::string state = StreamController::getStateString(newState);

			switch (e->type) {
			case libvlc_MediaPlayerNothingSpecial:
				log(c, LIBVLC_DEBUG, "event: nothing special", NULL);
				break;
			case libvlc_MediaPlayerOpening:
				log(c, LIBVLC_DEBUG, "opening", NULL);
				break;
			case libvlc_MediaPlayerBuffering:
			{
				double bufferLevel = (c->status_.videoInfo_.totalTime_ >= BufferingLevelMs) ?
					e->u.media_player_buffering.new_cache :
					((double)e->u.media_player_buffering.new_cache / 100. * BufferingLevelMs) / (double)c->status_.videoInfo_.totalTime_ * 100.;

				if ((int)c->status_.videoInfo_.bufferLevel_ % 10 >
					(int)bufferLevel % 10)
				{
					log(c, LIBVLC_DEBUG, "buffering %.2f", bufferLevel, NULL);
				}
				
				c->status_.videoInfo_.bufferLevel_ = bufferLevel;
			}
				break;
			case libvlc_MediaPlayerPlaying:
				log(c, LIBVLC_DEBUG, "playing", NULL);
				break;
			case libvlc_MediaPlayerPaused:
				log(c, LIBVLC_DEBUG, "paused", NULL);
				break;
			case libvlc_MediaPlayerStopped:
				log(c, LIBVLC_DEBUG, "stopped", NULL);
				break;
			case libvlc_MediaPlayerEndReached:
				log(c, LIBVLC_DEBUG, "EOF %ld state %s", libvlc_Error, state.c_str());
				break;
			case libvlc_MediaPlayerTimeChanged:
				break;
			default:
				log(c, LIBVLC_NOTICE, "state %s time %ld playing %ld", 
					state.c_str(),
					libvlc_media_player_get_time(c->vlcPlayer_), 
					libvlc_media_player_is_playing(c->vlcPlayer_), NULL);	
			}			

			if (c->status_.state_ != newState)
			{
				log(c, LIBVLC_NOTICE, "new state %s", state.c_str(), NULL);
				c->status_.state_ = newState;
			}
			
			double progress = (double)e->u.media_player_time_changed.new_time / (double)libvlc_media_player_get_length(c->vlcPlayer_);
			double prevProgress = (double)c->status_.videoInfo_.currentTime_ / (double)libvlc_media_player_get_length(c->vlcPlayer_);
			//log(c, LIBVLC_DEBUG, "%f %f", prevProgress, progress, NULL);
			if (c->status_.videoInfo_.currentTime_ != libvlc_media_player_get_time(c->vlcPlayer_) &&
				(int)(prevProgress * 100) % 10 > (int)(progress * 100) % 10)
				log(c, LIBVLC_DEBUG, "time %d", libvlc_media_player_get_time(c->vlcPlayer_), NULL);

			c->status_.videoInfo_.currentTime_ = libvlc_media_player_get_time(c->vlcPlayer_);
		}
	}

	void internal::StreamControllerPrivate::subscribeToEvents(std::initializer_list<libvlc_event_type_t> events)
	{
		auto p_em = libvlc_media_player_event_manager(vlcPlayer_);

		for (auto event : events)
			libvlc_event_attach(p_em, event, handleEvent, this);
	}

	void internal::StreamControllerPrivate::flushStatus()
	{
		status_.isVideoInfoReady_ = false;
		status_.state_ = libvlc_NothingSpecial;
		status_.videoUrl_ = "";
		status_.videoInfo_.bufferLevel_ = 0;
		status_.videoInfo_.currentTime_ = 0;
		status_.videoInfo_.height_ = 0;
		status_.videoInfo_.width_ = 0;
		status_.videoInfo_.totalTime_ = 0;
	}

	StreamController::StreamController(std::string name)
		: d_(new internal::StreamControllerPrivate)
	{
		static const int nArgs = 1;
		static const char *libVlcArgs[nArgs] = { "--network-caching=20000" };

 		d_->flushStatus();
		d_->name_ = name;
		d_->vlcInstance_ = libvlc_new(nArgs, libVlcArgs);

		if (d_->vlcInstance_)
		{
			libvlc_log_set(d_->vlcInstance_, &vlcLogCallback, d_.get());
			d_->vlcPlayer_ = libvlc_media_player_new(d_->vlcInstance_);
			log(d_.get(), LIBVLC_DEBUG, "created new player instance", NULL);

			d_->subscribeToEvents({
				libvlc_MediaPlayerNothingSpecial,
				libvlc_MediaPlayerOpening,
				libvlc_MediaPlayerBuffering,
				libvlc_MediaPlayerPlaying,
				libvlc_MediaPlayerPaused,
				libvlc_MediaPlayerStopped,
				libvlc_MediaPlayerEndReached,
				libvlc_MediaPlayerEncounteredError,
				libvlc_MediaPlayerTimeChanged,
				libvlc_MediaPlayerMediaChanged
			});

			libvlc_video_set_callbacks(d_->vlcPlayer_, &lockCB, NULL /*unlock*/, &displayCB, d_.get());
			libvlc_video_set_format_callbacks(d_->vlcPlayer_, handleFormat, NULL);
		}
		else
			throw std::exception("Couldn't initialize VLC instance");
	}

	StreamController::~StreamController()
	{
		libvlc_media_player_stop(d_->vlcPlayer_);

		{
			ScopedLock lock(d_->accessMutex_);

			libvlc_media_player_release(d_->vlcPlayer_);
			log(d_.get(), LIBVLC_NOTICE, "released player instance", NULL);
			libvlc_log_unset(d_->vlcInstance_);
			libvlc_release(d_->vlcInstance_);
			log(d_.get(), LIBVLC_NOTICE, "released VLC library instance", NULL);

			fclose(d_->logFile_);

			if (d_->buffer_)
				free(d_->buffer_);
		}
	}
	void StreamController::play(const std::string& url, OnRendering onRendering, const void* userData)
	{
		log(d_.get(), LIBVLC_NOTICE, "play request for URL %s", url.c_str(), NULL);
		libvlc_media_player_stop(d_->vlcPlayer_);

		d_->flushStatus();
		d_->onRendering_ = onRendering;
		d_->userData_ = userData;
		d_->status_.videoUrl_ = url;

		libvlc_media_t *media = libvlc_media_new_location(d_->vlcInstance_, url.c_str());

		libvlc_media_player_set_media(d_->vlcPlayer_, media);
		libvlc_media_release(media);
		libvlc_media_player_play(d_->vlcPlayer_);

		log(d_.get(), LIBVLC_DEBUG, "set player for playback...", NULL);
	}

	void StreamController::play()
	{
		int res = libvlc_media_player_play(d_->vlcPlayer_);
		log(d_.get(), LIBVLC_NOTICE, "resume playback request: %d", res, NULL);
	}

	void StreamController::pause(bool on)
	{
		bool isPaused = (libvlc_Paused == d_->status_.state_);

		if (isPaused ^ on)
		{
			log(d_.get(), LIBVLC_NOTICE, "pause playback request %d", on, NULL);
			libvlc_media_player_set_pause(d_->vlcPlayer_, on);
		}
	}

	void StreamController::stop()
	{
		log(d_.get(), LIBVLC_NOTICE, "stop playback request", NULL);
		libvlc_media_player_stop(d_->vlcPlayer_);
		d_->flushStatus();
	}

	void StreamController::seek(float pos)
	{
		if (libvlc_media_player_is_seekable(d_->vlcPlayer_))
		{
			float curPos = round(libvlc_media_player_get_position(d_->vlcPlayer_)*100)/100;
			if (round(pos*100)/100 != curPos)
			{
				log(d_.get(), LIBVLC_NOTICE, "seek to position %.2f", pos, NULL);
				libvlc_media_player_set_position(d_->vlcPlayer_, pos);
			}
		}
		else
			log(d_.get(), LIBVLC_WARNING, "media is not seekable", NULL);
	}

	void StreamController::setPlaybackSpeed(float speed)
	{
		log(d_.get(), LIBVLC_NOTICE, "set playback speed to %.2f", speed, NULL);
		libvlc_media_player_set_rate(d_->vlcPlayer_, speed);
	}

	void StreamController::setVolume(int volume)
	{
		log(d_.get(), LIBVLC_NOTICE, "set volume request", NULL);

		d_->volume = volume;
		d_->volumeChanged = true;

		if (libvlc_audio_get_volume(d_->vlcPlayer_) != -1)
			libvlc_audio_set_volume(d_->vlcPlayer_, volume);
	}

	libvlc_state_t StreamController::getState() const
	{
		return libvlc_media_player_get_state(d_->vlcPlayer_);
	}

	const StreamController::Status StreamController::getStatus() const
	{
		ScopedLock lock(d_->accessMutex_);
		StreamController::Status status = d_->status_;
		return status;
	}

	std::string StreamController::getStateString(libvlc_state_t state)
	{
		switch (state)
		{
		case libvlc_NothingSpecial:
			return "Nothing Special";
		case libvlc_Opening:
			return "Opening";
		case libvlc_Buffering:
			return "Buffering";
		case libvlc_Playing:
			return "Playing";
		case libvlc_Paused:
			return "Paused";
		case libvlc_Stopped:
			return "Stopped";
		case libvlc_Ended:
			return "Ended";
		case libvlc_Error:
			return "Error";
		default:
			break;
		}

		return "Unknown";
	}
}
