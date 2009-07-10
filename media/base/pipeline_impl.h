// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of Pipeline.

#ifndef MEDIA_BASE_PIPELINE_IMPL_H_
#define MEDIA_BASE_PIPELINE_IMPL_H_

#include <string>
#include <vector>
#include <set>

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "base/time.h"
#include "media/base/pipeline.h"

namespace media {

class FilterHostImpl;
class PipelineInternal;

// Class which implements the Media::Pipeline contract.  The majority of the
// actual code for this object lives in the PipelineInternal class, which is
// responsible for actually building and running the pipeline.  This object
// is basically a simple container for state information, and is responsible
// for creating and communicating with the PipelineInternal object.
class PipelineImpl : public Pipeline {
 public:
  PipelineImpl(MessageLoop* message_loop);
  virtual ~PipelineImpl();

  // Pipeline implementation.
  virtual bool Start(FilterFactory* filter_factory,
                     const std::string& uri,
                     PipelineCallback* start_callback);
  virtual void Stop(PipelineCallback* stop_callback);
  virtual void Seek(base::TimeDelta time, PipelineCallback* seek_callback);
  virtual bool IsRunning() const;
  virtual bool IsInitialized() const;
  virtual bool IsRendered(const std::string& major_mime_type) const;
  virtual float GetPlaybackRate() const;
  virtual void SetPlaybackRate(float playback_rate);
  virtual float GetVolume() const;
  virtual void SetVolume(float volume);
  virtual base::TimeDelta GetTime() const;
  virtual base::TimeDelta GetBufferedTime() const;
  virtual base::TimeDelta GetDuration() const;
  virtual int64 GetBufferedBytes() const;
  virtual int64 GetTotalBytes() const;
  virtual void GetVideoSize(size_t* width_out, size_t* height_out) const;
  virtual PipelineError GetError() const;

 private:
  friend class FilterHostImpl;
  friend class PipelineInternal;

  // Reset the state of the pipeline object to the initial state.  This method
  // is used by the constructor, and the Stop method.
  void ResetState();

  // Used internally to make sure that the thread is in a state that is
  // acceptable to post a task to.  It must exist, be initialized, and there
  // must not be an error.
  bool IsPipelineOk() const;

  // Methods called by FilterHostImpl to update pipeline state.
  void SetDuration(base::TimeDelta duration);
  void SetBufferedTime(base::TimeDelta buffered_time);
  void SetTotalBytes(int64 total_bytes);
  void SetBufferedBytes(int64 buffered_bytes);
  void SetVideoSize(size_t width, size_t height);
  void SetTime(base::TimeDelta time);
  void InternalSetPlaybackRate(float rate);

  // Sets the error to the new error code only if the current error state is
  // PIPELINE_OK. Returns true if error set, otherwise leaves current error
  // alone, and returns false.
  bool InternalSetError(PipelineError error);

  // Method called by the |pipeline_internal_| to insert a mime type into
  // the |rendered_mime_types_| set.
  void InsertRenderedMimeType(const std::string& major_mime_type);

  // Message loop used to execute pipeline tasks.
  MessageLoop* message_loop_;

  // Holds a ref counted reference to the PipelineInternal object associated
  // with this pipeline.  Prior to the call to the Start() method, this member
  // will be NULL, since we are not running.
  scoped_refptr<PipelineInternal> pipeline_internal_;

  // After calling Start, if all of the required filters are created and
  // initialized, this member will be set to true by the pipeline thread.
  bool initialized_;

  // Duration of the media in microseconds.  Set by a FilterHostImpl object on
  // behalf of a filter.
  base::TimeDelta duration_;

  // Amount of available buffered data in microseconds.  Set by a
  // FilterHostImpl object on behalf of a filter.
  base::TimeDelta buffered_time_;

  // Amount of available buffered data.  Set by a FilterHostImpl object
  // on behalf of a filter.
  int64 buffered_bytes_;

  // Total size of the media.  Set by a FilterHostImpl object on behalf
  // of a filter.
  int64 total_bytes_;

  // Lock used to serialize access for getter/setter methods.
  mutable Lock lock_;

  // Video width and height.  Set by a FilterHostImpl object on behalf
  // of a filter.  The video_size_access_lock_ is used to make sure access
  // to the pair of width and height are modified or read in thread safe way.
  size_t video_width_;
  size_t video_height_;

  // Current volume level (from 0.0f to 1.0f).  The volume reflects the last
  // value the audio filter was called with SetVolume, so there will be a short
  // period of time between the client calling SetVolume on the pipeline and
  // this value being updated.  Set by the PipelineInternal just prior to
  // calling the audio renderer.
  float volume_;

  // Current playback rate (>= 0.0f).  This member reflects the last value
  // that the filters in the pipeline were called with, so there will be a short
  // period of time between the client calling SetPlaybackRate and this value
  // being updated.  Set by the PipelineInternal just prior to calling filters.
  float playback_rate_;

