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
      DiscardQueues();
    }
    // Because decode_thread_ is a scoped_ptr this will destroy the thread,
    // if there was one, which causes it to be shut down in an orderly way.
    decode_thread_.reset();
  }

  // Decoder implementation.
  virtual bool Initialize(DemuxerStream* demuxer_stream) {
    demuxer_stream_ = demuxer_stream;
    if (decode_thread_.get()) {
      if (!decode_thread_->Start()) {
        NOTREACHED();
        return false;
      }
    }
    if (OnInitialize(demuxer_stream)) {
      DCHECK(!media_format_.empty());
      host()->InitializationComplete();
      return true;
    } else {
      demuxer_stream_ = NULL;
      decode_thread_.reset();
      return false;
    }
  }

  virtual const MediaFormat& media_format() { return media_format_; }

  // Audio or Video decoder.
  virtual void Read(Assignable<Output>* output) {
    AutoLock auto_lock(lock_);
    if (IsRunning()) {
      output->AddRef();
      output_queue_.push_back(output);
      ScheduleProcessTask();
    }
  }

  void OnReadComplete(Buffer* buffer) {
    AutoLock auto_lock(lock_);
    if (IsRunning()) {
      buffer->AddRef();
      input_queue_.push_back(buffer);
      --pending_reads_;
      ScheduleProcessTask();
    }
  }

 protected:
  // If NULL is passed for the |thread_name| then all processing of decodes
  // will happen on the pipeline thread.  If the name is non-NULL then a new
  // thread will be created for this decoder, and it will be assigned the
  // name provided by |thread_name|.
  explicit DecoderBase(const char* thread_name)
      : running_(true),
        demuxer_stream_(NULL),
        decode_thread_(thread_name ? new base::Thread(thread_name) : NULL),
        pending_reads_(0),
        process_task_(NULL) {
  }

  virtual ~DecoderBase() {
    Stop();
  }

  // This method is called by the derived class from within the OnDecode method.
  // It places an output buffer in the result queue.  It must be called from
  // within the OnDecode method.
  void EnqueueResult(Output* output) {
    AutoLock auto_lock(lock_);
    if (IsRunning()) {
      output->AddRef();
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
  // be called from within the MediaFilter::Stop method prior to stopping the
  // base class.
  virtual void OnStop() {}

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
  void ScheduleProcessTask() {
    DCHECK(IsRunning());
    if (!process_task_) {
      process_task_ = NewRunnableMethod(this, &DecoderBase::ProcessTask);
      if (decode_thread_.get()) {
        decode_thread_->message_loop()->PostTask(FROM_HERE, process_task_);
      } else {
        host()->PostTask(process_task_);
      }
    }
  }

  // The core work loop of the decoder base.  This method will run the methods
  // SubmitReads(), ProcessInput(), and ProcessOutput() in a loop until they
  // either produce no further work, or the filter is stopped.  Once there is
  // no further work to do, the method returns.  A later call to the
  // ScheduleProcessTask() method will start this task again.
  void ProcessTask() {
    AutoLock auto_lock(lock_);
    bool did_some_work;
    do {
      did_some_work  = SubmitReads();
      did_some_work |= ProcessInput();
      did_some_work |= ProcessOutput();
    } while (IsRunning() && did_some_work);
    DCHECK(process_task_ || !IsRunning());
    process_task_ = NULL;
  }

  // If necessary, calls the |demuxer_stream_| to read buffers.  Returns true
  // if reads have happened, else false.  This method must be called with
  // |lock_| acquired.  If the method submits any reads, then it will Release()
  // the |lock_| when calling the demuxer and then re-Acquire() the |lock_|.
  bool SubmitReads() {
    lock_.AssertAcquired();
    bool did_read = false;
    if (IsRunning() &&
        pending_reads_ + input_queue_.size() < output_queue_.size()) {
      did_read = true;
      size_t read = output_queue_.size() - pending_reads_ - input_queue_.size();
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
  bool ProcessInput() {
    lock_.AssertAcquired();
    bool did_decode = false;
    while (IsRunning() && !input_queue_.empty()) {
      did_decode = true;
      Buffer* input = input_queue_.front();
      input_queue_.pop_front();
      // Release |lock_| before calling the derived class to do the decode.
      {
        AutoUnlock unlock(lock_);
        OnDecode(input);
        input->Release();
      }
    }
    return did_decode;
  }

  // Removes any buffers from the |result_queue_| and assigns them to a pending
  // read Assignable buffer in the |output_queue_|.
  bool ProcessOutput() {
    lock_.AssertAcquired();
    bool called_renderer = false;
    while (IsRunning() && !output_queue_.empty() && !result_queue_.empty()) {
      called_renderer = true;
      Output* output = result_queue_.front();
      result_queue_.pop_front();
      Assignable<Output>* assignable_output = output_queue_.front();
      output_queue_.pop_front();
      // Release |lock_| before calling the renderer.
      {
        AutoUnlock unlock(lock_);
        assignable_output->SetBuffer(output);
        output->Release();
        assignable_output->OnAssignment();
        assignable_output->Release();
      }
    }
    return called_renderer;
  }

  // Throw away all buffers in all queues.
  void DiscardQueues() {
    lock_.AssertAcquired();
    while (!input_queue_.empty()) {
      input_queue_.front()->Release();
      input_queue_.pop_front();
    }
    while (!result_queue_.empty()) {
      result_queue_.front()->Release();
      result_queue_.pop_front();
    }
    while (!output_queue_.empty()) {
      output_queue_.front()->Release();
      output_queue_.pop_front();
    }
  }

  // The critical section for the decoder.
  Lock lock_;

  // If false, then the Stop() method has been called, and no further processing
  // of buffers should occur.
  bool running_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  // If this pointer is NULL then there is no thread dedicated to this decoder
  // and decodes will happen on the pipeline thread.
  scoped_ptr<base::Thread> decode_thread_;

  // Number of times we have called Read() on the demuxer that have not yet
  // been satisfied.
  size_t pending_reads_;

  CancelableTask* process_task_;

  // Queue of buffers read from teh |demuxer_stream_|
  typedef std::deque<Buffer*> InputQueue;
  InputQueue input_queue_;

  // Queue of decoded samples produced in the OnDecode() method of the decoder.
  // Any samples placed in this queue will be assigned to the OutputQueue
  // buffers once the OnDecode() method returns.
  // TODO(ralphl): Eventually we want to have decoders get their destination
  // buffer from the OutputQueue and write to it directly.  Until we change
  // from the Assignable buffer to callbacks and renderer-allocated buffers,
  // we need this extra queue.
  typedef std::deque<Output*> ResultQueue;
  ResultQueue result_queue_;

  // Queue of buffers supplied by the renderer through the Read() method.
  typedef std::deque<Assignable<Output>*> OutputQueue;
  OutputQueue output_queue_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_BASE_H_
