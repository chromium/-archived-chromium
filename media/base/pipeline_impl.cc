// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "media/base/filter_host_impl.h"
#include "media/base/media_format.h"
#include "media/base/pipeline_impl.h"

namespace media {

PipelineImpl::PipelineImpl() {
  ResetState();
}

PipelineImpl::~PipelineImpl() {
  Stop();
}

bool PipelineImpl::IsInitialized() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  return initialized_;
}

base::TimeDelta PipelineImpl::GetDuration() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  return duration_;
}

base::TimeDelta PipelineImpl::GetBufferedTime() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  return buffered_time_;
}

int64 PipelineImpl::GetTotalBytes() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  return total_bytes_;
}

int64 PipelineImpl::GetBufferedBytes() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  return buffered_bytes_;
}

void PipelineImpl::GetVideoSize(size_t* width_out, size_t* height_out) const {
  DCHECK(width_out);
  DCHECK(height_out);
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  *width_out = video_width_;
  *height_out = video_height_;
}

float PipelineImpl::GetVolume() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  return volume_;
}

float PipelineImpl::GetPlaybackRate() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  return playback_rate_;
}

base::TimeDelta PipelineImpl::GetTime() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  return time_;
}

base::TimeDelta PipelineImpl::GetInterpolatedTime() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  base::TimeDelta time = time_;
  if (playback_rate_ > 0.0f) {
    base::TimeDelta delta = base::TimeTicks::Now() - ticks_at_last_set_time_;
    if (playback_rate_ == 1.0f) {
      time += delta;
    } else {
      int64 adjusted_delta = static_cast<int64>(delta.InMicroseconds() *
                                                playback_rate_);
      time += base::TimeDelta::FromMicroseconds(adjusted_delta);
    }
  }
  return time;
}

void PipelineImpl::SetTime(base::TimeDelta time) {
  AutoLock auto_lock(lock_);
  time_ = time;
  ticks_at_last_set_time_ = base::TimeTicks::Now();
}

void PipelineImpl::InternalSetPlaybackRate(float rate) {
  AutoLock auto_lock(lock_);
  if (playback_rate_ == 0.0f && rate > 0.0f) {
    ticks_at_last_set_time_ = base::TimeTicks::Now();
  }
  playback_rate_ = rate;
}


PipelineError PipelineImpl::GetError() const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  return error_;
}

bool PipelineImpl::InternalSetError(PipelineError error) {
  AutoLock auto_lock(lock_);
  bool changed_error = false;
  DCHECK(PIPELINE_OK != error);
  if (PIPELINE_OK == error_) {
    error_ = error;
    changed_error = true;
  }
  return changed_error;
}

// Creates the PipelineThread and calls it's start method.
bool PipelineImpl::Start(FilterFactory* factory,
                         const std::string& url,
                         Callback1<bool>::Type* init_complete_callback) {
  DCHECK(!pipeline_thread_);
  DCHECK(factory);
  DCHECK(!initialized_);
  if (!pipeline_thread_ && factory) {
    pipeline_thread_ = new PipelineThread(this);
    if (pipeline_thread_) {
      // TODO(ralphl): Does the callback get copied by these fancy templates?
      // if so, then do I want to always delete it here???
      if (pipeline_thread_->Start(factory, url, init_complete_callback)) {
        return true;
      }
      pipeline_thread_ = NULL;  // Releases reference to destroy thread
    }
  }
  delete init_complete_callback;
  return false;
}

// Stop the PipelineThread and return to a state identical to that of a newly
// created PipelineImpl object.
void PipelineImpl::Stop() {
  if (pipeline_thread_) {
    pipeline_thread_->Stop();
  }
  ResetState();
}



void PipelineImpl::SetPlaybackRate(float rate) {
  if (OkToCallThread() && rate >= 0.0f) {
    pipeline_thread_->SetPlaybackRate(rate);
  } else {
    NOTREACHED();
  }
}

