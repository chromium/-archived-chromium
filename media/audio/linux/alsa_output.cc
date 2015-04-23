// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The audio stream implementation is made difficult because different methods
// are available for calling depending on what state the stream is.  Here is the
// state transition table for the stream.
//
// STATE_CREATED -> Open() -> STATE_OPENED
// STATE_OPENED -> Start() -> STATE_STARTED
// STATE_OPENED -> Close() -> STATE_CLOSED
// STATE_STARTED -> Stop() -> STATE_STOPPED
// STATE_STARTED -> Close() -> STATE_CLOSING | STATE_CLOSED
// STATE_STOPPED -> Close() -> STATE_CLOSING | STATE_CLOSED
// STATE_CLOSING -> [automatic] -> STATE_CLOSED
//
// Error states and resource management:
//
// Entrance into STATE_STOPPED signals schedules a call to ReleaseResources().
//
// Any state may transition to STATE_ERROR.  On transitioning into STATE_ERROR,
// the function doing the transition is reponsible for scheduling a call to
// ReleaseResources() or otherwise ensuring resources are cleaned (eg., as is
// done in Open()).  This should be done while holding the lock to avoid a
// destruction race condition where the stream is deleted via ref-count before
// the ReleaseResources() task is scheduled.  In particular, be careful of
// resource management in a transtiion from STATE_STOPPED -> STATE_ERROR if
// that becomes necessary in the future.
//
// STATE_ERROR may transition to STATE_CLOSED.  In this situation, no further
// resource management is done because it is assumed that the resource
// reclaimation was executed at the point of the state transition into
// STATE_ERROR.
//
// Entrance into STATE_CLOSED implies a transition through STATE_STOPPED, which
// triggers the resource management code.
//
// The destructor is not responsible for ultimate cleanup of resources.
// Instead, it only checks that the stream is in a state where all resources
// have been cleaned up.  These states are STATE_CREATED, STATE_CLOSED,
// STATE_ERROR.
//
// TODO(ajwong): This incorrectly handles the period size for filling of the
// ALSA device buffer.  Currently the period size is hardcoded, and not
// reported to the sound device.  Also, there are options that let the device
// wait until the buffer is minimally filled before playing.  Those should be
// explored.  Also consider doing the device interactions either outside of the
// class lock, or under a different lock to avoid unecessarily blocking other
// threads.

#include "media/audio/linux/alsa_output.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/time.h"

// Require 10ms latency from the audio device.  Taken from ALSA documentation
// example.
// TODO(ajwong): Figure out what this parameter actually does, and what a good
// value would be.
static const unsigned int kTargetLatencyMicroseconds = 10000;

// Minimal amount of time to sleep.  If any future event is expected to
// execute within this timeframe, treat it as if it should execute immediately.
//
// TODO(ajwong): Determine if there is a sensible minimum sleep resolution and
// adjust accordingly.
static const int64 kMinSleepMilliseconds = 10L;

const char* AlsaPCMOutputStream::kDefaultDevice = "default";

AlsaPCMOutputStream::AlsaPCMOutputStream(const std::string& device_name,
                                         int min_buffer_ms,
                                         AudioManager::Format format,
                                         int channels,
                                         int sample_rate,
                                         char bits_per_sample)
    : state_(STATE_CREATED),
      device_name_(device_name),
      playback_handle_(NULL),
      source_callback_(NULL),
      playback_thread_("PlaybackThread"),
      channels_(channels),
      sample_rate_(sample_rate),
      bits_per_sample_(bits_per_sample),
      min_buffer_frames_((min_buffer_ms * sample_rate_) /
                         base::Time::kMillisecondsPerSecond),
      packet_size_(0),
      device_write_suspended_(true),  // Start suspended.
      resources_released_(false) {
  // Reference self to avoid accidental deletion before the message loop is
  // done.
  AddRef();

  // Sanity check input values.
  if (channels_ != 2) {
    LOG(WARNING) << "Only 2-channel audio is supported right now.";
    state_ = STATE_ERROR;
  }

  if (AudioManager::AUDIO_PCM_LINEAR != format) {
    LOG(WARNING) << "Only linear PCM supported.";
    state_ = STATE_ERROR;
  }

  if (bits_per_sample % 8 != 0) {
    // We do this explicitly just incase someone messes up the switch below.
    LOG(WARNING) << "Only allow byte-aligned samples";
    state_ = STATE_ERROR;
  }

  switch (bits_per_sample) {
    case 8:
      pcm_format_ = SND_PCM_FORMAT_S8;
      break;

    case 16:
      pcm_format_ = SND_PCM_FORMAT_S16;
      break;

    case 24:
      pcm_format_ = SND_PCM_FORMAT_S24;
      break;

    case 32:
      pcm_format_ = SND_PCM_FORMAT_S32;
      break;

    default:
      LOG(DFATAL) << "Unsupported bits per sample: " << bits_per_sample_;
      state_ = STATE_ERROR;
      break;
  }

  // Interleaved audio is expected, so each frame has one sample per channel.
  bytes_per_frame_ = channels_ * (bits_per_sample_ / 8);
}

