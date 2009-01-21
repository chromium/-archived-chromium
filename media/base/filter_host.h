// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FilterHost describes an interface for individual filters to access and
// modify global playback information.  Every filter is given a filter host
// reference as part of initialization.
//
// This interface is intentionally verbose to cover the needs for the different
// types of filters (see media/base/filters.h for filter definitionss).  Filters
// typically use parts of the interface that are relevant to their function.
// For example, an audio renderer filter typically calls SetTime as it feeds
// data to the audio hardware.  A video renderer filter typically calls GetTime
// to synchronize video with audio.  An audio and video decoder would typically
// have no need to call either SetTime or GetTime.
//
// The reasoning behind providing PostTask is to discourage filters from
// implementing their own threading.  The overall design is that many filters
// can share few threads and that notifications return quickly by scheduling
// processing with PostTask.

#ifndef MEDIA_BASE_FILTER_HOST_H_
#define MEDIA_BASE_FILTER_HOST_H_

#include "base/task.h"
#include "media/base/pipeline.h"

namespace media {

class FilterHost {
 public:
  // The PipelineStatus class allows read-only access to the pipeline state.
  // This is the same object that is used by the pipeline client to examine
  // the state of the running pipeline.  The lifetime of the PipelineStatus
  // interface is the same as the lifetime of the FilterHost interface, so
  // it is acceptable for filters to use the returned pointer until their
  // Stop method has been called.
  virtual const PipelineStatus* GetPipelineStatus() const = 0;

  // Registers a callback to receive global clock update notifications.  The
  // callback will be called repeatedly and filters do not need to re-register
  // after each invocation of the callback.  To remove the callback, filters
  // may call this method passing NULL for the callback argument.
  //
  // Callback arguments:
  //   int64    the new pipeline time, in microseconds
  virtual void SetTimeUpdateCallback(Callback1<int64>::Type* callback) = 0;

  // Filters must call this method to indicate that their initialization is
  // complete.  They may call this from within their Initialize() method or may
  // choose call it after processing some data.
  virtual void InitializationComplete() = 0;

  // Posts a task to be executed asynchronously on the pipeline's thread.
  virtual void PostTask(Task* task) = 0;

  // Stops execution of the pipeline due to a fatal error.
  virtual void Error(PipelineError error) = 0;

  // Sets the current time.  Any filters that have registered a callback through
  // the SetTimeUpdateCallback method will be notified of the change.
  virtual void SetTime(int64 time) = 0;

  // Get the duration of the media in microseconds.  If the duration has not
  // been determined yet, then returns 0.
  virtual void SetDuration(int64 duration) = 0;

  // Set the approximate amount of playable data buffered so far in micro-
  // seconds.
  virtual void SetBufferedTime(int64 buffered_time) = 0;

  // Set the total size of the media file.
  virtual void SetTotalBytes(int64 total_bytes) = 0;

  // Sets the total number of bytes that are buffered on the client and ready to
  // be played.
  virtual void SetBufferedBytes(int64 buffered_bytes) = 0;

  // Sets the size of the video output in pixel units.
  virtual void SetVideoSize(size_t width, size_t height) = 0;

 protected:
  virtual ~FilterHost() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_HOST_H_
