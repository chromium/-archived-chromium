// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_BASE_MOCK_MEDIA_FILTERS_H_
#define MEDIA_BASE_MOCK_MEDIA_FILTERS_H_

#include <string>

#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/base/pipeline.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Behaviors for MockDataSource filter.
enum MockBehavior {
  MOCK_FILTER_NORMAL_INIT,
  MOCK_FILTER_NEVER_INIT,
  MOCK_FILTER_TASK_INIT,
  MOCK_FILTER_ERROR_IN_INIT,
  MOCK_FILTER_INIT_RETURN_FALSE,
  MOCK_FILTER_TASK_ERROR_PRE_INIT,
  MOCK_FILTER_TASK_ERROR_POST_INIT
};

class MockDataSource : public media::DataSource {
 public:
  static FilterFactory* CreateFactory(MockBehavior behavior) {
     return new FilterFactoryImpl1<MockDataSource, MockBehavior>(behavior);
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of DataSource.
  virtual bool Initialize(const std::string& url) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kApplicationOctetStream);
    media_format_.SetAsString(MediaFormat::kURL, url);
    switch (behavior_) {
      case MOCK_FILTER_NORMAL_INIT:
        host_->InitializationComplete();
        return true;
      case MOCK_FILTER_NEVER_INIT:
        return true;
      case MOCK_FILTER_TASK_ERROR_POST_INIT:
        host_->InitializationComplete();
        // Yes, we want to fall through to schedule the task...
      case MOCK_FILTER_TASK_ERROR_PRE_INIT:
      case MOCK_FILTER_TASK_INIT:
        host_->PostTask(NewRunnableMethod(this, &MockDataSource::TaskBehavior));
        return true;
      case MOCK_FILTER_ERROR_IN_INIT:
        host_->Error(PIPELINE_ERROR_NETWORK);
        return false;
      case MOCK_FILTER_INIT_RETURN_FALSE:
        return false;
      default:
        NOTREACHED();
        return false;
    }
  }

  virtual const MediaFormat* GetMediaFormat() {
    return &media_format_;
  }

  virtual size_t Read(char* data, size_t size) {
    return 0;
  }

  virtual bool GetPosition(int64* position_out) {
    *position_out = 0;
    return false;
  }

  virtual bool SetPosition(int64 position) {
    return true;
  }

  virtual bool GetSize(int64* size_out) {
    *size_out = 0;
    return false;
  }

 private:
  friend class media::FilterFactoryImpl1<MockDataSource, MockBehavior>;

  explicit MockDataSource(MockBehavior behavior) : behavior_(behavior) {}

  virtual ~MockDataSource() {}

  void TaskBehavior() {
    switch (behavior_) {
      case MOCK_FILTER_TASK_ERROR_POST_INIT:
      case MOCK_FILTER_TASK_ERROR_PRE_INIT:
        host_->Error(PIPELINE_ERROR_NETWORK);
        break;
      case MOCK_FILTER_TASK_INIT:
        host_->InitializationComplete();
        break;
      default:
        NOTREACHED();
    }
  }

  MockBehavior behavior_;
  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

//------------------------------------------------------------------------------

