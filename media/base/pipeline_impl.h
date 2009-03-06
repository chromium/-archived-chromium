// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
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
class PipelineThread;

// Class which implements the Media::Pipeline contract.  The majority of the
// actual code for this object lives in the PipelineThread class, which is
// responsible for actually building and running the pipeline.  This object
// is basically a simple container for state information, and is responsible
// for creating and communicating with the PipelineThread object.
class PipelineImpl : public Pipeline {
 public:
  PipelineImpl();
  virtual ~PipelineImpl();

  // Implementation of PipelineStatus methods.
  virtual bool IsInitialized() const;
  virtual base::TimeDelta GetDuration() const;
  virtual base::TimeDelta GetBufferedTime() const;
  virtual int64 GetTotalBytes() const;
  virtual int64 GetBufferedBytes()const;
  virtual void GetVideoSize(size_t* width_out, size_t* height_out) const;
  virtual float GetVolume() const;
  virtual float GetPlaybackRate() const;
  virtual base::TimeDelta GetTime() const;
  virtual base::TimeDelta GetInterpolatedTime() const;
  virtual PipelineError GetError() const;
  virtual bool IsRendered(const std::string& major_mime_type) const;

  // Impementation of Pipeline methods.
  virtual bool Start(FilterFactory* filter_factory,
                     const std::string& url,
                     Callback1<bool>::Type* init_complete_callback);
  virtual void Stop();
  virtual void SetPlaybackRate(float rate);
  virtual void Seek(base::TimeDelta time);
  virtual void SetVolume(float volume);

 private:
  friend class FilterHostImpl;
  friend class PipelineThread;

  // Reset the state of the pipline object to the initial state.  This method
  // is used by the constructor, and the Stop method.
  void ResetState();

  // Used internally to make sure that the thread is in a state that is
  // acceptable to post a task to.  It must exist, be initialized, and there
  // must not be an error.
  bool OkToCallThread() const {
    return (pipeline_thread_ && initialized_ && PIPELINE_OK == error_);
  }

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

  // Method called by the |pipeline_thread_| to insert a mime type into
  // the |rendered_mime_types_| set.
  void InsertRenderedMimeType(const std::string& major_mime_type);

  // Holds a ref counted reference to the PipelineThread object associated
  // with this pipeline.  Prior to the call to the Start method, this member
  // will be NULL, since no thread is running.
  scoped_refptr<PipelineThread> pipeline_thread_;

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
  Lock lock_;

  // Video width and height.  Set by a FilterHostImpl object on behalf
  // of a filter.  The video_size_access_lock_ is used to make sure access
  // to the pair of width and height are modified or read in thread safe way.
  size_t video_width_;
  size_t video_height_;

  // Current volume level (from 0.0f to 1.0f).  The volume reflects the last
  // value the audio filter was called with SetVolume, so there will be a short
  // period of time between the client calling SetVolume on the pipeline and
  // this value being updated.  Set by the PipelineThread just prior to calling
  // the audio renderer.
  float volume_;

  // Current playback rate (>= 0.0f).  This member reflects the last value
  // that the filters in the pipeline were called with, so there will be a short
  // period of time between the client calling SetPlaybackRate and this value
  // being updated.  Set by the PipelineThread just prior to calling filters.
  float playback_rate_;

  // Current playback time.  Set by a FilterHostImpl object on behalf of the
  // audio renderer filter.
  base::TimeDelta time_;

