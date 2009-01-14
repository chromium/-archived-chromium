// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The pipeline is the public API clients use for playing back media.  Clients
// provide a filter factory containing the filters they want the pipeline to
// use to render media.
//
// Playback controls (play, pause, seek) are asynchronous and clients are
// notified via callbacks.

#ifndef MEDIA_BASE_PIPELINE_H_
#define MEDIA_BASE_PIPELINE_H_

#include "base/task.h"

namespace media {

class FilterFactoryInterface;

enum PipelineState {
  PS_UNINITIALIZED,
  PS_PLAY,
  PS_PAUSE,
  PS_SEEK,
  PS_SHUTDOWN
};

class Pipeline : public base::RefCountedThreadSafe<Pipeline> {
 public:
  // Build a pipeline to render the given URI using the given filter factory to
  // construct a filter chain.  Returns true if successful, false otherwise
  // (i.e., pipeline already initialized).
  //
  // This method is asynchronous and will execute a state change callback when
  // completed.
  virtual bool Initialize(FilterFactoryInterface* filter_factory,
                          const std::string& uri) = 0;

  // Attempt to play the media.  Returns true if successful, false otherwise
  // (i.e., pipeline is unititalized).
  //
  // This method is asynchronous and will execute a state change callback when
  // completed.
  virtual bool Play() = 0;

  // Attempt to pause the media.  Returns true if successful, false otherwise
  // (i.e., pipeline is unititalized).  Pause() is not a toggle and should not
  // resume playback if already paused.
  //
  // This method is asynchronous and will execute a state change callback when
  // completed.
  virtual bool Pause() = 0;

  // Attempt to seek to the position in microseconds.  Returns true if
  // successful, false otherwise.  Playback is paused after the seek completes.
  //
  // This method is asynchronous and will execute a state change callback when
  // completed.
  virtual bool Seek(int64 seek_position) = 0;

  // Shutdown the pipeline and destroy the filter chain.
  virtual void Shutdown() = 0;

  // Returns the current playback position in microseconds.
  virtual int64 GetTime() const = 0;

  // Returns the media duration in microseconds.  Returns zero if the duration
  // is not known (i.e., streaming media).
  virtual int64 GetDuration() const = 0;

  // Set a callback to receive state change notifications.  This callback is
  // executed only after the state transition has been successfully carried out.
  // For example, calling Play() will only execute a callback with PS_PLAY
  // some time in the future.
  virtual void SetStateChangedCallback(
      Callback1<PipelineState>::Type* callback) = 0;

  // Set a callback to receive time change notifications.
  virtual void SetTimeChangedCallback(Callback1<int64>::Type* callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<Pipeline>;
  virtual ~Pipeline() {}
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_H_