// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The pipeline is the public API clients use for playing back media.  Clients
// provide a filter factory containing the filters they want the pipeline to
// use to render media.

#ifndef MEDIA_BASE_PIPELINE_H_
#define MEDIA_BASE_PIPELINE_H_

#include <string>

#include "base/task.h"
#include "media/base/factory.h"

namespace base {
class TimeDelta;
}

namespace media {

// Error definitions for pipeline.  All codes indicate an error except:
// PIPELINE_OK indicates the pipeline is running normally.
// PIPELINE_STOPPING is used internally when Pipeline::Stop() is called.
enum PipelineError {
  PIPELINE_OK,
  PIPELINE_STOPPING,
  PIPELINE_ERROR_URL_NOT_FOUND,
  PIPELINE_ERROR_NETWORK,
  PIPELINE_ERROR_DECODE,
  PIPELINE_ERROR_ABORT,
  PIPELINE_ERROR_INITIALIZATION_FAILED,
  PIPELINE_ERROR_REQUIRED_FILTER_MISSING,
  PIPELINE_ERROR_OUT_OF_MEMORY,
  PIPELINE_ERROR_COULD_NOT_RENDER,
  PIPELINE_ERROR_READ,
  PIPELINE_ERROR_AUDIO_HARDWARE,
  PIPELINE_ERROR_NO_DATA,
  // Demuxer related errors.
  DEMUXER_ERROR_COULD_NOT_OPEN,
  DEMUXER_ERROR_COULD_NOT_PARSE,
  DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
  DEMUXER_ERROR_COULD_NOT_CREATE_THREAD,
};

// Client-provided callbacks for various pipeline operations.
//
// TODO(scherkus): consider returning a PipelineError instead of a bool, or
// perhaps a client callback interface.
typedef Callback1<bool>::Type PipelineCallback;

class Pipeline {
 public:
  // Build a pipeline to render the given URL using the given filter factory to
  // construct a filter chain.  Returns true if successful, false otherwise
  // (i.e., pipeline already started).  Note that a return value of true
  // only indicates that the initialization process has started successfully.
  // Pipeline initialization is an inherently asynchronous process.  Clients can
  // either poll the IsInitialized() method (discouraged) or use the
  // |start_callback| as described below.
  //
  // This method is asynchronous and can execute a callback when completed.
  // If the caller provides a |start_callback|, it will be called when the
  // pipeline initialization completes.  If successful, the callback's bool
  // parameter will be true.  If the callback is called with false, then the
  // client can use GetError() to obtain more information about the reason
  // initialization failed.
  virtual bool Start(FilterFactory* filter_factory,
                     const std::string& url,
                     PipelineCallback* start_callback) = 0;

  // Asynchronously stops the pipeline and resets it to an uninitialized state.
  // If provided, |stop_callback| will be executed when the pipeline has been
  // completely torn down and reset to an uninitialized state.  It is acceptable
  // to call Start() again once the callback has finished executing.
  //
  // Stop() must be called before destroying the pipeline.  Clients can
  // determine whether Stop() must be called by checking IsRunning().
  //
  // TODO(scherkus): ideally clients would destroy the pipeline after calling
  // Stop() and create a new pipeline as needed.
  virtual void Stop(PipelineCallback* stop_callback) = 0;

  // Attempt to seek to the position specified by time.  |seek_callback| will be
  // executed when the all filters in the pipeline have processed the seek.
  // The callback will return true if the seek was carried out, false otherwise
  // (i.e., streaming media).
  virtual void Seek(base::TimeDelta time, PipelineCallback* seek_callback) = 0;

  // Returns true if the pipeline has been started via Start().  If IsRunning()
  // returns true, it is expected that Stop() will be called before destroying
  // the pipeline.
  virtual bool IsRunning() const = 0;

  // Returns true if the pipeline has been started and fully initialized to a
  // point where playback controls will be respected.  Note that it is possible
  // for a pipeline to be started but not initialized (i.e., an error occurred).
  virtual bool IsInitialized() const = 0;

  // If the |major_mime_type| exists in the pipeline and is being rendered, this
  // method will return true.  Types are defined in media/base/media_foramt.h.
  // For example, to determine if a pipeline contains video:
  //   bool has_video = pipeline->IsRendered(mime_type::kMajorTypeVideo);
  virtual bool IsRendered(const std::string& major_mime_type) const = 0;

  // Gets the current playback rate of the pipeline.  When the pipeline is
  // started, the playback rate will be 0.0f.  A rate of 1.0f indicates
  // that the pipeline is rendering the media at the standard rate.  Valid
  // values for playback rate are >= 0.0f.
  virtual float GetPlaybackRate() const = 0;

  // Attempt to adjust the playback rate. Setting a playback rate of 0.0f pauses
  // all rendering of the media.  A rate of 1.0f indicates a normal playback
  // rate.  Values for the playback rate must be greater than or equal to 0.0f.
  //
  // TODO(scherkus): What about maximum rate?  Does HTML5 specify a max?
  virtual void SetPlaybackRate(float playback_rate) = 0;

  // Gets the current volume setting being used by the audio renderer.  When
  // the pipeline is started, this value will be 1.0f.  Valid values range
  // from 0.0f to 1.0f.
  virtual float GetVolume() const = 0;

  // Attempt to set the volume of the audio renderer.  Valid values for volume
  // range from 0.0f (muted) to 1.0f (full volume).  This value affects all
  // channels proportionately for multi-channel audio streams.
  virtual void SetVolume(float volume) = 0;

  // Gets the current pipeline time. For a pipeline "time" progresses from 0 to
  // the end of the media.
  virtual base::TimeDelta GetTime() const = 0;

  // Get the approximate amount of playable data buffered so far in micro-
  // seconds.
  virtual base::TimeDelta GetBufferedTime() const = 0;

  // Get the duration of the media in microseconds.  If the duration has not
  // been determined yet, then returns 0.
  virtual base::TimeDelta GetDuration() const = 0;

  // Get the total number of bytes that are buffered on the client and ready to
  // be played.
  virtual int64 GetBufferedBytes() const = 0;

  // Get the total size of the media file.  If the size has not yet been
  // determined or can not be determined, this value is 0.
  virtual int64 GetTotalBytes() const = 0;

  // Gets the size of the video output in pixel units.  If there is no video
  // or the video has not been rendered yet, the width and height will be 0.
  virtual void GetVideoSize(size_t* width_out, size_t* height_out) const = 0;

  // Gets the current error status for the pipeline.  If the pipeline is
  // operating correctly, this will return OK.
  virtual PipelineError GetError() const = 0;

 protected:
  virtual ~Pipeline() {}
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_H_
