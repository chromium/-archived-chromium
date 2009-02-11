// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// The video renderer implementation to be use by the media pipeline. It lives
// inside video renderer thread and also WebKit's main thread. We need to be
// extra careful about members shared by two different threads, especially
// video frame buffers.
//
// Methods called from WebKit's main thread:
//   Paint()
//   SetRect()

#ifndef CHROME_RENDERER_MEDIA_VIDEO_RENDERER_IMPL_H_
#define CHROME_RENDERER_MEDIA_VIDEO_RENDERER_IMPL_H_

#include <deque>

#include "base/lock.h"
#include "base/task.h"
#include "base/gfx/platform_canvas.h"
#include "chrome/renderer/webmediaplayer_delegate_impl.h"
#include "base/gfx/rect.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "webkit/glue/webmediaplayer_delegate.h"

class SubmitReadsTask;

class VideoRendererImpl : public media::VideoRenderer {
 public:
  // media::MediaFilter implementation.
  virtual void Stop();

  // media::VideoRenderer implementation.
  virtual bool Initialize(media::VideoDecoder* decoder);

  // Methods for painting called by the WebMediaPlayerDelegateImpl
  // TODO(ralphl): What the *$*%##@ is this?  What does it mean?  How do we
  // treat the "rect"?  Is is clipping?
  virtual void SetRect(const gfx::Rect& rect);

  // TODO(ralphl): What is this rect?  Is it relative to the canvas?
  virtual void Paint(skia::PlatformCanvas* canvas, const gfx::Rect& rect);

  // Static method for creating factory for this object.
  static media::FilterFactory* CreateFactory(
      WebMediaPlayerDelegateImpl* delegate) {
    return new media::FilterFactoryImpl1<
        VideoRendererImpl, WebMediaPlayerDelegateImpl*>(delegate);
  }

  // Implementation of AssignableBuffer<this>::OnAssignment method.
  void OnAssignment(media::VideoFrame* video_frame);

 private:
  friend class SubmitReadsTask;
  friend class media::FilterFactoryImpl1<VideoRendererImpl,
                                         WebMediaPlayerDelegateImpl*>;

  // Constructor and destructor are private.  Only the filter factory is
  // allowed to create instances.
  explicit VideoRendererImpl(WebMediaPlayerDelegateImpl* delegate);
  virtual ~VideoRendererImpl();

  // Answers question from the factory to see if we accept |format|.
  static bool IsMediaFormatSupported(const media::MediaFormat* format);

  // Used by the IsMediaFormatSupported and Initialize methods.  Examines the
  // |media_format| and returns true if the format is supported.  Both output
  // parameters, |width_out| and |height_out| are required and must not be NULL.
  static bool ParseMediaFormat(const media::MediaFormat* media_format,
                               int* width_out,
                               int* height_out);

  // Used internally to post a task that will call the SubmitReads() method.
  // The |lock_| must be acquired before calling this method.  If the value of
  // |number_of_reads_needed_| is 0 or if there is already a pending task then
  // this method simply returns and does not post a new task.
  void PostSubmitReadsTask();

  // Examines the |number_of_reads_needed_| member and calls the decoder to
  // read the necessary number of frames.
  void SubmitReads();

  // For simplicity, we use the |delegate_| member to indicate if we have been
  // stopped or not.
  bool IsRunning() const { return (delegate_ != NULL); }

  // Throw away all frames in the queue.  The |lock_| must have been acquired
  // before calling this method.
  void DiscardAllFrames();

  // This method is always called with the object's |lock_| acquired..
  // The bool return value indicates weather or not the front of the queue has
  // been updated.  If this method returns true, then the front of the queue
  // is a new video frame, otherwise, the front is the same as the last call.
  // Given the current |time|, this method updates the state of the video frame
  // queue.  The caller may pass in a |new_frame| which will be added to the
  // queue in the appropriate position based on the frame's timestamp.  The
  // |front_frame_out| and |time_next_frame_out| parameters are both optional
  // and can be NULL.  If |front_frame_out| is non-NULL, then it is returned as
  // either NULL, which indicates that there are no frames in the queue, or
  // it will be a pointer to the frame at the front of the queue.  NOTE:  THIS
  // FRAME'S REFERENCE COUNT HAS BEEN INCREMENTED BY THIS CALL.  THE
  // CALLING FUNCTION MUST CALL |front_frame_out|->Release();
  // If the |time_next_frame_out| parameter is non-NULL then it will be assigned
  // the stream time of the next frame in the queue.  This is used by the Paint
  // method to schedule a time update callback at the appropriate presentation
  // time for the next frame.
  bool UpdateQueue(base::TimeDelta time,
                   media::VideoFrame* new_frame,
                   media::VideoFrame** front_frame_out,
                   base::TimeDelta* time_next_frame_out);

  // Internal method used by the Paint method to convert the specified video
  // frame to RGB, placing the converted pixels in the |current_frame_| bitmap.
  void CopyToCurrentFrame(media::VideoFrame* video_frame);

  // Called when the clock is updated by the audio renderer of when a scheduled
  // callback is called based on the interpolated current position of the media
  // stream.
  void TimeUpdateCallback(base::TimeDelta time);

  // Critical section.  There is only one for this object.  Used to serialize
  // access to the following members:
  //  |queue_| for obvious reasons
  //  |delegate_| because it is used by methods that can be called from random
  //      threads to determine if the render has been stopped (it will
  //      be NULL if stopped)
  //  |submit_reads_task_| to prevent multiple scheduling of the task and to
  //      allow for safe cancelation of the task.
  //  |current_frame_timestamp_| because member is used by the render
  //      thread in the Paint method and by DiscardAllFrames which can be
  //      called on the decoder's thread when a seek occurs.
  //  |number_of_reads_needed_| is modified by UpdateQueue from the decoder
  //      thread, the renderer thread, and the pipeline thread.
  //  |preroll_complete_| has a very small potential race condition if the
  //      OnAssignment method were reentered for the last frame in the queue
  //      and an end-of-stream frame.
  Lock lock_;

  // Pointer to our parent object that is called to request repaints.
  WebMediaPlayerDelegateImpl* delegate_;

  // Pointer to the decoder that will feed us with video frames.
  scoped_refptr<media::VideoDecoder> decoder_;

  // If non-NULL then a task has been scheduled to submit read requests to the
  // video decoder.
  CancelableTask* submit_reads_task_;

  // The number of buffers we need to request.  This member is updated by any
  // method that removes frames from the queue, such as UpdateQueue and
  // DiscardAllFrames.
  int number_of_reads_needed_;

  // An RGB bitmap of the current frame.  Note that we use the timestamp to
  // determine if the frame contents need to be color space converted, or if the
  // |current_frame_| member contains the correct image for the queue front.
  SkBitmap current_frame_;
  base::TimeDelta current_frame_timestamp_;

  // TODO(ralphl):  Try to understand all of the various "rect" and dimension
  // aspects of the renderer and document it so that other people can understand
  // it too.  I can not accurately describe what the role of this member is now.
  gfx::Rect rect_;

  // The queue of video frames.  The front of the queue is the frame that should
  // be displayed.
  typedef std::deque<media::VideoFrame*> VideoFrameQueue;
  VideoFrameQueue queue_;

  // True if we have received a full queue of video frames from the decoder.
  // We don't call FilterHost::InitializationComplete() until the the queue
  // is full.
  bool preroll_complete_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImpl);
};

#endif  // CHROME_RENDERER_MEDIA_VIDEO_RENDERER_IMPL_H_
