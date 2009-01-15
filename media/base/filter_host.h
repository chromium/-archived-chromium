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
// Filter state is managed by the FilterHost implementor, with the filter
// receiving notifications from the host when a state transition is starting
// and the filter notifying the host when the filter has completed the
// transition.  The state transition is broken into two steps since some state
// transitions may be blocking or long running.  The host provides PostTask to
// help filters schedule such tasks.
//
// Example of a pause state transition:
//   During Initialization:
//     - Audio renderer registers OnPause with SetPauseCallback
//
//   During Playback:
//     - User hits pause button, triggering a pause state transition
//     - Filter host executes the pause callback
//     - Inside OnPause, the audio renderer schedules DoPause with PostTask
//       and immediately returns
//     - Filter host asynchronously executes DoPause
//     - Inside DoPause, the audio renderer does its blocking operations and
//       when complete calls PauseComplete
//
// The reasoning behind providing PostTask is to discourage filters from
// implementing their own threading.  The overall design is that many filters
// can share few threads and that notifications return quickly by scheduling
// processing with PostTask.

#ifndef MEDIA_BASE_FILTER_HOST_H_
#define MEDIA_BASE_FILTER_HOST_H_

#include "base/task.h"

namespace media {

class FilterHost {
 public:
  // Returns the global time.
  virtual int64 GetTime() const = 0;

  // Updates the global time.
  virtual void SetTime(int64 time) = 0;

  // Returns the global duration.
  virtual int64 GetDuration() const = 0;

  // Updates the global media duration.
  virtual void SetDuration(int64 duration) = 0;

  // Posts a task to be executed asynchronously.
  virtual void PostTask(Task* task) = 0;

  // Notifies the host that the filter has transitioned into the playing state.
  virtual bool PlayComplete() = 0;

  // Notifies the host that the filter has transitioned into the paused state.
  virtual bool PauseComplete() = 0;

  // Notifies the host that the filter has transitioned into the seek state.
  virtual bool SeekComplete() = 0;

  // Notifies the host that the filter has transitioned into the shutdown state.
  virtual bool ShutdownComplete() = 0;

  // Notifies the host that an error has occurred and that further processing
  // cannot continue.  |error| identifies the type of error that occurred.
  //
  // TODO(scherkus): Add error constants as we start implementing filters.
  virtual void Error(int error) = 0;

  // Notifies the host that the end of the stream has been reached.
  virtual void EndOfStream() = 0;

  // Registers a callback to handle the play state transition. The filter must
  // call PlayComplete at some point in the future to signal completion of
  // the transition.
  //
  // Callback arguments:
  //   None
  virtual void SetPlayCallback(Callback0::Type* callback) = 0;

  // Registers a callback to handle the pause state transition.  The filter must
  // call PauseComplete at some point in the future to signal completion of
  // the transition.
  //
  // Callback arguments:
  //   bool     true if the pause was triggered by end of stream
  virtual void SetPauseCallback(Callback1<bool>::Type* callback) = 0;

  // Registers a callback to handle the seek state transition.  The filter must
  // call SeekComplete at some point in the future to signal completion of
  // the transition.
  //
  // Callback arguments:
  //   int64    the timestamp position to seek to, in microseconds
  virtual void SetSeekCallback(Callback1<int64>::Type* callback) = 0;

  // Registers a callback to handle the shutdown state transition.  The filter
  // must call ShutdownComplete at some point in the future to signal completion
  // of the transition.
  //
  // Callback arguments:
  //   None
  virtual void SetShutdownCallback(Callback0::Type* callback) = 0;

  // Registers a callback to receive global clock update notifications.
  //
  // Callback arguments:
  //   int64    the new global time, in microseconds
  virtual void SetClockCallback(Callback1<int64>::Type* callback) = 0;

  // Registers a callback to receive global error notifications.
  //
  // Callback arguments:
  //   int      the error code reported.
  virtual void SetErrorCallback(Callback1<int>::Type* callback) = 0;

 protected:
  virtual ~FilterHost() {}
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_HOST_H_
