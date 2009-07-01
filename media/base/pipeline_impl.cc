// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(scherkus): clean up PipelineImpl... too many crazy function names,
// potential deadlocks, etc...

#include "base/compiler_specific.h"
#include "base/condition_variable.h"
#include "base/stl_util-inl.h"
#include "media/base/filter_host_impl.h"
#include "media/base/media_format.h"
#include "media/base/pipeline_impl.h"

namespace media {

namespace {

// Small helper function to help us transition over to injected message loops.
//
// TODO(scherkus): have every filter support injected message loops.
template <class Filter>
bool SupportsSetMessageLoop() {
  switch (Filter::filter_type()) {
    case FILTER_DEMUXER:
    case FILTER_AUDIO_DECODER:
    case FILTER_VIDEO_DECODER:
      return true;
    default:
      return false;
  }
}

// Helper function used with NewRunnableMethod to implement a (very) crude
// blocking counter.
//
// TODO(scherkus): remove this as soon as Stop() is made asynchronous.
void DecrementCounter(Lock* lock, ConditionVariable* cond_var, int* count) {
  AutoLock auto_lock(*lock);
  --(*count);
  CHECK(*count >= 0);
  if (*count == 0) {
    cond_var->Signal();
  }
}

}  // namespace

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

bool PipelineImpl::IsRendered(const std::string& major_mime_type) const {
  AutoLock auto_lock(const_cast<Lock&>(lock_));
  bool is_rendered = (rendered_mime_types_.find(major_mime_type) !=
                      rendered_mime_types_.end());
  return is_rendered;
}


bool PipelineImpl::InternalSetError(PipelineError error) {
  // Don't want callers to set an error of "OK".  STOPPING is a special value
  // that should only be used internally by the StopTask() method.
  DCHECK(PIPELINE_OK != error && PIPELINE_STOPPING != error);
  AutoLock auto_lock(lock_);
  bool changed_error = false;
  if (PIPELINE_OK == error_) {
    error_ = error;
    changed_error = true;
  }
  return changed_error;
}

// Creates the PipelineThread and calls it's start method.
bool PipelineImpl::Start(FilterFactory* factory,
                         const std::string& url,
                         PipelineCallback* init_complete_callback) {
  DCHECK(!pipeline_thread_);
  DCHECK(factory);
  DCHECK(!initialized_);
  DCHECK(!IsPipelineThread());
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
  DCHECK(!IsPipelineThread());

  if (pipeline_thread_) {
    pipeline_thread_->Stop();
  }
  ResetState();
}

void PipelineImpl::SetPlaybackRate(float rate) {
  DCHECK(!IsPipelineThread());

  if (IsPipelineOk() && rate >= 0.0f) {
    pipeline_thread_->SetPlaybackRate(rate);
  } else {
    // It's OK for a client to call SetPlaybackRate(0.0f) if we're stopped.
    DCHECK(rate == 0.0f && playback_rate_ == 0.0f);
  }
}

void PipelineImpl::Seek(base::TimeDelta time,
                        PipelineCallback* seek_callback) {
  DCHECK(!IsPipelineThread());

  if (IsPipelineOk()) {
    pipeline_thread_->Seek(time, seek_callback);
  } else {
    NOTREACHED();
  }
}

void PipelineImpl::SetVolume(float volume) {
  DCHECK(!IsPipelineThread());

  if (IsPipelineOk() && volume >= 0.0f && volume <= 1.0f) {
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
  rendered_mime_types_.clear();
}

bool PipelineImpl::IsPipelineOk() const {
  return pipeline_thread_ && initialized_ && PIPELINE_OK == error_;
}

bool PipelineImpl::IsPipelineThread() const {
  return pipeline_thread_ &&
      PlatformThread::CurrentId() == pipeline_thread_->thread_id();
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

void PipelineImpl::InsertRenderedMimeType(const std::string& major_mime_type) {
  AutoLock auto_lock(lock_);
  rendered_mime_types_.insert(major_mime_type);
}


//-----------------------------------------------------------------------------

PipelineThread::PipelineThread(PipelineImpl* pipeline)
    : pipeline_(pipeline),
      thread_("PipelineThread"),
      time_update_callback_scheduled_(false),
      state_(kCreated) {
}

PipelineThread::~PipelineThread() {
  Stop();
  DCHECK(state_ == kStopped || state_ == kError);
}

// This method is called on the client's thread.  It starts the pipeline's
// dedicated thread and posts a task to call the StartTask() method on that
// thread.
bool PipelineThread::Start(FilterFactory* filter_factory,
                           const std::string& url,
                           PipelineCallback* init_complete_callback) {
  DCHECK_EQ(kCreated, state_);
  if (thread_.Start()) {
    filter_factory_ = filter_factory;
    url_ = url;
    init_callback_.reset(init_complete_callback);
    PostTask(NewRunnableMethod(this, &PipelineThread::StartTask));
    return true;
  }
  return false;
}

// Called on the client's thread.  If the thread has been started, then posts
// a task to call the StopTask() method, then waits until the thread has
// stopped.
void PipelineThread::Stop() {
  if (thread_.IsRunning()) {
    PostTask(NewRunnableMethod(this, &PipelineThread::StopTask));
    thread_.Stop();
  }
  DCHECK(filter_hosts_.empty());
  DCHECK(filter_threads_.empty());
}

// Called on client's thread.
void PipelineThread::SetPlaybackRate(float rate) {
  PostTask(NewRunnableMethod(this, &PipelineThread::SetPlaybackRateTask, rate));
}

// Called on client's thread.
void PipelineThread::Seek(base::TimeDelta time,
                          PipelineCallback* seek_callback) {
  PostTask(NewRunnableMethod(this, &PipelineThread::SeekTask, time,
                             seek_callback));
}

// Called on client's thread.
void PipelineThread::SetVolume(float volume) {
  PostTask(NewRunnableMethod(this, &PipelineThread::SetVolumeTask, volume));
}

void PipelineThread::InitializationComplete(FilterHostImpl* host) {
  if (IsPipelineOk()) {
    // Continue the start task by proceeding to the next stage.
    PostTask(NewRunnableMethod(this, &PipelineThread::StartTask));
  }
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

// Called from any thread.  Sets the pipeline |error_| member and schedules a
// task to stop all the filters in the pipeline.  Note that the thread will
// continue to run until the client calls Pipeline::Stop(), but nothing will
// be processed since filters will not be able to post tasks.
void PipelineThread::Error(PipelineError error) {
  // If this method returns false, then an error has already happened, so no
  // reason to run the StopTask again.  It's going to happen.
  if (pipeline()->InternalSetError(error)) {
    PostTask(NewRunnableMethod(this, &PipelineThread::StopTask));
  }
}

// This is a helper method to post task on message_loop(). This method is only
// called from this class or from Pipeline.
void PipelineThread::PostTask(Task* task) {
  message_loop()->PostTask(FROM_HERE, task);
}


// Main initialization method called on the pipeline thread.  This code attempts
// to use the specified filter factory to build a pipeline.
// Initialization step performed in this method depends on current state of this
// object, indicated by |state_|.  After each step of initialization, this
// object transits to the next stage.  It starts by creating a DataSource,
// connects it to a Demuxer, and then connects the Demuxer's audio stream to an
// AudioDecoder which is then connected to an AudioRenderer.  If the media has
// video, then it connects a VideoDecoder to the Demuxer's video stream, and
// then connects the VideoDecoder to a VideoRenderer.
//
// When all required filters have been created and have called their
// FilterHost's InitializationComplete method, the pipeline's |initialized_|
// member is set to true, and, if the client provided an
// |init_complete_callback_|, it is called with "true".
//
// If initialization fails, the client's callback will still be called, but
// the bool parameter passed to it will be false.
//
// TODO(hclam): StartTask is now starting the pipeline asynchronously. It
// works like a big state change table. If we no longer need to start filters
// in order, we need to get rid of all the state change.
void PipelineThread::StartTask() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());

  // If we have received the stop signal, return immediately.
  if (state_ == kStopped)
    return;

  DCHECK(state_ == kCreated || IsPipelineInitializing());

  // Just created, create data source.
  if (state_ == kCreated) {
    message_loop()->AddDestructionObserver(this);
    state_ = kInitDataSource;
    CreateDataSource();
    return;
  }

  // Data source created, create demuxer.
  if (state_ == kInitDataSource) {
    state_ = kInitDemuxer;
    CreateDemuxer();
    return;
  }

  // Demuxer created, create audio decoder.
  if (state_ == kInitDemuxer) {
    state_ = kInitAudioDecoder;
    // If this method returns false, then there's no audio stream.
    if (CreateDecoder<AudioDecoder>())
      return;
  }

  // Assuming audio decoder was created, create audio renderer.
  if (state_ == kInitAudioDecoder) {
    state_ = kInitAudioRenderer;
    // Returns false if there's no audio stream.
    if (CreateRenderer<AudioDecoder, AudioRenderer>()) {
      pipeline_->InsertRenderedMimeType(AudioDecoder::major_mime_type());
      return;
    }
  }

  // Assuming audio renderer was created, create video decoder.
  if (state_ == kInitAudioRenderer) {
    // Then perform the stage of initialization, i.e. initialize video decoder.
    state_ = kInitVideoDecoder;
    if (CreateDecoder<VideoDecoder>())
      return;
  }

  // Assuming video decoder was created, create video renderer.
  if (state_ == kInitVideoDecoder) {
    state_ = kInitVideoRenderer;
    if (CreateRenderer<VideoDecoder, VideoRenderer>()) {
      pipeline_->InsertRenderedMimeType(VideoDecoder::major_mime_type());
      return;
    }
  }

  if (state_ == kInitVideoRenderer) {
    if (!IsPipelineOk() || pipeline_->rendered_mime_types_.empty()) {
      Error(PIPELINE_ERROR_COULD_NOT_RENDER);
      return;
    }

    state_ = kStarted;
    pipeline_->initialized_ = true;
    filter_factory_ = NULL;
    if (init_callback_.get()) {
      init_callback_->Run(true);
      init_callback_.reset();
    }
  }
}

// This method is called as a result of the client calling Pipeline::Stop() or
// as the result of an error condition.  If there is no error, then set the
// pipeline's error_ member to PIPELINE_STOPPING.  We stop the filters in the
// reverse order.
//
// TODO(scherkus): beware!  this can get posted multiple times since we post
// Stop() tasks even if we've already stopped.  Perhaps this should no-op for
// additional calls, however most of this logic will be changing.
void PipelineThread::StopTask() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());