void PipelineImpl::Seek(base::TimeDelta time) {
  if (OkToCallThread()) {
    pipeline_thread_->Seek(time);
  } else {
    NOTREACHED();
  }
}

void PipelineImpl::SetVolume(float volume) {
  if (OkToCallThread() && volume >= 0.0f && volume <= 1.0f) {
    pipeline_thread_->SetVolume(volume);
  } else {
    NOTREACHED();
  }
}

void PipelineImpl::ResetState() {
  AutoLock auto_lock(lock_);
  pipeline_thread_  = NULL;
  initialized_      = false;
  duration_         = base::TimeDelta();
  buffered_time_    = base::TimeDelta();
  buffered_bytes_   = 0;
  total_bytes_      = 0;
  video_width_      = 0;
  video_height_     = 0;
  volume_           = 0.0f;
  playback_rate_    = 0.0f;
  error_            = PIPELINE_OK;
  time_             = base::TimeDelta();
  ticks_at_last_set_time_ = base::TimeTicks::Now();
}

void PipelineImpl::SetDuration(base::TimeDelta duration) {
  AutoLock auto_lock(lock_);
  duration_ = duration;
}

void PipelineImpl::SetBufferedTime(base::TimeDelta buffered_time) {
  AutoLock auto_lock(lock_);
  buffered_time_ = buffered_time;
}

void PipelineImpl::SetTotalBytes(int64 total_bytes) {
  AutoLock auto_lock(lock_);
  total_bytes_ = total_bytes;
}

void PipelineImpl::SetBufferedBytes(int64 buffered_bytes) {
  AutoLock auto_lock(lock_);
  buffered_bytes_ = buffered_bytes;
}

void PipelineImpl::SetVideoSize(size_t width, size_t height) {
  AutoLock auto_lock(lock_);
  video_width_ = width;
  video_height_ = height;
}

//-----------------------------------------------------------------------------

PipelineThread::PipelineThread(PipelineImpl* pipeline)
    : pipeline_(pipeline),
      thread_("PipelineThread"),
      time_update_callback_scheduled_(false),
      host_initializing_(NULL) {
}

PipelineThread::~PipelineThread() {
  Stop();
}

// This method is called on the client's thread.  It starts the pipeline's
// dedicated thread and posts a task to call the StartTask method on that
// thread.
bool PipelineThread::Start(FilterFactory* filter_factory,
                           const std::string& url,
                           Callback1<bool>::Type* init_complete_callback) {
  if (thread_.Start()) {
    filter_factory->AddRef();
    PostTask(NewRunnableMethod(this,
                               &PipelineThread::StartTask,
                               filter_factory,
                               url,
                               // TODO(ralphl): what happens to this callback?
                               // is it copied by NewRunnableTask?  Just pointer
                               // or is the callback itself copied?
                               init_complete_callback));
    return true;
  }
  return false;
}

// Called on the client's thread.  If the thread has been started, then posts
// a task to call the StopTask method, then waits until the thread has stopped.
// There is a critical section that wraps the entire duration of the StartTask
// method.  This method waits for that Lock to be released so that we know
// that the thread is not executing a nested message loop.  This way we know
// that that Thread::Stop call will quit the appropriate message loop.
void PipelineThread::Stop() {
  if (thread_.IsRunning()) {
    PostTask(NewRunnableMethod(this, &PipelineThread::StopTask));
    AutoLock lock_crit(initialization_lock_);
    thread_.Stop();
  }
  DCHECK(filter_hosts_.empty());
}

// Called on client's thread.
void PipelineThread::SetPlaybackRate(float rate) {
  PostTask(NewRunnableMethod(this, &PipelineThread::SetPlaybackRateTask, rate));
}

// Called on client's thread.
void PipelineThread::Seek(base::TimeDelta time) {
  PostTask(NewRunnableMethod(this, &PipelineThread::SeekTask, time));
}

