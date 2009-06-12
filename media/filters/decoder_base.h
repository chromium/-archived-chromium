// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// A base class that provides the plumbing for a decoder filters.

#ifndef MEDIA_FILTERS_DECODER_BASE_H_
#define MEDIA_FILTERS_DECODER_BASE_H_

#include <deque>

#include "base/lock.h"
#include "base/task.h"
#include "base/thread.h"
#include "media/base/buffers.h"
#include "media/base/filters.h"
#include "media/base/filter_host.h"

namespace media {

template <class Decoder, class Output>
class DecoderBase : public Decoder {
 public:
  typedef CallbackRunner< Tuple1<Output*> > ReadCallback;

  // MediaFilter implementation.
  virtual void Stop() {
    OnStop();
    {
      AutoLock auto_lock(lock_);
      running_ = false;
      if (process_task_) {
        process_task_->Cancel();
        process_task_ = NULL;
      }
      DiscardQueues_Locked();
    }

    // Stop our decoding thread.
    thread_.Stop();
  }

  virtual void Seek(base::TimeDelta time) {
    // Delegate to the subclass first.
    OnSeek(time);

    // Flush the result queue.
    AutoLock auto_lock(lock_);
    result_queue_.clear();

    // Flush the input queue. This will trigger more reads from the demuxer.
    input_queue_.clear();

    // Turn on the seeking flag so that we can discard buffers until a
    // discontinuous buffer is received.
    seeking_ = true;

    // Trigger more reads and keep the process loop rolling.
    ScheduleProcessTask_Locked();
  }

  // Decoder implementation.
  virtual bool Initialize(DemuxerStream* demuxer_stream) {
    demuxer_stream_ = demuxer_stream;

    // Start our internal decoding thread.
    if (!thread_.Start()) {
      host()->Error(PIPELINE_ERROR_DECODE);
      return false;
    }

    if (!OnInitialize(demuxer_stream)) {
      // Release our resources and stop our thread.
      // TODO(scherkus): shouldn't stop a thread inside Initialize(), but until I
      // figure out proper error signaling semantics we're going to do it anyway!!
      host()->Error(PIPELINE_ERROR_DECODE);
      demuxer_stream_ = NULL;
      thread_.Stop();
      return false;
    }

    DCHECK(!media_format_.empty());
    host()->InitializationComplete();
    return true;
  }

  virtual const MediaFormat& media_format() { return media_format_; }

  // Audio or Video decoder.
  virtual void Read(ReadCallback* read_callback) {
    AutoLock auto_lock(lock_);
    if (IsRunning()) {
      read_queue_.push_back(read_callback);
      ScheduleProcessTask_Locked();
    } else {
      delete read_callback;
    }
  }

  void OnReadComplete(Buffer* buffer) {
    AutoLock auto_lock(lock_);
    if (IsRunning()) {
      // Once the |seeking_| flag is set we ignore every buffers here
      // until we receive a discontinuous buffer and we will turn off the
      // |seeking_| flag.
      if (buffer->IsDiscontinuous()) {
        // TODO(hclam): put a DCHECK here to assert |seeking_| being true.
        // I cannot do this now because seek operation is not fully
        // asynchronous. There may be pending seek requests even before the
        // previous was finished.
        seeking_ = false;
      }
      if (!seeking_)
        input_queue_.push_back(buffer);
      --pending_reads_;
      ScheduleProcessTask_Locked();
    }
  }

 protected:
  // |thread_name| is mandatory and is used to identify the thread in debuggers.
  explicit DecoderBase(const char* thread_name)
      : running_(true),
        demuxer_stream_(NULL),
        thread_(thread_name),
        pending_reads_(0),
        process_task_(NULL),
        seeking_(false) {
  }

  virtual ~DecoderBase() {
    DCHECK(!thread_.IsRunning());
    DCHECK(!process_task_);
  }

  // This method is called by the derived class from within the OnDecode method.
  // It places an output buffer in the result queue.  It must be called from
  // within the OnDecode method.
  void EnqueueResult(Output* output) {
    AutoLock auto_lock(lock_);
    if (IsRunning()) {
      result_queue_.push_back(output);
    }
  }

  // Method that must be implemented by the derived class.  Called from within
  // the DecoderBase::Initialize() method before any reads are submitted to
  // the demuxer stream.  Returns true if successful, otherwise false indicates
  // a fatal error.  The derived class should NOT call the filter host's
  // InitializationComplete() method.  If this method returns true, then the
  // base class will call the host to complete initialization.  During this
  // call, the derived class must fill in the media_format_ member.
  virtual bool OnInitialize(DemuxerStream* demuxer_stream) = 0;

  // Method that may be implemented by the derived class if desired.  It will
  // be called from within the MediaFilter::Stop() method prior to stopping the
  // base class.
  virtual void OnStop() {}

  // Derived class can implement this method and perform seeking logic prior
  // to the base class.
  virtual void OnSeek(base::TimeDelta time) {}

  // Method that must be implemented by the derived class.  If the decode
  // operation produces one or more outputs, the derived class should call
  // the EnequeueResult() method from within this method.
  virtual void OnDecode(Buffer* input) = 0;