  // Current playback time.  Set by a FilterHostImpl object on behalf of the
  // audio renderer filter.
  base::TimeDelta time_;

  // Status of the pipeline.  Initialized to PIPELINE_OK which indicates that
  // the pipeline is operating correctly. Any other value indicates that the
  // pipeline is stopped or is stopping.  Clients can call the Stop method to
  // reset the pipeline state, and restore this to PIPELINE_OK.
  PipelineError error_;

  // Vector of major mime types that have been rendered by this pipeline.
  typedef std::set<std::string> RenderedMimeTypesSet;
  RenderedMimeTypesSet rendered_mime_types_;

  DISALLOW_COPY_AND_ASSIGN(PipelineImpl);
};


// PipelineInternal contains most of the logic involved with running the
// media pipeline. Filters are created and called on the message loop injected
// into this object. PipelineInternal works like a state machine to perform
// asynchronous initialization. Initialization is done in multiple passes by
// InitializeTask(). In each pass a different filter is created and chained with
// a previously created filter.
//
// Here's a state diagram that describes the lifetime of this object.
//
// [ *Created ] -> [ InitDataSource ] -> [ InitDemuxer ] ->
// [ InitAudioDecoder ] -> [ InitAudioRenderer ] ->
// [ InitVideoDecoder ] -> [ InitVideoRenderer ] -> [ Started ]
//        |                    |                         |
//        .-> [ Error ]        .->      [ Stopped ]    <-.
//
// Initialization is a series of state transitions from "Created" to
// "Started". If any error happens during initialization, this object will
// transition to the "Error" state from any state. If Stop() is called during
// initialization, this object will transition to "Stopped" state.

class PipelineInternal : public base::RefCountedThreadSafe<PipelineInternal> {
 public:
  // Methods called by PipelineImpl object on the client's thread.  These
  // methods post a task to call a corresponding xxxTask() method on the
  // message loop.  For example, Seek posts a task to call SeekTask.
  explicit PipelineInternal(PipelineImpl* pipeline, MessageLoop* message_loop);

  // After Start() is called, a task of StartTask() is posted on the pipeline
  // thread to perform initialization. See StartTask() to learn more about
  // initialization.
  void Start(FilterFactory* filter_factory,
             const std::string& url_media_source,
             PipelineCallback* start_callback);
  void Stop(PipelineCallback* stop_callback);
  void Seek(base::TimeDelta time, PipelineCallback* seek_callback);
  void SetPlaybackRate(float rate);
  void SetVolume(float volume);

  // Methods called by a FilterHostImpl object.  These methods may be called
  // on any thread, either the pipeline's thread or any other.

  // When a filter calls it's FilterHost, the filter host calls back to the
  // pipeline thread.  If the pipeline thread is running a nested message loop
  // then it will be exited.
  void InitializationComplete(FilterHostImpl* host);

  // Sets the pipeline time and schedules a task to call back to any filters
  // that have registered a time update callback.
  void SetTime(base::TimeDelta time);

  // Called by a FilterHostImpl on behalf of a filter calling FilterHost::Error.
  // If the pipeline is running a nested message loop, it will be exited.
  void Error(PipelineError error);

  // Simple accessor used by the FilterHostImpl class to get access to the
  // pipeline object.
  //
  // TODO(scherkus): I think FilterHostImpl should not be talking to
  // PipelineImpl but rather PipelineInternal.
  PipelineImpl* pipeline() const { return pipeline_; }

  // Returns true if the pipeline has fully initialized.
  bool IsInitialized() { return state_ == kStarted; }

 private:
  // Only allow ourselves to be destroyed via ref-counting.
  friend class base::RefCountedThreadSafe<PipelineInternal>;
  virtual ~PipelineInternal();

  enum State {
    kCreated,
    kInitDataSource,
    kInitDemuxer,
    kInitAudioDecoder,
    kInitAudioRenderer,
    kInitVideoDecoder,
    kInitVideoRenderer,
    kStarted,
    kStopped,
    kError,
  };

  // Simple method used to make sure the pipeline is running normally.
  bool IsPipelineOk() { return PIPELINE_OK == pipeline_->error_; }

  // Helper method to tell whether we are in the state of initializing.
  bool IsPipelineInitializing() {
    return state_ == kInitDataSource ||
           state_ == kInitDemuxer ||
           state_ == kInitAudioDecoder ||
           state_ == kInitAudioRenderer ||
           state_ == kInitVideoDecoder ||
           state_ == kInitVideoRenderer;
  }

  // The following "task" methods correspond to the public methods, but these
  // methods are run as the result of posting a task to the PipelineInternal's
  // message loop.
  void StartTask(FilterFactory* filter_factory,
                 const std::string& url,
                 PipelineCallback* start_callback);

  // InitializeTask() performs initialization in multiple passes. It is executed
  // as a result of calling Start() or InitializationComplete() that advances
  // initialization to the next state. It works as a hub of state transition for
  // initialization.
  void InitializeTask();