  // Internal system timer at last time the SetTime method was called.  Used to
  // compute interpolated time.
  base::TimeTicks ticks_at_last_set_time_;

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


// The PipelineThread contains most of the logic involved with running the
// media pipeline.  Filters are created and called on a dedicated thread owned
// by this object.
class PipelineThread : public base::RefCountedThreadSafe<PipelineThread>,
                       public MessageLoop::DestructionObserver {
 public:
  // Methods called by PipelineImpl object on the client's thread.  These
  // methods post a task to call a corresponding xxxTask() method on the
  // pipeline thread.  For example, Seek posts a task to call SeekTask.
  explicit PipelineThread(PipelineImpl* pipeline);

  bool Start(FilterFactory* filter_factory,
             const std::string& url_media_source,
             Callback1<bool>::Type* init_complete_callback);
  void Stop();
  void SetPlaybackRate(float rate);
  void Seek(base::TimeDelta time);
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

  // Called by a FilterHostImpl on behalf of a filter that calls the
  // FilterHost::PostTask method.
  void PostTask(Task* task);

  // Simple accessor used by the FilterHostImpl class to get access to the
  // pipeline object.
  PipelineImpl* pipeline() const { return pipeline_; }

  // Accessor used to post messages to thread's message loop.
  MessageLoop* message_loop() const { return thread_.message_loop(); }

 private:
  // Implementation of MessageLoop::DestructionObserver.  StartTask registers
  // this class as a destruction observer on the thread's message loop.
  // It is used to destroy the list of FilterHosts
  // (and thus destroy the associated filters) when all tasks have been
  // processed and the message loop has been quit.
  virtual void WillDestroyCurrentMessageLoop();

  friend class base::RefCountedThreadSafe<PipelineThread>;
  virtual ~PipelineThread();

  // Simple method used to make sure the pipeline is running normally.
  bool PipelineOk() { return PIPELINE_OK == pipeline_->error_; }

  // The following "task" methods correspond to the public methods, but these
  // methods are run as the result of posting a task to the PipelineThread's
  // message loop.  For example, the Start method posts a task to call the
  // StartTask message on the pipeline thread.
  void StartTask(FilterFactory* filter_factory,
                 const std::string& url,
                 Callback1<bool>::Type* init_complete_callback);
  void StopTask();
  void SetPlaybackRateTask(float rate);
  void SeekTask(base::TimeDelta time);
  void SetVolumeTask(float volume);
  void SetTimeTask();
  void InitializationCompleteTask(FilterHostImpl* FilterHost);

  // Internal methods used in the implementation of the pipeline thread.  All
  // of these methods are only called on the pipeline thread.

  // Calls the Stop method on every filter in the pipeline
  void StopFilters();

  // The following template funcions make use of the fact that media filter
  // derived interfaces are self-describing in the sense that they all contain
  // the static method filter_type() which returns a FilterType enum that
  // uniquely identifies the filter's interface.  In addition, filters that are
  // specific to audio or video also support a static method major_mime_type()
  // which returns a string of "audio/" or "video/".

  // Uses the FilterFactory to create a new filter of the Filter class, and
  // initiaializes it using the Source object.  The source may be another filter
  // or it could be a string in the case of a DataSource.
  //
  // The CreateFilter method actually does much more than simply creating the
  // filter.  It creates the FilterHostImpl object, creates the filter using
  // the filter factory, calls the MediaFilter::SetHost method on the filter,
  // and then calls the filter's type-specific Initialize(source) method to
  // initialize the filter.  It then runs the thread's message loop and waits
  // until one of the following occurs:
  //  1. The filter calls FilterHost::InitializationComplete()
  //  2. A filter calls FilterHost::Error()
  //  3. The client calls Pipeline::Stop()
  //
  // Callers can optionally use the returned Filter for further processing,
  // but since the call already placed the filter in the list of filter hosts,
  // callers can ignore the return value.  In any case, if this function can
  // not create and initailze the speified Filter, then this method will return
  // with |pipeline_->error_| != PIPELINE_OK.
  template <class Filter, class Source>
  scoped_refptr<Filter> CreateFilter(FilterFactory* filter_factory,
                                     Source source,
                                     const MediaFormat* source_media_format);

  // Creates a Filter and initilizes it with the given |source|.  If a Filter
  // could not be created or an error occurred, this metod returns NULL and the
  // pipeline's |error_| member will contain a specific error code.  Note that
  // the Source could be a filter or a DemuxerStream, but it must support the
  // GetMediaFormat() method.
  template <class Filter, class Source>
  scoped_refptr<Filter> CreateFilter(FilterFactory* filter_factory,
                                     Source* source) {
    return CreateFilter<Filter, Source*>(filter_factory,
                                         source,
                                         source->GetMediaFormat());
  }

  // Creates a DataSource (the first filter in a pipeline), and initializes it
  // with the specified URL.
  scoped_refptr<DataSource> CreateDataSource(FilterFactory* filter_factory,
                                             const std::string& url);

  // If the |demuxer| contains a stream that matches Decoder::major_media_type()
  // this method creates and initializes the specified Decoder and Renderer.
  // Callers should examine the |pipeline_->error_| member to see if there was
  // an error duing the call.  The lack of the specified stream does not
  // constitute an error, and no Decoder or Renderer will be created if the
  // data stream does not exist in the |demuxer|.  If a stream is rendered, then
  // this method will call |pipeline_|->InsertRenderedMimeType() to add the
  // mime type to the set of rendered major mime types for the pipeline.
  template <class Decoder, class Renderer>
  void Render(FilterFactory* filter_factory, Demuxer* demuxer);

  // Examine the list of existing filters to find one that supports the
  // specified Filter interface. If one exists, the |filter_out| will contain
  // the filter, |*filter_out| will be NULL.
  template <class Filter>
  void GetFilter(scoped_refptr<Filter>* filter_out) const;

  // Pointer to the pipeline that owns this PipelineThread.
  PipelineImpl* const pipeline_;

  // The actual thread.
  base::Thread thread_;

  // Used to avoid scheduling multiple time update tasks.  If this member is
  // true then a task that will call the SetTimeTask() method is in the message
  // loop's queue.
  bool time_update_callback_scheduled_;

  // During initialization of a filter, this member points to the FilterHostImpl
  // that is being initialized.
  FilterHostImpl* host_initializing_;

  // This lock is held through the entire StartTask method to prevent the
  // Stop method from quitting the nested message loop of the StartTask method.
  Lock initialization_lock_;

  // Vector of FilterHostImpl objects that contian the filters for the pipeline.
  typedef std::vector<FilterHostImpl*> FilterHostVector;
  FilterHostVector filter_hosts_;

  DISALLOW_COPY_AND_ASSIGN(PipelineThread);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_IMPL_H_