AlsaPCMOutputStream::~AlsaPCMOutputStream() {
  AutoLock l(lock_);
  // In STATE_CREATED, STATE_CLOSED, and STATE_ERROR, resources are guaranteed
  // to be released.
  CHECK(state_ == STATE_CREATED ||
        state_ == STATE_CLOSED ||
        state_ == STATE_ERROR);
}

bool AlsaPCMOutputStream::Open(size_t packet_size) {
  AutoLock l(lock_);

  // Check that stream is coming from the correct state and early out if not.
  if (state_ == STATE_ERROR) {
    return false;
  }
  if (state_ != STATE_CREATED) {
    NOTREACHED() << "Stream must be in STATE_CREATED on Open. Instead in: "
                 << state_;
    return false;
  }

  // Open the device and set the parameters.
  // TODO(ajwong): Can device open block?  Probably.  If yes, we need to move
  // the open call into a different thread.
  int error = snd_pcm_open(&playback_handle_, device_name_.c_str(),
                           SND_PCM_STREAM_PLAYBACK, 0);
  if (error < 0) {
    LOG(ERROR) << "Cannot open audio device (" << device_name_ << "): "
               << snd_strerror(error);
    EnterStateError_Locked();
    return false;
  }
  if ((error = snd_pcm_set_params(playback_handle_,
                                  pcm_format_,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  channels_,
                                  sample_rate_,
                                  1,  // soft_resample -- let ALSA resample
                                  kTargetLatencyMicroseconds)) < 0) {
    LOG(ERROR) << "Unable to set PCM parameters: " << snd_strerror(error);
    if (!CloseDevice_Locked()) {
      LOG(WARNING) << "Unable to close audio device. Leaking handle.";
    }
    playback_handle_ = NULL;
    EnterStateError_Locked();
    return false;
  }

  // Configure the buffering.
  packet_size_ = packet_size;
  DCHECK_EQ(0U, packet_size_ % bytes_per_frame_)
      << "Buffers should end on a frame boundary. Frame size: "
      << bytes_per_frame_;

  // Everything is okay.  Stream is officially STATE_OPENED for business.
  state_ = STATE_OPENED;

  return true;
}

void AlsaPCMOutputStream::Start(AudioSourceCallback* callback) {
  AutoLock l(lock_);

  // Check that stream is coming from the correct state and early out if not.
  if (state_ == STATE_ERROR) {
    return;
  }
  if (state_ != STATE_OPENED) {
    NOTREACHED() << "Can only be started from STATE_OPEN. Current state: "
                 << state_;
    return;
  }

  source_callback_ = callback;

  playback_thread_.Start();
  playback_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AlsaPCMOutputStream::BufferPackets));

  state_ = STATE_STARTED;
}

void AlsaPCMOutputStream::Stop() {
  AutoLock l(lock_);
  // If the stream is in STATE_ERROR, it is effectively stopped already.
  if (state_ == STATE_ERROR) {
    return;
  }
  StopInternal_Locked();
}

void AlsaPCMOutputStream::StopInternal_Locked() {
  // Check the lock is held in a debug build.
  DCHECK((lock_.AssertAcquired(), true));

  if (state_ != STATE_STARTED) {
    NOTREACHED() << "Stream must be in STATE_STARTED to Stop. Instead in: "
                 << state_;
    return;
  }

  // Move immediately to STATE_STOPPED to signal that all functions should cease
  // working at this point.  Then post a task to the playback thread to release
  // resources.
  state_ = STATE_STOPPED;

  playback_thread_.message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AlsaPCMOutputStream::ReleaseResources));
}

void AlsaPCMOutputStream::EnterStateError_Locked() {
  // Check the lock is held in a debug build.
  DCHECK((lock_.AssertAcquired(), true));

  state_ = STATE_ERROR;
  resources_released_ = true;

  // TODO(ajwong): Call OnError() on source_callback_.
}

