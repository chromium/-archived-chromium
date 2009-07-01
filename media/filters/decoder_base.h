// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// A base class that provides the plumbing for a decoder filters.

#ifndef MEDIA_FILTERS_DECODER_BASE_H_
#define MEDIA_FILTERS_DECODER_BASE_H_

#include <deque>

#include "base/lock.h"
#include "base/stl_util-inl.h"
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
    message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::StopTask));
  }

  virtual void Seek(base::TimeDelta time) {
    message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::SeekTask, time));
  }

  // Decoder implementation.
  virtual bool Initialize(DemuxerStream* demuxer_stream) {
    message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::InitializeTask, demuxer_stream));
    return true;
  }

  virtual const MediaFormat& media_format() { return media_format_; }

  // Audio or video decoder.
  virtual void Read(ReadCallback* read_callback) {
    message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::ReadTask, read_callback));
  }

  void OnReadComplete(Buffer* buffer) {
    // Little bit of magic here to get NewRunnableMethod() to generate a Task
    // that holds onto a reference via scoped_refptr<>.
    //
    // TODO(scherkus): change the callback format to pass a scoped_refptr<> or
    // better yet see if we can get away with not using reference counting.
    scoped_refptr<Buffer> buffer_ref = buffer;
    message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &DecoderBase::ReadCompleteTask, buffer_ref));
  }

 protected:
  DecoderBase()
      : pending_reads_(0),
        seeking_(false),
        state_(UNINITIALIZED),
        thread_id_(NULL) {
  }

  virtual ~DecoderBase() {
    DCHECK(state_ == UNINITIALIZED || state_ == STOPPED);
    DCHECK(result_queue_.empty());
    DCHECK(read_queue_.empty());
  }

  // This method is called by the derived class from within the OnDecode method.
  // It places an output buffer in the result queue.  It must be called from
  // within the OnDecode method.
  void EnqueueResult(Output* output) {
    DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);
    if (!IsStopped()) {
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

  // Used for subclasses who friend unit tests and need to set the thread id.
  virtual void set_thread_id(PlatformThreadId thread_id) {
    thread_id_ = thread_id;
  }

  MediaFormat media_format_;

 private:
  // GCC doesn't let us access superclass member variables directly, so use
  // a helper to get around the situation.
  //
  // TODO(scherkus): another reason to add protected accessors to MediaFilter.
  FilterHost* host() const { return Decoder::host_; }
  MessageLoop* message_loop() const { return Decoder::message_loop_; }
  bool IsStopped() { return state_ == STOPPED; }

  void StopTask() {
    DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);
    // Delegate to the subclass first.
    OnStop();

    // Throw away all buffers in all queues.
    result_queue_.clear();
    STLDeleteElements(&read_queue_);
    state_ = STOPPED;
  }

  void SeekTask(base::TimeDelta time) {
    DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);
    // Delegate to the subclass first.
    OnSeek(time);

    // Flush the result queue.
    result_queue_.clear();

    // Turn on the seeking flag so that we can discard buffers until a
    // discontinuous buffer is received.
    seeking_ = true;
  }

  void InitializeTask(DemuxerStream* demuxer_stream) {
    DCHECK(state_ == UNINITIALIZED);
    DCHECK(!demuxer_stream_);
    DCHECK(!thread_id_ || thread_id_ == PlatformThread::CurrentId());
    demuxer_stream_ = demuxer_stream;

    // Grab the thread id for debugging.
    thread_id_ = PlatformThread::CurrentId();

    // Delegate to subclass first.
    if (!OnInitialize(demuxer_stream_)) {
      host()->Error(PIPELINE_ERROR_DECODE);
      return;
    }

    // TODO(scherkus): subclass shouldn't mutate superclass media format.
    DCHECK(!media_format_.empty()) << "Subclass did not set media_format_";
    state_ = INITIALIZED;
    host()->InitializationComplete();
  }

  void ReadTask(ReadCallback* read_callback) {
    DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);
    // TODO(scherkus): should reply with a null operation (empty buffer).
    if (IsStopped()) {
      delete read_callback;
      return;
    }

    // Enqueue the callback and attempt to fulfill it immediately.
    read_queue_.push_back(read_callback);
    FulfillPendingRead();

    // Issue reads as necessary.
    while (pending_reads_ < read_queue_.size()) {
      demuxer_stream_->Read(NewCallback(this, &DecoderBase::OnReadComplete));
      ++pending_reads_;
    }
  }

  void ReadCompleteTask(scoped_refptr<Buffer> buffer) {
    DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);
    DCHECK_GT(pending_reads_, 0u);
    --pending_reads_;
    if (IsStopped()) {
      return;
    }

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
    if (seeking_) {
      return;
    }

    // Decode the frame right away.
    OnDecode(buffer);

    // Attempt to fulfill a pending read callback and schedule additional reads
    // if necessary.
    FulfillPendingRead();

    // Issue reads as necessary.
    //
    // Note that it's possible for us to decode but not produce a frame, in
    // which case |pending_reads_| will remain less than |read_queue_| so we
    // need to schedule an additional read.
    DCHECK_LE(pending_reads_, read_queue_.size());
    while (pending_reads_ < read_queue_.size()) {
      demuxer_stream_->Read(NewCallback(this, &DecoderBase::OnReadComplete));
      ++pending_reads_;
    }
  }

  // Attempts to fulfill a single pending read by dequeuing a buffer and read
  // callback pair and executing the callback.
  void FulfillPendingRead() {
    DCHECK_EQ(PlatformThread::CurrentId(), thread_id_);
    if (read_queue_.empty() || result_queue_.empty()) {
      return;
    }

    // Dequeue a frame and read callback pair.
    scoped_refptr<Output> output = result_queue_.front();
    scoped_ptr<ReadCallback> read_callback(read_queue_.front());
    result_queue_.pop_front();
    read_queue_.pop_front();

    // Execute the callback!
    read_callback->Run(output);
  }

  // Tracks the number of asynchronous reads issued to |demuxer_stream_|.
  // Using size_t since it is always compared against deque::size().
  size_t pending_reads_;

  // An internal state of the decoder that indicates that are waiting for seek
  // to complete. We expect to receive a discontinuous frame/packet from the
  // demuxer to signal that seeking is completed.
  bool seeking_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  // Queue of decoded samples produced in the OnDecode() method of the decoder.
  // Any samples placed in this queue will be assigned to the OutputQueue
  // buffers once the OnDecode() method returns.
  //
  // TODO(ralphl): Eventually we want to have decoders get their destination
  // buffer from the OutputQueue and write to it directly.  Until we change
  // from the Assignable buffer to callbacks and renderer-allocated buffers,
  // we need this extra queue.
  typedef std::deque<scoped_refptr<Output> > ResultQueue;
  ResultQueue result_queue_;

  // Queue of callbacks supplied by the renderer through the Read() method.
  typedef std::deque<ReadCallback*> ReadQueue;
  ReadQueue read_queue_;

  // Simple state tracking variable.
  enum State {
    UNINITIALIZED,
    INITIALIZED,
    STOPPED,
  };
  State state_;

  // Used for debugging.
  PlatformThreadId thread_id_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_BASE_H_