  if (IsPipelineInitializing()) {
    // If IsPipelineOk() is true, the pipeline was simply stopped during
    // initialization. Otherwise it is a failure.
    state_ = IsPipelineOk() ? kStopped : kError;
    filter_factory_ = NULL;
    if (init_callback_.get()) {
      init_callback_->Run(false);
      init_callback_.reset();
    }
  } else {
    state_ = kStopped;
  }

  if (IsPipelineOk()) {
    pipeline_->error_ = PIPELINE_STOPPING;
  }

  // Stop every filter.
  for (FilterHostVector::iterator iter = filter_hosts_.begin();
       iter != filter_hosts_.end();
       ++iter) {
    (*iter)->Stop();
  }

  // Figure out how many threads we have to stop.
  //
  // TODO(scherkus): remove the workaround for the "multiple StopTask()" issue.
  FilterThreadVector running_threads;
  for (FilterThreadVector::iterator iter = filter_threads_.begin();
    iter != filter_threads_.end();
    ++iter) {
    if ((*iter)->IsRunning()) {
      running_threads.push_back(*iter);
    }
  }

  // Crude blocking counter implementation.
  Lock lock;
  ConditionVariable wait_for_zero(&lock);
  int count = running_threads.size();

  // Post a task to every filter's thread to ensure that they've completed their
  // stopping logic before stopping the threads themselves.
  //
  // TODO(scherkus): again, Stop() should either be synchronous or we should
  // receive a signal from filters that they have indeed stopped.
  for (FilterThreadVector::iterator iter = running_threads.begin();
       iter != running_threads.end();
       ++iter) {
    (*iter)->message_loop()->PostTask(FROM_HERE,
        NewRunnableFunction(&DecrementCounter, &lock, &wait_for_zero, &count));
  }