void AlsaPCMOutputStream::Close() {
  AutoLock l(lock_);

  // If in STATE_ERROR, all asynchronous resource reclaimation is finished, so
  // just change states and release this instance to delete ourself.
  if (state_ == STATE_ERROR) {
    Release();
    state_ = STATE_CLOSED;
    return;
  }

  // Otherwise, cleanup as necessary.
  if (state_ == STATE_CLOSED || state_ == STATE_CLOSING) {
    NOTREACHED() << "Attempting to close twice.";
    return;
  }

  // If the stream is still running, stop it.
  if (state_ == STATE_STARTED) {
    StopInternal_Locked();
  }

  // If it is stopped (we may have just transitioned here in the previous if
  // block), check if the resources have been released.  If they have,
  // transition immediately to STATE_CLOSED.  Otherwise, move to
  // STATE_CLOSING, and the ReleaseResources() task will move to STATE_CLOSED
  // for us.
  //
  // If the stream has been stopped, close.
  if (state_ == STATE_STOPPED) {
    if (resources_released_) {
      state_ = STATE_CLOSED;
    } else {
      state_ = STATE_CLOSING;
    }
  } else {
    // TODO(ajwong): Can we safely handle state_ == STATE_CREATED?
    NOTREACHED() << "Invalid state on close: " << state_;
    // In release, just move to STATE_ERROR, and hope for the best.
    EnterStateError_Locked();
  }
}

bool AlsaPCMOutputStream::CloseDevice_Locked() {
  // Check the lock is held in a debug build.
  DCHECK((lock_.AssertAcquired(), true));

  int error = snd_pcm_close(playback_handle_);
  if (error < 0) {
    LOG(ERROR) << "Cannot close audio device (" << device_name_ << "): "
               << snd_strerror(error);
    return false;
  }

  return true;
}

void AlsaPCMOutputStream::ReleaseResources() {
  AutoLock l(lock_);

  // Shutdown the audio device.
  if (!CloseDevice_Locked()) {
    LOG(WARNING) << "Unable to close audio device. Leaking handle.";
    playback_handle_ = NULL;
  }

  // Delete all the buffers.
  STLDeleteElements(&buffered_packets_);

  // Release the source callback.
  source_callback_->OnClose(this);

  // Shutdown the thread.
  DCHECK_EQ(PlatformThread::CurrentId(), playback_thread_.thread_id());
  playback_thread_.message_loop()->Quit();

  // TODO(ajwong): Do we need to join the playback thread?

  // If the stream is closing, then this function has just completed the last
  // bit needed before closing.  Transition to STATE_CLOSED.
  if (state_ == STATE_CLOSING) {
    state_ = STATE_CLOSED;
  }

  // TODO(ajwong): Currently, the stream is leaked after the |playback_thread_|
  // is stopped.  Find a way to schedule its deletion on another thread, maybe
  // using a DestructionObserver.
}

snd_pcm_sframes_t AlsaPCMOutputStream::GetFramesOfDelay_Locked() {
  // Check the lock is held in a debug build.
  DCHECK((lock_.AssertAcquired(), true));

  // Find the number of frames queued in the sound device.
  snd_pcm_sframes_t delay_frames = 0;
  int error = snd_pcm_delay(playback_handle_, &delay_frames);
  if (error  < 0) {
    error = snd_pcm_recover(playback_handle_,
                            error  /* Original error. */,
                            0  /* Silenty recover. */);
  }
  if (error < 0) {
    LOG(ERROR) << "Could not query sound device for delay. Assuming 0: "
               << snd_strerror(error);
  }

  for (std::deque<Packet*>::const_iterator it = buffered_packets_.begin();
       it != buffered_packets_.end();
       ++it) {
    delay_frames += ((*it)->size - (*it)->used) / bytes_per_frame_;
  }

  return delay_frames;
}

