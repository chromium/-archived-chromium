// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/buffers.h"
#include "media/base/filter_host.h"
#include "media/filters/video_renderer_base.h"

namespace media {

// Limit our read ahead to three frames.  One frame is typically in flux at all
// times, as in frame n is discarded at the top of ThreadMain() while frame
// (n + kMaxFrames) is being asynchronously fetched.  The remaining two frames
// allow us to advance the current frame as well as read the timestamp of the
// following frame for more accurate timing.
//
// Increasing this number beyond 3 simply creates a larger buffer to work with
// at the expense of memory (~0.5MB and ~1.3MB per frame for 480p and 720p
// resolutions, respectively).  This can help on lower-end systems if there are
// difficult sections in the movie and decoding slows down.
static const size_t kMaxFrames = 3;

// Sleeping for negative amounts actually hangs your thread on Windows!
static const int64 kMinSleepMilliseconds = 0;

// This equates to ~13.33 fps, which is just under the typical 15 fps that
// lower quality cameras or shooting modes usually use for video encoding.
static const int64 kMaxSleepMilliseconds = 75;

VideoRendererBase::VideoRendererBase()
    : frame_available_(&lock_),
      state_(UNINITIALIZED),
      thread_(NULL),
      playback_rate_(0) {
}

VideoRendererBase::~VideoRendererBase() {
  AutoLock auto_lock(lock_);
  DCHECK(state_ == UNINITIALIZED || state_ == STOPPED);
}

// static
bool VideoRendererBase::ParseMediaFormat(const MediaFormat& media_format,
                                         int* width_out, int* height_out) {
  std::string mime_type;
  if (!media_format.GetAsString(MediaFormat::kMimeType, &mime_type))
    return false;
  if (mime_type.compare(mime_type::kUncompressedVideo) != 0)
    return false;
  if (!media_format.GetAsInteger(MediaFormat::kWidth, width_out))
    return false;
  if (!media_format.GetAsInteger(MediaFormat::kHeight, height_out))
    return false;
  return true;
}

void VideoRendererBase::Stop() {
  AutoLock auto_lock(lock_);
  state_ = STOPPED;

  // Signal the subclass we're stopping.
  // TODO(scherkus): do we trust subclasses not to do something silly while
  // we're holding the lock?
  OnStop();

  // Clean up our thread if present.
  if (thread_) {
    // Signal the thread since it's possible to get stopped with the video
    // thread waiting for a read to complete.
    frame_available_.Signal();
    {
      AutoUnlock auto_unlock(lock_);
      PlatformThread::Join(thread_);
    }
    thread_ = NULL;
  }
}

void VideoRendererBase::SetPlaybackRate(float playback_rate) {
  AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererBase::Seek(base::TimeDelta time) {
  AutoLock auto_lock(lock_);
  // We need the first frame in |frames_| to run the VideoRendererBase main
  // loop, but we don't need decoded frames after the first frame since we are
  // at a new time. We should get some new frames so issue reads to compensate
  // for those discarded.
  while (frames_.size() > 1) {
    frames_.pop_back();
    ScheduleRead();
  }
}

bool VideoRendererBase::Initialize(VideoDecoder* decoder) {
  AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, UNINITIALIZED);
  state_ = INITIALIZING;
  decoder_ = decoder;

  // Notify the pipeline of the video dimensions.
  int width = 0;
  int height = 0;
  if (!ParseMediaFormat(decoder->media_format(), &width, &height))
    return false;
  host()->SetVideoSize(width, height);

  // Initialize the subclass.
  // TODO(scherkus): do we trust subclasses not to do something silly while
  // we're holding the lock?
  if (!OnInitialize(decoder))
    return false;

  // Create our video thread.
  if (!PlatformThread::Create(0, this, &thread_)) {
    NOTREACHED() << "Video thread creation failed";
    return false;
  }

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)

  // Queue initial reads.
  for (size_t i = 0; i < kMaxFrames; ++i) {
    ScheduleRead();
  }

  return true;
}

