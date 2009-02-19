// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_BASE_MOCK_MEDIA_FILTERS_H_
#define MEDIA_BASE_MOCK_MEDIA_FILTERS_H_

#include <string>

#include "base/waitable_event.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/base/pipeline.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Behaviors for MockDataSource filter.
enum MockDataSourceBehavior {
  MOCK_DATA_SOURCE_NORMAL_INIT,
  MOCK_DATA_SOURCE_NEVER_INIT,
  MOCK_DATA_SOURCE_TASK_INIT,
  MOCK_DATA_SOURCE_ERROR_IN_INIT,
  MOCK_DATA_SOURCE_INIT_RETURN_FALSE,
  MOCK_DATA_SOURCE_TASK_ERROR_PRE_INIT,
  MOCK_DATA_SOURCE_TASK_ERROR_POST_INIT
};


// This class is used by all of the mock filters to change the configuration
// of the desired pipeline.  The test using this must ensure that the lifetime
// of the object is at least as long as the lifetime of the filters, as this
// is typically allocated on the stack.
struct MockFilterConfig {
  MockFilterConfig()
      : data_source_behavior(MOCK_DATA_SOURCE_NORMAL_INIT),
        has_video(true),
        video_width(1280u),
        video_height(720u),
        video_surface_format(VideoSurface::YV12),
        compressed_audio_mime_type(mime_type::kAACAudio),
        uncompressed_audio_mime_type(mime_type::kUncompressedAudio),
        compressed_video_mime_type(mime_type::kH264AnnexB),
        uncompressed_video_mime_type(mime_type::kUncompressedVideo),
        frame_duration(base::TimeDelta::FromMicroseconds(33333)) {
  }

  MockDataSourceBehavior data_source_behavior;
  bool has_video;
  size_t video_width;
  size_t video_height;
  VideoSurface::Format video_surface_format;
  std::string compressed_audio_mime_type;
  std::string uncompressed_audio_mime_type;
  std::string compressed_video_mime_type;
  std::string uncompressed_video_mime_type;
  base::TimeDelta frame_duration;
};