// Called on client's thread.
void PipelineThread::SetVolume(float volume) {
  PostTask(NewRunnableMethod(this, &PipelineThread::SetVolumeTask, volume));
}

// May be called on any thread, and therefore we always assume the worst
// possible race condition.  This could, for example, be called from a filter's
// thread just as the pipeline thread is exiting the call to the filter's
// Initialize() method.  Therefore, we make NO assumptions, and post work
// in every case, even the trivial one of a thread calling this method from
// within it's Initialize method.  This means that we will always run a nested
// message loop, and the InitializationCompleteTask will Quit that loop
// immediately in the trivial case.
void PipelineThread::InitializationComplete(FilterHostImpl* host) {
  DCHECK(host == host_initializing_);
  PostTask(NewRunnableMethod(this,
                             &PipelineThread::InitializationCompleteTask,
                             host));
}

// Called from any thread.  Updates the pipeline time and schedules a task to
// call back to filters that have registered a callback for time updates.
void PipelineThread::SetTime(base::TimeDelta time) {
  pipeline()->SetTime(time);
  if (!time_update_callback_scheduled_) {
    time_update_callback_scheduled_ = true;
    PostTask(NewRunnableMethod(this, &PipelineThread::SetTimeTask));
  }
}

// Called from any thread.  Sets the pipeline error_ member and schedules a
// task to stop all the filters in the pipeline.  Note that the thread will
// continue to run until the client calls Pipeline::Stop, but nothing will
// be processed since filters will not be able to post tasks.
void PipelineThread::Error(PipelineError error) {
  // If this method returns false, then an error has already happened, so no
  // reason to run the StopTask again.  It's going to happen.
  if (pipeline()->InternalSetError(error)) {
    PostTask(NewRunnableMethod(this, &PipelineThread::StopTask));
  }
}

// Called from any thread.  Used by FilterHostImpl::PostTask method and used
// internally.
void PipelineThread::PostTask(Task* task) {
  message_loop()->PostTask(FROM_HERE, task);
}


// Main initialization method called on the pipeline thread.  This code attempts
// to use the specified filter factory to build a pipeline.  It starts by
// creating a DataSource, connects it to a Demuxer, and then connects the
// Demuxer's audio stream to an AudioDecoder which is then connected to an
// AudioRenderer.  If the media has video, then it connects a VideoDecoder to
// the Demuxer's video stream, and then connects the VideoDecoder to a
// VideoRenderer.  When all required filters have been created and have called
// their FilterHost's InitializationComplete method, the pipeline's
// initialized_ member is set to true, and, if the client provided an
// init_complete_callback, it is called with "true".
// If initializatoin fails, the client's callback will still be called, but
// the bool parameter passed to it will be false.
//
// Note that at each step in this process, the initialization of any filter
// may require running the pipeline thread's message loop recursively.  This is
// handled by the CreateFilter method.
void PipelineThread::StartTask(FilterFactory* filter_factory,
                               const std::string& url,
                               Callback1<bool>::Type* init_complete_callback) {
  bool success = true;

  // During the entire StartTask we hold the initialization_lock_ so that
  // if the client calls the Pipeline::Stop method while we are running a
  // nested message loop, we can correctly unwind out of it before calling
  // the Thread::Stop method.
  AutoLock auto_lock(initialization_lock_);

  // Add ourselves as a destruction observer of the thread's message loop so
  // we can delete filters at an appropriate time (when all tasks have been
  // processed and the thread is about to be destroyed).
  message_loop()->AddDestructionObserver(this);
  success = CreateDataSource(filter_factory, url);
  if (success) {
    success = CreateAndConnect<Demuxer, DataSource>(filter_factory);
  }
  if (success) {
    success = CreateDecoder<AudioDecoder>(filter_factory);
  }
  if (success) {
    success = CreateAndConnect<AudioRenderer, AudioDecoder>(filter_factory);
  }
  if (success && HasVideo()) {
    success = CreateDecoder<VideoDecoder>(filter_factory);
    if (success) {
      success = CreateAndConnect<VideoRenderer, VideoDecoder>(filter_factory);
    }
  }
  if (success) {
    pipeline_->initialized_ = true;
  } else {
    Error(PIPELINE_ERROR_INITIALIZATION_FAILED);
  }

  // No matter what, we're done with the filter factory, and
  // client callback so get rid of them.
  filter_factory->Release();
  if (init_complete_callback) {
    init_complete_callback->Run(success);
    delete init_complete_callback;
  }
}

