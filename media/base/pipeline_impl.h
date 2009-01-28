// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of Pipeline.

#ifndef MEDIA_BASE_PIPELINE_IMPL_H_
#define MEDIA_BASE_PIPELINE_IMPL_H_

#include <string>
#include <vector>

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
  ~PipelineImpl();

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
  virtual PipelineError GetError() const;

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

  // Called directly by the FilterHostImpl object to set the video size.
  void SetVideoSize(size_t width, size_t height);

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

  // Video width and height.  Set by a FilterHostImpl object on behalf
  // of a filter.  The video_size_access_lock_ is used to make sure access
  // to the pair of width and height are modified or read in thread safe way.
  Lock video_size_access_lock_;
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

  // Status of the pipeline.  Initialized to PIPELINE_OK which indicates that
  // the pipeline is operating correctly. Any other value indicates that the
  // pipeline is stopped or is stopping.  Clients can call the Stop method to
  // reset the pipeline state, and restore this to PIPELINE_OK.
  PipelineError error_;

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

  // Called by a FilterHostImpl on behalf of a filter calling FilerHost::Error.
  // If the pipeline is running a nested message loop, it will be exited.
  void Error(PipelineError error);

  // Called by a FilterHostImpl on behalf of a filter that calls the
  // FilterHost::PostTask method.
  void PostTask(Task* task);

  // Simple accessor used by the FilterHostImpl class to get access to the
  // pipeline object.
  PipelineImpl* pipeline() const { return pipeline_; }

 private:
  // Implementation of MessageLoop::DestructionObserver.  StartTask registers
  // this class as a destruction observer on the thread's message loop.
  // It is used to destroy the list of FilterHosts
  // (and thus destroy the associated filters) when all tasks have been
  // processed and the message loop has been quit.
  virtual void WillDestroyCurrentMessageLoop();

  friend class base::RefCountedThreadSafe<PipelineThread>;
  virtual ~PipelineThread();

  MessageLoop* message_loop() const { return thread_.message_loop(); }

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

  // Examines the demuxer filter output streams.  If one contains video then
  // returns true.
  bool HasVideo() const;

  // The following template funcions make use of the fact that media filter
  // derived interfaces are self-describing in the sense that they all contain
  // the static method filter_type() which returns a FilterType enum that
  // uniquely identifies the filer's interface.  In addition, filters that are
  // specific to audio or video also support a static method major_mime_type()
  // which returns a string of "audio/" or "video/".

  // Uses the FilterFactory to create a new filter of the NewFilter class, and
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
  template <class NewFilter, class Source>
  bool CreateFilter(FilterFactory* filter_factory,
                    Source source,
                    const MediaFormat* source_media_format);

  // Creates a DataSource (the first filter in a pipeline), and initializes it
  // with the specified URL.
  bool CreateDataSource(FilterFactory* filter_factory,
                        const std::string& url);

  // Examines the list of existing filters to find a Source, then creates a
  // NewFilter, and initializes it with the Source filter.
  template <class NewFilter, class Source>
  bool CreateAndConnect(FilterFactory* filter_factory);

  // Creates and initiaializes a decoder.
  template <class Decoder>
  bool CreateDecoder(FilterFactory* filter_factory);

  // Examine the list of existing filters to find one that supports the
  // specified Filter interface. If one exists, the specified Filter interface
  // is returned otherwise the method retuns NULL.
  template <class Filter>
  Filter* GetFilter() const;

  // Simple function that returns true if the specified MediaFormat object
  // has a mime type that matches the major_mime_type.  Examples of major mime
  // types are "audio/" and "video/"
  bool IsMajorMimeType(const MediaFormat* media_format,
                       const std::string& major_mime_type) const;

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