class MockDataSource : public media::DataSource {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
     return new FilterFactoryImpl1<MockDataSource,
                                   const MockFilterConfig*>(config);
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of DataSource.
  virtual bool Initialize(const std::string& url) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kApplicationOctetStream);
    media_format_.SetAsString(MediaFormat::kURL, url);
    switch (behavior_) {
      case MOCK_DATA_SOURCE_NORMAL_INIT:
        host_->InitializationComplete();
        return true;
      case MOCK_DATA_SOURCE_NEVER_INIT:
        return true;
      case MOCK_DATA_SOURCE_TASK_ERROR_POST_INIT:
        host_->InitializationComplete();
        // Yes, we want to fall through to schedule the task...
      case MOCK_DATA_SOURCE_TASK_ERROR_PRE_INIT:
      case MOCK_DATA_SOURCE_TASK_INIT:
        host_->PostTask(NewRunnableMethod(this, &MockDataSource::TaskBehavior));
        return true;
      case MOCK_DATA_SOURCE_ERROR_IN_INIT:
        host_->Error(PIPELINE_ERROR_NETWORK);
        return false;
      case MOCK_DATA_SOURCE_INIT_RETURN_FALSE:
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
  friend class media::FilterFactoryImpl1<MockDataSource,
                                         const MockFilterConfig*>;

  explicit MockDataSource(const MockFilterConfig* config)
      : behavior_(config->data_source_behavior) {
  }

  virtual ~MockDataSource() {}

  void TaskBehavior() {
    switch (behavior_) {
      case MOCK_DATA_SOURCE_TASK_ERROR_POST_INIT:
      case MOCK_DATA_SOURCE_TASK_ERROR_PRE_INIT:
        host_->Error(PIPELINE_ERROR_NETWORK);
        break;
      case MOCK_DATA_SOURCE_TASK_INIT:
        host_->InitializationComplete();
        break;
      default:
        NOTREACHED();
    }
  }

  MockDataSourceBehavior behavior_;
  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

//------------------------------------------------------------------------------

class MockDemuxer : public Demuxer {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
     return new FilterFactoryImpl1<MockDemuxer,
                                   const MockFilterConfig*>(config);
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of Demuxer.
  virtual bool Initialize(media::DataSource* data_source) {
    host_->InitializationComplete();
    return true;
  }

  virtual size_t GetNumberOfStreams() {
    if (config_->has_video) {
      return 2;
    }
    return 1;
  }

  virtual media::DemuxerStream* GetStream(int stream_id) {
    switch (stream_id) {
      case 0:
        return &mock_audio_stream_;
      case 1:
        if (config_->has_video) {
          return &mock_video_stream_;
        }
        // Fall-through is correct if no video.
      default:
        ADD_FAILURE();
        return NULL;
    }
  }

 private:
  friend class media::FilterFactoryImpl1<MockDemuxer, const MockFilterConfig*>;

  explicit MockDemuxer(const MockFilterConfig* config)
      : config_(config),
        mock_audio_stream_(config->compressed_audio_mime_type),
        mock_video_stream_(config->compressed_video_mime_type) {
  }

  virtual ~MockDemuxer() {}

  // Internal class implements DemuxerStream interface.
  class MockDemuxerStream : public DemuxerStream {
   public:
    explicit MockDemuxerStream(const std::string& mime_type) {
      media_format_.SetAsString(MediaFormat::kMimeType, mime_type);
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

  const MockFilterConfig* config_;
  MockDemuxerStream mock_audio_stream_;
  MockDemuxerStream mock_video_stream_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

//------------------------------------------------------------------------------

class MockAudioDecoder : public AudioDecoder {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
    return new FilterFactoryImpl1<MockAudioDecoder,
                                  const MockFilterConfig*>(config);
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
  friend class media::FilterFactoryImpl1<MockAudioDecoder,
                                         const MockFilterConfig*>;

  explicit MockAudioDecoder(const MockFilterConfig* config) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              config->uncompressed_audio_mime_type);
  }

  virtual ~MockAudioDecoder() {}

  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioDecoder);
};

//------------------------------------------------------------------------------

class MockAudioRenderer : public AudioRenderer {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
    return new FilterFactoryImpl1<MockAudioRenderer,
                                  const MockFilterConfig*>(config);
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
  friend class media::FilterFactoryImpl1<MockAudioRenderer,
                                         const MockFilterConfig*>;

  explicit MockAudioRenderer(const MockFilterConfig* config) {}

  virtual ~MockAudioRenderer() {}

  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

//------------------------------------------------------------------------------

class MockVideoFrame : public VideoFrame {
 public:
  explicit MockVideoFrame(const MockFilterConfig* config,
                          base::TimeDelta timestamp)
      : config_(config),
        surface_locked_(false),
        timestamp_(timestamp),
        duration_(config->frame_duration) {
  }

  virtual ~MockVideoFrame() {}

  virtual base::TimeDelta GetTimestamp() const {
    return timestamp_;
  }

  virtual base::TimeDelta GetDuration() const {
    return duration_;
  }

  virtual void SetTimestamp(const base::TimeDelta& timestamp) {
    timestamp_ = timestamp;
  }

  virtual void SetDuration(const base::TimeDelta& duration) {
    duration_ = duration;
  }

  virtual bool Lock(VideoSurface* surface) {
    EXPECT_FALSE(surface_locked_);
    surface_locked_ = true;
    surface->format = config_->video_surface_format;
    surface->width = config_->video_width;
    surface->height = config_->video_height;
    // TODO(ralphl): mock the data for video surfaces too.
    surface->planes = 3;
    surface->data[0] = NULL;
    surface->data[1] = NULL;
    surface->data[2] = NULL;
    surface->strides[0] = 0;
    surface->strides[1] = 0;
    surface->strides[2] = 0;
    return false;
  }

  virtual void Unlock() {
    EXPECT_TRUE(surface_locked_);
    surface_locked_ = false;
  }

 private:
  const MockFilterConfig* config_;
  bool surface_locked_;
  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoFrame);
};

class MockVideoDecoder : public VideoDecoder {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
    return new FilterFactoryImpl1<MockVideoDecoder,
                                  const MockFilterConfig*>(config);
  }

  static bool IsMediaFormatSupported(const MediaFormat* media_format) {
    return true;  // TODO(ralphl): check for a supported format.
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of VideoDecoder.
  virtual bool Initialize(DemuxerStream* stream) {
    host_->InitializationComplete();
    return true;
  }

  virtual const MediaFormat* GetMediaFormat() {
    return &media_format_;
  }

  virtual void Read(Assignable<VideoFrame>* buffer) {
    buffer->AddRef();
    host_->PostTask(NewRunnableMethod(this, &MockVideoDecoder::DoRead, buffer));
  }

 private:
  friend class media::FilterFactoryImpl1<MockVideoDecoder,
                                        const MockFilterConfig*>;

  explicit MockVideoDecoder(const MockFilterConfig* config)
      : config_(config) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              config->uncompressed_video_mime_type);
    media_format_.SetAsInteger(MediaFormat::kWidth, config->video_width);
    media_format_.SetAsInteger(MediaFormat::kHeight, config->video_height);
  }

  void DoRead(Assignable<VideoFrame>* buffer) {
    VideoFrame* frame = new MockVideoFrame(config_, mock_frame_time_);
    mock_frame_time_ += config_->frame_duration;
    buffer->SetBuffer(frame);
    buffer->OnAssignment();
    buffer->Release();
  }

  virtual ~MockVideoDecoder() {}

  MediaFormat media_format_;
  base::TimeDelta mock_frame_time_;
  const MockFilterConfig* config_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

//------------------------------------------------------------------------------

class MockVideoRenderer : public VideoRenderer {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
    return new FilterFactoryImpl1<MockVideoRenderer,
                                  const MockFilterConfig*>(config);
  }

  static bool IsMediaFormatSupported(const MediaFormat* media_format) {
    return true;  // TODO(ralphl): check for a supported format
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of VideoRenderer.
  virtual bool Initialize(VideoDecoder* decoder) {
    host_->SetVideoSize(config_->video_width, config_->video_height);
    host_->InitializationComplete();
    return true;
  }

 private:
  friend class media::FilterFactoryImpl1<MockVideoRenderer,
                                         const MockFilterConfig*>;

  explicit MockVideoRenderer(const MockFilterConfig* config)
      : config_(config) {
  }

  virtual ~MockVideoRenderer() {}

  const MockFilterConfig* config_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoRenderer);
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