// PlatformThread::Delegate implementation.
void VideoRendererBase::ThreadMain() {
  PlatformThread::SetName("VideoThread");

  // Wait to be initialized so we can notify the first frame is available.
  if (!WaitForInitialized())
    return;
  OnFrameAvailable();

  for (;;) {
    // State and playback rate to assume for this iteration of the loop.
    State state;
    float playback_rate;
    {
      AutoLock auto_lock(lock_);
      state = state_;
      playback_rate = playback_rate_;
    }
    if (state == STOPPED) {
      return;
    }
    DCHECK_EQ(state, INITIALIZED);

    // Sleep for 10 milliseconds while paused.
    if (playback_rate == 0) {
      PlatformThread::Sleep(10);
      continue;
    }

    // Advance |current_frame_| and try to determine |next_frame|.
    scoped_refptr<VideoFrame> next_frame;
    {
      AutoLock auto_lock(lock_);
      DCHECK(!frames_.empty());
      DCHECK_EQ(current_frame_, frames_.front());
      frames_.pop_front();
      ScheduleRead();
      while (frames_.empty()) {
        frame_available_.Wait();

        // We have the lock again, check the actual state to see if we're trying
        // to stop.
        if (state_ == STOPPED) {
          return;
        }
      }
      current_frame_ = frames_.front();
      if (frames_.size() >= 2) {
        next_frame = frames_[1];
      }
    }

    // Notify subclass that |current_frame_| has been updated.
    OnFrameAvailable();

    // Determine the current and next presentation timestamps.
    base::TimeDelta now = host()->GetTime();
    base::TimeDelta this_pts = current_frame_->GetTimestamp();
    base::TimeDelta next_pts;
    if (next_frame) {
      next_pts = next_frame->GetTimestamp();
    } else {
      next_pts = this_pts + current_frame_->GetDuration();
    }

    // Determine our sleep duration based on whether time advanced.
    base::TimeDelta sleep;
    if (now == previous_time_) {
      // Time has not changed, assume we sleep for the frame's duration.
      sleep = next_pts - this_pts;
    } else {
      // Time has changed, figure out real sleep duration.
      sleep = next_pts - now;
      previous_time_ = now;
    }

    // Scale our sleep based on the playback rate.
    // TODO(scherkus): floating point badness and degrade gracefully.
    int sleep_ms = static_cast<int>(sleep.InMicroseconds() / playback_rate /
        base::Time::kMicrosecondsPerMillisecond);

    // To be safe, limit our sleep duration.
    // TODO(scherkus): handle seeking gracefully.. right now a seek backwards
    // will hit kMinSleepMilliseconds whereas a seek forward will hit
    // kMaxSleepMilliseconds.
    if (sleep_ms < kMinSleepMilliseconds)
      sleep_ms = kMinSleepMilliseconds;
    else if (sleep_ms > kMaxSleepMilliseconds)
      sleep_ms = kMaxSleepMilliseconds;

    PlatformThread::Sleep(sleep_ms);
  }
}

void VideoRendererBase::GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out) {
  AutoLock auto_lock(lock_);
  // Either we have initialized or we have the current frame.
  DCHECK(state_ != INITIALIZED || current_frame_);
  *frame_out = current_frame_;
}

void VideoRendererBase::OnReadComplete(VideoFrame* frame) {
  AutoLock auto_lock(lock_);
  // If this is an end of stream frame, don't enqueue it since it has no data.
  if (!frame->IsEndOfStream()) {
    frames_.push_back(frame);
    DCHECK_LE(frames_.size(), kMaxFrames);
    frame_available_.Signal();
  }

  // Check for our initialization condition.
  if (state_ == INITIALIZING &&
      (frames_.size() == kMaxFrames || frame->IsEndOfStream())) {
    if (frames_.empty()) {
      // We should have initialized but there's no decoded frames in the queue.
      // Raise an error.
      host()->Error(PIPELINE_ERROR_NO_DATA);
    } else {
      state_ = INITIALIZED;
      current_frame_ = frames_.front();
      host()->InitializationComplete();
    }
  }
}

void VideoRendererBase::ScheduleRead() {
  decoder_->Read(NewCallback(this, &VideoRendererBase::OnReadComplete));
}

bool VideoRendererBase::WaitForInitialized() {
  // This loop essentially handles preroll.  We wait until we've been fully
  // initialized so we can call OnFrameAvailable() to provide subclasses with
  // the first frame.
  AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, INITIALIZING);
  while (state_ == INITIALIZING) {
    frame_available_.Wait();
    if (state_ == STOPPED) {
      return false;
    }
  }
  DCHECK_EQ(state_, INITIALIZED);
  DCHECK(current_frame_);
  return true;
}

}  // namespace media