// This method is called as a result of the client calling Pipeline::Stop() or
// as the result of an error condition.  If there is no error, then set the
// pipeline's error_ member to PIPELINE_STOPPING.  We stop the filters in the
// reverse order.
void PipelineThread::StopTask() {
  if (PIPELINE_OK == pipeline_->error_) {
    pipeline_->error_ = PIPELINE_STOPPING;
  }
  FilterHostVector::reverse_iterator riter = filter_hosts_.rbegin();
  while (riter != filter_hosts_.rend()) {
    (*riter)->Stop();
    ++riter;
  }
  if (host_initializing_) {
    host_initializing_ = NULL;
    message_loop()->Quit();
  }
}

// Task runs as a result of a filter calling InitializationComplete.  If for
// some reason StopTask has been executed prior to this, the host_initializing_
// member will be NULL, and the message loop will have been quit already, so
// we don't want to do it again.
void PipelineThread::InitializationCompleteTask(FilterHostImpl* host) {
  if (host == host_initializing_) {
    host_initializing_ = NULL;
    message_loop()->Quit();
  } else {
    DCHECK(!host_initializing_);
  }
}

void PipelineThread::SetPlaybackRateTask(float rate) {
  pipeline_->InternalSetPlaybackRate(rate);
  FilterHostVector::iterator iter = filter_hosts_.begin();
  while (iter != filter_hosts_.end()) {
    (*iter)->media_filter()->SetPlaybackRate(rate);
    ++iter;
  }
}

void PipelineThread::SeekTask(base::TimeDelta time) {
  FilterHostVector::iterator iter = filter_hosts_.begin();
  while (iter != filter_hosts_.end()) {
    (*iter)->media_filter()->Seek(time);
    ++iter;
  }
}

void PipelineThread::SetVolumeTask(float volume) {
  pipeline_->volume_ = volume;
  AudioRenderer* audio_renderer = GetFilter<AudioRenderer>();
  if (audio_renderer) {
    audio_renderer->SetVolume(volume);
  }
}

void PipelineThread::SetTimeTask() {
  time_update_callback_scheduled_ = false;
  FilterHostVector::iterator iter = filter_hosts_.begin();
  while (iter != filter_hosts_.end()) {
    (*iter)->RunTimeUpdateCallback(pipeline_->time_);
    ++iter;
  }
}

template <class Filter>
Filter* PipelineThread::GetFilter() const {
  Filter* filter = NULL;
  FilterHostVector::const_iterator iter = filter_hosts_.begin();
  while (iter != filter_hosts_.end() && NULL == filter) {
    filter = (*iter)->GetFilter<Filter>();
    ++iter;
  }
  return filter;
}