  // StopTask() and ErrorTask() are similar but serve different purposes:
  //   - Both destroy the filter chain.
  //   - Both will execute |start_callback| if the pipeline was initializing.
  //   - StopTask() resets the pipeline to a fresh state, where as ErrorTask()
  //     leaves the pipeline as is for client inspection.
  //   - StopTask() can be scheduled by the client calling Stop(), where as
  //     ErrorTask() is scheduled as a result of a filter calling Error().
  void StopTask(PipelineCallback* stop_callback);
  void ErrorTask(PipelineError error);

  void SetPlaybackRateTask(float rate);
  void SeekTask(base::TimeDelta time, PipelineCallback* seek_callback);
  void SetVolumeTask(float volume);

  // Internal methods used in the implementation of the pipeline thread.  All
  // of these methods are only called on the pipeline thread.

  // The following template functions make use of the fact that media filter
  // derived interfaces are self-describing in the sense that they all contain
  // the static method filter_type() which returns a FilterType enum that
  // uniquely identifies the filter's interface.  In addition, filters that are
  // specific to audio or video also support a static method major_mime_type()
  // which returns a string of "audio/" or "video/".
  //
  // Uses the FilterFactory to create a new filter of the Filter class, and
  // initializes it using the Source object.  The source may be another filter
  // or it could be a string in the case of a DataSource.
  //
  // The CreateFilter() method actually does much more than simply creating the
  // filter.  It creates the FilterHostImpl object, creates the filter using
  // the filter factory, calls the MediaFilter::SetHost() method on the filter,
  // and then calls the filter's type-specific Initialize(source) method to
  // initialize the filter.  If the required filter cannot be created,
  // PIPELINE_ERROR_REQUIRED_FILTER_MISSING is raised, initialization is halted
  // and this object will remain in the "Error" state.
  //
  // Callers can optionally use the returned Filter for further processing,
  // but since the call already placed the filter in the list of filter hosts,
  // callers can ignore the return value.  In any case, if this function can
  // not create and initializes the specified Filter, then this method will
  // return with |pipeline_->error_| != PIPELINE_OK.
  template <class Filter, class Source>
  void CreateFilter(FilterFactory* filter_factory,
                    Source source,
                    const MediaFormat& source_media_format);

  // Creates a Filter and initializes it with the given |source|.  If a Filter
  // could not be created or an error occurred, this method returns NULL and the
  // pipeline's |error_| member will contain a specific error code.  Note that
  // the Source could be a filter or a DemuxerStream, but it must support the
  // GetMediaFormat() method.
  template <class Filter, class Source>
  void CreateFilter(FilterFactory* filter_factory, Source* source) {
    CreateFilter<Filter, Source*>(filter_factory,
                                  source,
                                  source->media_format());
  }

  // Creates a DataSource (the first filter in a pipeline).
  void CreateDataSource();

  // Creates a Demuxer.
  void CreateDemuxer();

  // Creates a decoder of type Decoder. Returns true if the asynchronous action
  // of creating decoder has started. Returns false if this method did nothing
  // because the corresponding audio/video stream does not exist.
  template <class Decoder>
  bool CreateDecoder();

  // Creates a renderer of type Renderer and connects it with Decoder. Returns
  // true if the asynchronous action of creating renderer has started. Returns
  // false if this method did nothing because the corresponding audio/video
  // stream does not exist.
  template <class Decoder, class Renderer>
  bool CreateRenderer();

  // Examine the list of existing filters to find one that supports the
  // specified Filter interface. If one exists, the |filter_out| will contain
  // the filter, |*filter_out| will be NULL.
  template <class Filter>
  void GetFilter(scoped_refptr<Filter>* filter_out) const;

  // Stops every filters, filter host and filter thread and releases all
  // references to them.
  void DestroyFilters();

  // Pointer to the pipeline that owns this PipelineInternal.
  PipelineImpl* pipeline_;

  // Message loop used to execute pipeline tasks.
  MessageLoop* message_loop_;

  // Member that tracks the current state.
  State state_;

  // Filter factory as passed in by Start().
  scoped_refptr<FilterFactory> filter_factory_;

  // URL for the data source as passed in by Start().
  std::string url_;

  // Callbacks for various pipeline operations.
  scoped_ptr<PipelineCallback> start_callback_;
  scoped_ptr<PipelineCallback> seek_callback_;
  scoped_ptr<PipelineCallback> stop_callback_;

  // Vector of FilterHostImpl objects that contain the filters for the pipeline.
  typedef std::vector<FilterHostImpl*> FilterHostVector;
  FilterHostVector filter_hosts_;

  typedef std::vector<base::Thread*> FilterThreadVector;
  FilterThreadVector filter_threads_;

  DISALLOW_COPY_AND_ASSIGN(PipelineInternal);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_IMPL_H_