void AlsaPCMOutputStream::BufferPackets() {
  AutoLock l(lock_);

  // Handle early outs for errored, stopped, or closing streams.
  if (state_ == STATE_ERROR ||
      state_ == STATE_STOPPED ||
      state_ == STATE_CLOSING) {
    return;
  }
  if (state_ != STATE_STARTED) {
    NOTREACHED() << "Invalid stream state while buffering. "
                 << "Expected STATE_STARTED. Current state: " << state_;
    return;
  }

  // Early out if the buffer is already full.
  snd_pcm_sframes_t delay_frames = GetFramesOfDelay_Locked();
  if (delay_frames < min_buffer_frames_) {
    // Grab one packet.  Drop the lock for the synchronous call.  This will
    // still stall the playback thread, but at least it will not block any
    // other threads.
    //
    // TODO(ajwong): Move to cpu@'s non-blocking audio source.
    scoped_ptr<Packet> packet;
    size_t capacity = packet_size_;  // Snag it for non-locked usage.
    {
      AutoUnlock synchronous_data_fetch(lock_);
      packet.reset(new Packet(capacity));
      size_t used = source_callback_->OnMoreData(this, packet->buffer.get(),
                                                 packet->capacity);
      CHECK(used <= capacity) << "Data source overran buffer. Aborting.";
      packet->size = used;
      // TODO(ajwong): Do more buffer validation here, like checking that the
      // packet is correctly aligned to frames, etc.
    }
    // After reacquiring the lock, recheck state to make sure it is still
    // STATE_STARTED.
    if (state_ != STATE_STARTED) {
      return;
    }
    buffered_packets_.push_back(packet.release());

    // Recalculate delay frames.
    delay_frames = GetFramesOfDelay_Locked();
  }

  // Since the current implementation of OnMoreData() blocks, only try to grab
  // one packet per task.  If the buffer is still too low, post another
  // BufferPackets() task immediately.  Otherwise, calculate when the buffer is
  // likely to need filling and schedule a poll for the future.
  int next_fill_time_ms = (delay_frames - min_buffer_frames_) / sample_rate_;
  if (next_fill_time_ms <= kMinSleepMilliseconds) {
    playback_thread_.message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AlsaPCMOutputStream::BufferPackets));
  } else {
    // TODO(ajwong): Measure the reliability of the delay interval.  Use
    // base/histogram.h.
    playback_thread_.message_loop()->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &AlsaPCMOutputStream::BufferPackets),
        next_fill_time_ms);
  }

  // If the |device_write_suspended_|, the audio device write tasks have
  // stopped scheduling themselves due to an underrun of the in-memory buffer.
  // Post a new task to restart it since we now have data.
  if (device_write_suspended_) {
    device_write_suspended_ = false;
    playback_thread_.message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AlsaPCMOutputStream::FillAlsaDeviceBuffer));
  }
}

void AlsaPCMOutputStream::FillAlsaDeviceBuffer() {
  // TODO(ajwong): Try to move some of this code out from underneath the lock.
  AutoLock l(lock_);

  // Find the number of frames that the device can accept right now.
  snd_pcm_sframes_t device_buffer_frames_avail =
      snd_pcm_avail_update(playback_handle_);

  // Write up to |device_buffer_frames_avail| frames to the ALSA device.
  while (device_buffer_frames_avail > 0) {
    if (buffered_packets_.empty()) {
      device_write_suspended_ = true;
      break;
    }

    Packet* current_packet = buffered_packets_.front();

    // Only process non 0-lengthed packets.
    if (current_packet->used < current_packet->size) {
      // Calculate the number of frames we have to write.
      char* buffer_pos = current_packet->buffer.get() + current_packet->used;
      snd_pcm_sframes_t buffer_frames =
          (current_packet->size - current_packet->used) /
          bytes_per_frame_;
      snd_pcm_sframes_t frames_to_write =
          std::min(buffer_frames, device_buffer_frames_avail);

      // Check that device_buffer_frames_avail isn't < 0.
      DCHECK_GT(frames_to_write, 0);

      // Write it to the device.
      int frames_written =
          snd_pcm_writei(playback_handle_, buffer_pos, frames_to_write);
      if (frames_written < 0) {
        // Recover from EINTR, EPIPE (overrun/underrun), ESTRPIPE (stream
        // suspended).
        //
        // TODO(ajwong): Check that we do not need to loop on recover, here and
        // anywhere else we use recover.
        frames_written = snd_pcm_recover(playback_handle_,
                                         frames_written  /* Original error. */,
                                         0  /* Silenty recover. */);
      }
      if (frames_written < 0) {
        LOG(ERROR) << "Failed to write to pcm device: "
                   << snd_strerror(frames_written);
        ReleaseResources();
        EnterStateError_Locked();
        break;
      } else {
        current_packet->used += frames_written * bytes_per_frame_;
        DCHECK_LE(current_packet->used, current_packet->size);
      }
    }

    if (current_packet->used >= current_packet->size) {
      delete current_packet;
      buffered_packets_.pop_front();
    }
  }

  // If the memory buffer was not underrun, schedule another fill in the future.
  if (!device_write_suspended_) {
    playback_thread_.message_loop()->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &AlsaPCMOutputStream::FillAlsaDeviceBuffer),
        kTargetLatencyMicroseconds / base::Time::kMicrosecondsPerMillisecond);
  }
}

void AlsaPCMOutputStream::SetVolume(double left_level, double right_level) {
  NOTIMPLEMENTED();
}

void AlsaPCMOutputStream::GetVolume(double* left_level, double* right_level) {
  NOTIMPLEMENTED();
}

size_t AlsaPCMOutputStream::GetNumBuffers() {
  return 0;
}