class MockDemuxer : public Demuxer {
 public:
  static FilterFactory* CreateFactory() {
    return new FilterFactoryImpl0<MockDemuxer>();
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of Demuxer.
  virtual bool Initialize(media::DataSource* data_source) {
    host_->InitializationComplete();
    return true;
  }

  virtual size_t GetNumberOfStreams() {
    return 1;
  }

  virtual media::DemuxerStream* GetStream(int stream_id) {
    EXPECT_EQ(stream_id, 0);
    return &mock_demuxer_stream_;
  }

 private:
  friend class media::FilterFactoryImpl0<MockDemuxer>;

  MockDemuxer() {}

  virtual ~MockDemuxer() {}

  // Internal class implements DemuxerStream interface.
  class MockDemuxerStream : public DemuxerStream {
   public:
    MockDemuxerStream() {
      media_format_.SetAsString(MediaFormat::kMimeType,
                                media::mime_type::kUncompressedAudio);
    }

    virtual ~MockDemuxerStream() {}

    // Implementation of DemuxerStream.
    virtual const MediaFormat* GetMediaFormat() {
      return &media_format_;  // TODO(ralphl): implement
    }

    virtual void Read(Assignable<Buffer>* buffer) {
      NOTREACHED();  // TODO(ralphl): fix me!!
    }

   private:
    MediaFormat media_format_;

    DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
  };

  MockDemuxerStream mock_demuxer_stream_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

//------------------------------------------------------------------------------

class MockAudioDecoder : public AudioDecoder {
 public:
  static FilterFactory* CreateFactory() {
    return new FilterFactoryImpl0<MockAudioDecoder>();
  }

  static bool IsMediaFormatSupported(const MediaFormat* media_format) {
    return true;  // TODO(ralphl): check for a supported format.
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of AudioDecoder.
  virtual bool Initialize(DemuxerStream* stream) {
    host_->InitializationComplete();
    return true;
  }

  virtual const MediaFormat* GetMediaFormat() {
    return &media_format_;
  }

  virtual void Read(Assignable<Buffer>* buffer) {
    // TODO(ralphl): implement mock read.
    NOTREACHED();
  }

 private:
  friend class media::FilterFactoryImpl0<MockAudioDecoder>;

  MockAudioDecoder() {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              media::mime_type::kUncompressedAudio);
  }

  virtual ~MockAudioDecoder() {}

  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioDecoder);
};

//------------------------------------------------------------------------------

class MockAudioRenderer : public AudioRenderer {
 public:
  static FilterFactory* CreateFactory() {
    return new FilterFactoryImpl0<MockAudioRenderer>();
  }

  static bool IsMediaFormatSupported(const MediaFormat* media_format) {
    return true;  // TODO(ralphl): check for a supported format
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of AudioRenderer.
  virtual bool Initialize(AudioDecoder* decoder) {
    host_->InitializationComplete();
    return true;
  }

  virtual void SetVolume(float volume) {}

 private:
  friend class media::FilterFactoryImpl0<MockAudioRenderer>;

  MockAudioRenderer() {}

  virtual ~MockAudioRenderer() {}

  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

//------------------------------------------------------------------------------
// Simple class that derives from the WaitableEvent class.  The event remains
// in the reset state until the initialization complete callback is called from
// a media pipeline.  The normal use of this object looks like:
//   Pipeline p;
//   FilterFactoryCollection f;
//   f->AddFactory(a);
//   f->AddFactory(b);
//   ...
//   InitializationHelper h;
//   h.Start(&p, f, uri);
//   h.Wait();
//   (when the Wait() returns, the pipeline is initialized or in error state)
class InitializationHelper : public base::WaitableEvent {
 public:
  InitializationHelper()
    : WaitableEvent(true, false),
      callback_success_status_(false),
      waiting_for_callback_(false) {}

  // If callback has been called, then returns the boolean passed by the
  // pipeline to the callback.
  bool callback_success_status() { return callback_success_status_; }

  // Returns true if Start has been called, but the pipeline has not yet
  // called the intialization complete callback.
  bool waiting_for_callback() { return waiting_for_callback_; }

  // Starts the pipeline, providing an initialization callback that points
  // to this object.
  void Start(Pipeline* pipeline,
             FilterFactory* filter_factory,
             const std::string& uri) {
    Reset();
    waiting_for_callback_ = true;
    pipeline->Start(filter_factory, uri,
                    NewCallback(this, &InitializationHelper::InitCallback));
  }

  // Resets the state.   This method should not be called if waiting for
  // a callback from a previous call to Start.  Note that the Start method
  // resets the state, so callers are not required to call this method prior
  // to calling the start method.
  void Reset() {
    EXPECT_FALSE(waiting_for_callback_);
    base::WaitableEvent::Reset();
    callback_success_status_ = false;
  }

 private:
  void InitCallback(bool success) {
    EXPECT_TRUE(waiting_for_callback_);
    EXPECT_FALSE(IsSignaled());
    waiting_for_callback_ = false;
    callback_success_status_ = success;
    Signal();
  }

  bool callback_success_status_;
  bool waiting_for_callback_;

  DISALLOW_COPY_AND_ASSIGN(InitializationHelper);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_MEDIA_FILTERS_H_