  // Wait on our "blocking counter".
  {
    AutoLock auto_lock(lock);
    while (count > 0) {
      wait_for_zero.Wait();
    }
  }

  // Stop every running filter thread.
  //
  // TODO(scherkus): can we watchdog this section to detect wedged threads?
  for (FilterThreadVector::iterator iter = running_threads.begin();
       iter != running_threads.end();
       ++iter) {
    (*iter)->Stop();
  }
}

void PipelineThread::SetPlaybackRateTask(float rate) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());

  pipeline_->InternalSetPlaybackRate(rate);
  for (FilterHostVector::iterator iter = filter_hosts_.begin();
       iter != filter_hosts_.end();
       ++iter) {
    (*iter)->media_filter()->SetPlaybackRate(rate);
  }
}

void PipelineThread::SeekTask(base::TimeDelta time,
                              PipelineCallback* seek_callback) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());

  for (FilterHostVector::iterator iter = filter_hosts_.begin();
       iter != filter_hosts_.end();
       ++iter) {
    (*iter)->media_filter()->Seek(time);
  }

  // TODO(hclam): we should set the time when the above seek operations were all
  // successful and first frame/packet at the desired time is decoded. I'm
  // setting the time here because once we do the callback the user can ask for
  // current time immediately, which is the old time. In order to get rid this
  // little glitch, we either assume the seek was successful and time is updated
  // immediately here or we set time and do callback when we have new
  // frames/packets.
  SetTime(time);
  if (seek_callback) {
    seek_callback->Run(true);
    delete seek_callback;
  }
}