  bool IsRunning() const { return running_; }

  MediaFormat media_format_;

 private:
  // The GCL compiler does not like .cc files that directly access members of
  // a base class.  This inline method helps.
  FilterHost* host() const { return Decoder::host_; }

  // Schedules a task that will execute the ProcessTask method.
  void ScheduleProcessTask_Locked() {
    lock_.AssertAcquired();
    DCHECK(IsRunning());
    if (!process_task_) {
      process_task_ = NewRunnableMethod(this, &DecoderBase::ProcessTask);
      thread_.message_loop()->PostTask(FROM_HERE, process_task_);
    }
  }

  // The core work loop of the decoder base.  This method will run the methods
  // SubmitReads_Locked(), ProcessInput_Locked(), and ProcessOutput_Locked() in
  // a loop until they either produce no further work, or the filter is stopped.
  // Once there is no further work to do, the method returns.  A later call to
  // the ScheduleProcessTask_Locked() method will start this task again.
  void ProcessTask() {
    AutoLock auto_lock(lock_);
    bool did_some_work;
    do {
      did_some_work  = SubmitReads_Locked();
      did_some_work |= ProcessInput_Locked();
      did_some_work |= ProcessOutput_Locked();
    } while (IsRunning() && did_some_work);
    DCHECK(process_task_ || !IsRunning());
    process_task_ = NULL;
  }

  // If necessary, calls the |demuxer_stream_| to read buffers.  Returns true
  // if reads have happened, else false.  This method must be called with
  // |lock_| acquired.  If the method submits any reads, then it will Release()
  // the |lock_| when calling the demuxer and then re-Acquire() the |lock_|.
  bool SubmitReads_Locked() {
    lock_.AssertAcquired();
    bool did_read = false;
    if (IsRunning() &&
        pending_reads_ + input_queue_.size() < read_queue_.size()) {
      did_read = true;
      size_t read = read_queue_.size() - pending_reads_ - input_queue_.size();
      pending_reads_ += read;
      {
        AutoUnlock unlock(lock_);
        while (read) {
          demuxer_stream_->
              Read(NewCallback(this, &DecoderBase::OnReadComplete));
          --read;
        }
      }
    }
    return did_read;
  }

  // If the |input_queue_| has any buffers, this method will call the derived
  // class's OnDecode() method.
  bool ProcessInput_Locked() {
    lock_.AssertAcquired();
    bool did_decode = false;
    while (IsRunning() && !input_queue_.empty()) {
      did_decode = true;
      scoped_refptr<Buffer> input = input_queue_.front();
      input_queue_.pop_front();
      // Release |lock_| before calling the derived class to do the decode.
      {
        AutoUnlock unlock(lock_);
        OnDecode(input);
      }
    }
    return did_decode;
  }

  // Removes any buffers from the |result_queue_| and calls the next callback
  // in the |read_queue_|.
  bool ProcessOutput_Locked() {
    lock_.AssertAcquired();
    bool called_renderer = false;
    while (IsRunning() && !read_queue_.empty() && !result_queue_.empty()) {
      called_renderer = true;
      scoped_refptr<Output> output = result_queue_.front();
      result_queue_.pop_front();
      scoped_ptr<ReadCallback> read_callback(read_queue_.front());
      read_queue_.pop_front();
      // Release |lock_| before calling the renderer.
      {
        AutoUnlock unlock(lock_);
        read_callback->Run(output);
      }
    }
    return called_renderer;
  }

  // Throw away all buffers in all queues.
  void DiscardQueues_Locked() {
    lock_.AssertAcquired();
    input_queue_.clear();
    result_queue_.clear();
    while (!read_queue_.empty()) {
      delete read_queue_.front();
      read_queue_.pop_front();
    }
  }

  // The critical section for the decoder.
  Lock lock_;

  // If false, then the Stop() method has been called, and no further processing
  // of buffers should occur.
  bool running_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  // The dedicated decoding thread for this filter.
  base::Thread thread_;

  // Number of times we have called Read() on the demuxer that have not yet
  // been satisfied.
  size_t pending_reads_;

  CancelableTask* process_task_;

  // Queue of buffers read from the |demuxer_stream_|.
  typedef std::deque< scoped_refptr<Buffer> > InputQueue;
  InputQueue input_queue_;

  // Queue of decoded samples produced in the OnDecode() method of the decoder.
  // Any samples placed in this queue will be assigned to the OutputQueue
  // buffers once the OnDecode() method returns.
  // TODO(ralphl): Eventually we want to have decoders get their destination
  // buffer from the OutputQueue and write to it directly.  Until we change
  // from the Assignable buffer to callbacks and renderer-allocated buffers,
  // we need this extra queue.
  typedef std::deque< scoped_refptr<Output> > ResultQueue;
  ResultQueue result_queue_;

  // Queue of callbacks supplied by the renderer through the Read() method.
  typedef std::deque<ReadCallback*> ReadQueue;
  ReadQueue read_queue_;

  // An internal state of the decoder that indicates that are waiting for seek
  // to complete. We expect to receive a discontinuous frame/packet from the
  // demuxer to signal that seeking is completed.
  bool seeking_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_BASE_H_
