// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// A base class that provides the plumbing for a video renderer.  Derived
// classes must implement the following methods:
//   OnInitialized
//   OnStop (optional)
//   OnPaintNeeded
//
// The derived class can determine what frame needs to be painted by calling
// the GetCurrentFrame method.

#ifndef MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
#define MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_

#include <deque>

#include "base/lock.h"
#include "base/task.h"
#include "media/base/filters.h"

namespace media {

//------------------------------------------------------------------------------

// Contains the core logic for submitting reads to the video decoder, queueing
// the decoded frames, and waking up when the next frame needs to be drawn.
// The actual renderer class that is derived from this base class is responsible
// for actually drawing the video frames.  When it is time for the next frame
// in the queue to be drawn, the OnPaintNeeded() will be called to notify the
// derived renderer that it needs to call GetCurrentFrame() and paint.
class VideoRendererBase : public VideoRenderer {
 public:
  // MediaFilter implementation.
  virtual void Stop();

  // VideoRenderer implementation.
  virtual bool Initialize(VideoDecoder* decoder);

  // Implementation of AssignableBuffer<this>::OnAssignment method.
  void OnAssignment(VideoFrame* video_frame);

 protected:
  // Default tuning parameter values used to initialize the base class.
  static const size_t           kDefaultNumberOfFrames;
  static const base::TimeDelta  kDefaultSkipFrameDelta;
  static const base::TimeDelta  kDefaultEmptyQueueSleep;

  // Constructor that uses defaults for all tuning parameters.
  VideoRendererBase();

  // Constructor allows derived class to specify the tuning parameters used
  // by the base class.  See the comments for the member variables
  // |number_of_frames_|, |skip_frame_delta_|, and |empty_queue_sleep_| for
  // details on the meaning of these parameters.
  VideoRendererBase(size_t number_of_frames,
                    base::TimeDelta skip_frame_delta,
                    base::TimeDelta empty_queue_sleep);

  virtual ~VideoRendererBase();

  // Method that must be implemented by the derived class.  Called from within
  // the VideoRenderer::Initialize method before any reads are submitted to
  // the decoder.  Returns true if successful, otherwise false indicates a
  // fatal error.
  virtual bool OnInitialize(size_t width, size_t height) = 0;

  // Method that may be implemented by the derived class if desired.  It will
  // be called from within the MediaFilter::Stop method prior to stopping the
  // base class.
  virtual void OnStop() {}

  // Method that must be implemented by the derived class.  When the base class
  // detects a situation that requires a repaint, it calls this method to
  // request that the derived class paint again.  This method can be called on
  // any thread, so typically derived classes would post a message or
  // otherwise wake up a paint thread, but it is acceptable to call the
  // GetCurrentFrame() method from within OnPaintNeeded().
  virtual void OnPaintNeeded() = 0;

  // Gets the frame based on the current pipeline's time.  If the queue is
  // empty, |*frame_out| will be NULL, otherwise it will contain the frame
  // that should be displayed.
  void GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out);

  // Answers question from the factory to see if we accept |format|.
  static bool IsMediaFormatSupported(const MediaFormat& format);

  // Used by the IsMediaFormatSupported and Initialize methods.  Examines the
  // |media_format| and returns true if the format is supported.  Both output
  // parameters, |width_out| and |height_out| are required and must not be NULL.
  static bool ParseMediaFormat(const MediaFormat& media_format,
                               int* width_out,
                               int* height_out);

 private:
  // Used internally to post a task that will call the SubmitReads() method.
  // The |lock_| must be acquired before calling this method.  If the value of
  // |number_of_reads_needed_| is 0 or if there is already a pending task then
  // this method simply returns and does not post a new task.
  void PostSubmitReadsTask();

  // Examines the |number_of_reads_needed_| member and calls the decoder to
  // read the necessary number of frames.
  void SubmitReads();

  // Throw away all frames in the queue.  The |lock_| must have been acquired
  // before calling this method.
  void DiscardAllFrames();

  // This method is always called with the object's |lock_| acquired..
  // The bool return value indicates weather or not the front of the queue has
  // been updated.  If this method returns true, then the front of the queue
  // is a new video frame, otherwise, the front is the same as the last call.
  // Given the current |time|, this method updates the state of the video frame
  // queue.  The caller may pass in a |new_frame| which will be added to the
  // queue in the appropriate position based on the frame's timestamp.
  bool UpdateQueue(base::TimeDelta time, VideoFrame* new_frame);

  // Called when the clock is updated by the audio renderer of when a scheduled
  // callback is called based on the interpolated current position of the media
  // stream.
  void TimeUpdateCallback(base::TimeDelta time);

  // For simplicity, we use the |decoder_| member to indicate if we have been
  // stopped or not.  If this method is called on a thread that is not the
  // pipeline thread (the thread that the renderer was created on) then the
  // caller must hold the |lock_| and must continue to hold the |lock_| after
  // this check when accessing any member variables of this class.  See comments
  // on the |lock_| member for details.
  bool IsRunning() const { return (decoder_.get() != NULL); }

  // Critical section.  There is only one for this object.  Used to serialize
  // access to the following members:
  //  |queue_| for obvious reasons
  //  |decoder_| because it is used by methods that can be called from random
  //      threads to determine if the renderer has been stopped (it will be
  //      NULL if stopped)
  //  |submit_reads_task_| to prevent multiple scheduling of the task and to
  //      allow for safe cancelation of the task.
  //  |number_of_reads_needed_| is modified by UpdateQueue from the decoder
  //      thread, the renderer thread, and the pipeline thread.
  //  |preroll_complete_| has a very small potential race condition if the
  //      OnAssignment method were reentered for the last frame in the queue
  //      and an end-of-stream frame.
  Lock lock_;

  // Pointer to the decoder that will feed us with video frames.
  scoped_refptr<VideoDecoder> decoder_;

  // Number of frames either in the |queue_| or pending in the video decoder's
  // read queue.
  const size_t number_of_frames_;

  // The amount of time to skip ahead to the next frame is one is available.
  // If there are at least two frames in the queue, and the current time is
  // within the |skip_frame_delta_| from the next frame's timestamp, then the
  // UpdateQueue method will skip to the next frame.
  const base::TimeDelta skip_frame_delta_;

  // The UpdateQueue method will return the current time + |empty_queue_sleep_|
  // if there are no frames in the |queue_| but the caller has requested the
  // timestamp of the next frame.
  const base::TimeDelta empty_queue_sleep_;

  // The number of buffers we need to request.  This member is updated by any
  // method that removes frames from the queue, such as UpdateQueue and
  // DiscardAllFrames.
  int number_of_reads_needed_;

  // If non-NULL then a task has been scheduled to submit read requests to the
  // video decoder.
  CancelableTask* submit_reads_task_;

  // True if we have received a full queue of video frames from the decoder.
  // We don't call FilterHost::InitializationComplete() until the the queue
  // is full.
  bool preroll_complete_;

  // The queue of video frames.  The front of the queue is the frame that should
  // be displayed.
  typedef std::deque<VideoFrame*> VideoFrameQueue;
  VideoFrameQueue queue_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_RENDERER_BASE_H_