void PipelineThread::SetVolumeTask(float volume) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());

  pipeline_->volume_ = volume;
  scoped_refptr<AudioRenderer> audio_renderer;
  GetFilter(&audio_renderer);
  if (audio_renderer) {
    audio_renderer->SetVolume(volume);
  }
}

void PipelineThread::SetTimeTask() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());

  time_update_callback_scheduled_ = false;
  for (FilterHostVector::iterator iter = filter_hosts_.begin();
       iter != filter_hosts_.end();
       ++iter) {
    (*iter)->RunTimeUpdateCallback(pipeline_->time_);
  }
}

template <class Filter>
void PipelineThread::GetFilter(scoped_refptr<Filter>* filter_out) const {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());

  *filter_out = NULL;
  for (FilterHostVector::const_iterator iter = filter_hosts_.begin();
       iter != filter_hosts_.end() && NULL == *filter_out;
       iter++) {
    (*iter)->GetFilter(filter_out);
  }
}

template <class Filter, class Source>
void PipelineThread::CreateFilter(FilterFactory* filter_factory,
                                  Source source,
                                  const MediaFormat& media_format) {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());
  DCHECK(IsPipelineOk());

  scoped_refptr<Filter> filter = filter_factory->Create<Filter>(media_format);
  if (!filter) {
    Error(PIPELINE_ERROR_REQUIRED_FILTER_MISSING);
  } else {
    scoped_ptr<FilterHostImpl> host(new FilterHostImpl(this, filter.get()));
    // Create a dedicated thread for this filter.
    if (SupportsSetMessageLoop<Filter>()) {
      // TODO(scherkus): figure out a way to name these threads so it matches
      // the filter type.
      scoped_ptr<base::Thread> thread(new base::Thread("FilterThread"));
      if (!thread.get() || !thread->Start()) {
        NOTREACHED() << "Could not start filter thread";
        Error(PIPELINE_ERROR_INITIALIZATION_FAILED);
      } else {
        filter->SetMessageLoop(thread->message_loop());
        filter_threads_.push_back(thread.release());
      }
    }

    // Creating a thread could have failed, verify we're still OK.
    if (IsPipelineOk()) {
      filter_hosts_.push_back(host.get());
      filter->SetFilterHost(host.release());
      if (!filter->Initialize(source)) {
        Error(PIPELINE_ERROR_INITIALIZATION_FAILED);
      }
    }
  }
}

void PipelineThread::CreateDataSource() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());
  DCHECK(IsPipelineOk());

  MediaFormat url_format;
  url_format.SetAsString(MediaFormat::kMimeType, mime_type::kURL);
  url_format.SetAsString(MediaFormat::kURL, url_);
  CreateFilter<DataSource>(filter_factory_, url_, url_format);
}

void PipelineThread::CreateDemuxer() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());
  DCHECK(IsPipelineOk());

  scoped_refptr<DataSource> data_source;
  GetFilter(&data_source);
  DCHECK(data_source);
  CreateFilter<Demuxer, DataSource>(filter_factory_, data_source);
}

template <class Decoder>
bool PipelineThread::CreateDecoder() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());
  DCHECK(IsPipelineOk());

  scoped_refptr<Demuxer> demuxer;
  GetFilter(&demuxer);
  DCHECK(demuxer);

  const std::string major_mime_type = Decoder::major_mime_type();
  const int num_outputs = demuxer->GetNumberOfStreams();
  for (int i = 0; i < num_outputs; ++i) {
    scoped_refptr<DemuxerStream> stream = demuxer->GetStream(i);
    std::string value;
    if (stream->media_format().GetAsString(MediaFormat::kMimeType, &value) &&
        0 == value.compare(0, major_mime_type.length(), major_mime_type)) {
      CreateFilter<Decoder, DemuxerStream>(filter_factory_, stream);
      return true;
    }
  }
  return false;
}

template <class Decoder, class Renderer>
bool PipelineThread::CreateRenderer() {
  DCHECK_EQ(PlatformThread::CurrentId(), thread_.thread_id());
  DCHECK(IsPipelineOk());

  scoped_refptr<Decoder> decoder;
  GetFilter(&decoder);

  if (decoder) {
    // If the decoder was created.
    const std::string major_mime_type = Decoder::major_mime_type();
    CreateFilter<Renderer, Decoder>(filter_factory_, decoder);
    return true;
  }
  return false;
}

// Called as a result of destruction of the thread.
//
// TODO(scherkus): this can block the client due to synchronous Stop() API call.
void PipelineThread::WillDestroyCurrentMessageLoop() {
  STLDeleteElements(&filter_hosts_);
  STLDeleteElements(&filter_threads_);
}

}  // namespace media
