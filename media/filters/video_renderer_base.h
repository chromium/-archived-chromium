// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// VideoRendererBase creates its own thread for the sole purpose of timing frame
// presentation.  It handles reading from the decoder and stores the results in
// a queue of decoded frames, calling OnFrameAvailable() on subclasses to notify
// when a frame is ready to display.
//
// The media filter methods Initialize(), Stop(), SetPlaybackRate() and Seek()
// should be serialized, which they commonly are the pipeline thread.
// As long as VideoRendererBase is initialized, GetCurrentFrame() is safe to
// call from any thread, at any time, including inside of OnFrameAvailable().

#ifndef MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
#define MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_

#include <deque>

#include "base/condition_variable.h"
#include "base/lock.h"
#include "media/base/filters.h"

namespace media {

// TODO(scherkus): to avoid subclasses, consider using a peer/delegate interface
// and pass in a reference to the constructor.
class VideoRendererBase : public VideoRenderer,
                          public PlatformThread::Delegate {
 public:
  VideoRendererBase();
  virtual ~VideoRendererBase();

  // Helper method for subclasses to parse out video-related information from
  // a MediaFormat.  Returns true if |width_out| and |height_out| were assigned.
  static bool ParseMediaFormat(const MediaFormat& media_format,
                               int* width_out, int* height_out);

  // MediaFilter implementation.
  virtual void Stop();
  virtual void SetPlaybackRate(float playback_rate);
  virtual void Seek(base::TimeDelta time);

  // VideoRenderer implementation.
  virtual bool Initialize(VideoDecoder* decoder);

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain();

  // Assigns the current frame, which will never be NULL as long as this filter
  // is initialized.
  void GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out);

 protected:
  // Subclass interface.  Called before any other initialization in the base
  // class takes place.
  //
  // Implementors typically use the media format of |decoder| to create their
  // output surfaces.  Implementors should NOT call InitializationComplete().
  virtual bool OnInitialize(VideoDecoder* decoder) = 0;

  // Subclass interface.  Called before any other stopping actions takes place.
  //
  // Implementors should perform any necessary cleanup before returning.
  virtual void OnStop() = 0;

  // Subclass interface.  Called when a new frame is ready for display, which
  // can be accessed via GetCurrentFrame().
  //
  // Implementors should avoid doing any sort of heavy work in this method and
  // instead post a task to a common/worker thread to handle rendering.  Slowing
  // down the video thread may result in losing synchronization with audio.
  virtual void OnFrameAvailable() = 0;

 private:
  // Read complete callback from video decoder.
  void OnReadComplete(VideoFrame* frame);

  // Helper method that schedules an asynchronous read from the decoder.
  //
  // Safe to call from any thread.
  void ScheduleRead();

  // Called by ThreadMain() to handle preroll.  Returns false if the thread
  // should exit due to Stop() being called.
  bool WaitForInitialized();

  scoped_refptr<VideoDecoder> decoder_;

  // Queue of incoming frames as well as the current frame since the last time
  // OnFrameAvailable() was called.
  typedef std::deque< scoped_refptr<VideoFrame> > VideoFrameQueue;
  VideoFrameQueue frames_;
  scoped_refptr<VideoFrame> current_frame_;

  // Used for accessing |frames_|.
  Lock lock_;

  // Used to signal |thread_| as frames are added to |frames_|.  Rule of thumb:
  // always check |state_| to see if it was set to STOPPED after waking up!
  ConditionVariable frame_available_;

  // Simple state tracking variable.
  enum State {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    STOPPED,
  };
  State state_;

  // Video thread handle.
  PlatformThreadHandle thread_;

  // Previous time returned from the pipeline.
  base::TimeDelta previous_time_;

  float playback_rate_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