template <class Filter, class Source>
bool PipelineThread::CreateFilter(FilterFactory* filter_factory,
                                  Source source,
                                  const MediaFormat* media_format) {
  scoped_refptr<Filter> filter = filter_factory->Create<Filter>(media_format);
  bool success = (NULL != filter);
  if (success) {
    DCHECK(!host_initializing_);
    host_initializing_ = new FilterHostImpl(this, filter.get());
    success = (NULL != host_initializing_);
  }
  if (success) {
    filter_hosts_.push_back(host_initializing_);
    filter->SetFilterHost(host_initializing_);

    // The filter must return true from initialize and there must still not
    // be an error or it's not successful.
    success = (filter->Initialize(source) &&
               PIPELINE_OK == pipeline_->error_);
  }
  if (success) {
    // Now we run the thread's message loop recursively.  We want all
    // pending tasks to be processed, so we set nestable tasks to be allowed
    // and then run the loop.  The only way we exit the loop is as the result
    // of a call to FilterHost::InitializationComplete, FilterHost::Error, or
    // Pipeline::Stop.  In each of these cases, the corresponding task method
    // sets host_initializing_ to NULL to signal that the message loop's Quit
    // method has already been called, and then calls message_loop()->Quit().
    // The setting of |host_initializing_| to NULL in the task prevents a
    // subsequent task from accidentally quitting the wrong (non-nested) loop.
    message_loop()->SetNestableTasksAllowed(true);
    message_loop()->Run();
    message_loop()->SetNestableTasksAllowed(false);
    DCHECK(!host_initializing_);

    // If an error occurred while we were in the nested Run state, then
    // not successful.  When stopping, the |error_| member is set to a value of
    // PIPELINE_STOPPING so we will exit in that case also with false.
    success = (PIPELINE_OK == pipeline_->error_);
  }

  // This could still be set if we never ran the message loop (for example,
  // if the fiter returned false from it's Initialize method), so make sure
  // to reset it.
  host_initializing_ = NULL;

  // If this method fails, but no error set, then indicate a general
  // initialization failure.
  if (!success) {
    Error(PIPELINE_ERROR_INITIALIZATION_FAILED);
  }
  return success;
}

bool PipelineThread::CreateDataSource(FilterFactory* filter_factory,
                                      const std::string& url) {
  MediaFormat url_format;
  url_format.SetAsString(MediaFormat::kMimeType, mime_type::kURL);
  url_format.SetAsString(MediaFormat::kURL, url);
  return CreateFilter<DataSource>(filter_factory, url, &url_format);
}

template <class Decoder>
bool PipelineThread::CreateDecoder(FilterFactory* filter_factory) {
  Demuxer* demuxer = GetFilter<Demuxer>();
  if (demuxer) {
    int num_outputs = demuxer->GetNumberOfStreams();
    for (int i = 0; i < num_outputs; ++i) {
      DemuxerStream* stream = demuxer->GetStream(i);
      const MediaFormat* stream_format = stream->GetMediaFormat();
      if (IsMajorMimeType(stream_format, Decoder::major_mime_type())) {
        return CreateFilter<Decoder>(filter_factory, stream, stream_format);
      }
    }
  }
  return false;
}

template <class NewFilter, class SourceFilter>
bool PipelineThread::CreateAndConnect(FilterFactory* filter_factory) {
  SourceFilter* source_filter = GetFilter<SourceFilter>();
  bool success = (source_filter &&
                  CreateFilter<NewFilter>(filter_factory,
                                          source_filter,
                                          source_filter->GetMediaFormat()));
  return success;
}

// TODO(ralphl): Consider making this part of the demuxer interface.
bool PipelineThread::HasVideo() const {
  Demuxer* demuxer = GetFilter<Demuxer>();
  if (demuxer) {
    int num_outputs = demuxer->GetNumberOfStreams();
    for (int i = 0; i < num_outputs; ++i) {
      if (IsMajorMimeType(demuxer->GetStream(i)->GetMediaFormat(),
                          mime_type::kMajorTypeVideo)) {
        return true;
      }
    }
  }
  return false;
}

bool PipelineThread::IsMajorMimeType(const MediaFormat* media_format,
                                     const std::string& major_mime_type) const {
  std::string value;
  if (media_format->GetAsString(MediaFormat::kMimeType, &value)) {
    return (0 == value.compare(0, major_mime_type.length(), major_mime_type));
  }
  return false;
}

// Called as a result of destruction of the thread.
void PipelineThread::WillDestroyCurrentMessageLoop() {
  while (!filter_hosts_.empty()) {
    delete filter_hosts_.back();
    filter_hosts_.pop_back();
  }
}

}  // namespace media
