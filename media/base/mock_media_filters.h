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
        frame_duration(base::TimeDelta::FromMicroseconds(33333)),
        media_duration(base::TimeDelta::FromSeconds(5)) {
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
  base::TimeDelta media_duration;
};


class MockDataSource : public DataSource {
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
  friend class FilterFactoryImpl1<MockDataSource,
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
  virtual bool Initialize(DataSource* data_source) {
    host_->InitializationComplete();
    return true;
  }

  virtual size_t GetNumberOfStreams() {
    if (config_->has_video) {
      return 2;
    }
    return 1;
  }

  virtual DemuxerStream* GetStream(int stream_id) {
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
  friend class FilterFactoryImpl1<MockDemuxer, const MockFilterConfig*>;

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
      return &media_format_;
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
  friend class FilterFactoryImpl1<MockAudioDecoder, const MockFilterConfig*>;

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
  friend class FilterFactoryImpl1<MockAudioRenderer, const MockFilterConfig*>;

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
        surface_locked_(false) {
    SetTimestamp(timestamp);
    SetDuration(config->frame_duration);
    size_t y_byte_count = config_->video_width * config_->video_height;
    size_t uv_byte_count = y_byte_count / 4;
    surface_.format = config_->video_surface_format;
    surface_.width = config_->video_width;
    surface_.height = config_->video_height;
    surface_.planes = 3;
    surface_.data[0] = new char[y_byte_count];
    surface_.data[1] = new char[uv_byte_count];
    surface_.data[2] = new char[uv_byte_count];
    surface_.strides[0] = config_->video_width;
    surface_.strides[1] = config_->video_width / 2;
    surface_.strides[2] = config_->video_width / 2;
    memset(surface_.data[0], 0, y_byte_count);
    memset(surface_.data[1], 0x80, uv_byte_count);
    memset(surface_.data[2], 0x80, uv_byte_count);
    int64 num_white_pixels = y_byte_count *
                             timestamp.InMicroseconds() /
                             config_->media_duration.InMicroseconds();
    if (num_white_pixels > y_byte_count) {
      ADD_FAILURE();
      num_white_pixels = y_byte_count;
    }
    if (num_white_pixels < 0) {
      ADD_FAILURE();
      num_white_pixels = 0;
    }
    memset(surface_.data[0], 0xFF, static_cast<size_t>(num_white_pixels));
  }

  virtual ~MockVideoFrame() {
    delete[] surface_.data[0];
    delete[] surface_.data[1];
    delete[] surface_.data[2];
  }

  virtual bool Lock(VideoSurface* surface) {
    EXPECT_FALSE(surface_locked_);
    if (surface_locked_) {
      memset(surface, 0, sizeof(*surface));
      return false;
    }
    surface_locked_ = true;
    DCHECK(sizeof(*surface) == sizeof(surface_));
    memcpy(surface, &surface_, sizeof(*surface));
    return true;
  }

  virtual void Unlock() {
    EXPECT_TRUE(surface_locked_);
    surface_locked_ = false;
  }

 private:
  const MockFilterConfig* config_;
  bool surface_locked_;
  VideoSurface surface_;

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
  friend class FilterFactoryImpl1<MockVideoDecoder, const MockFilterConfig*>;

  explicit MockVideoDecoder(const MockFilterConfig* config)
      : config_(config) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              config->uncompressed_video_mime_type);
    media_format_.SetAsInteger(MediaFormat::kWidth, config->video_width);
    media_format_.SetAsInteger(MediaFormat::kHeight, config->video_height);
  }

  void DoRead(Assignable<VideoFrame>* buffer) {
    if (mock_frame_time_ < config_->media_duration) {
      VideoFrame* frame = new MockVideoFrame(config_, mock_frame_time_);
      mock_frame_time_ += config_->frame_duration;
      if (mock_frame_time_ >= config_->media_duration) {
        frame->SetEndOfStream(true);
      }
      buffer->SetBuffer(frame);
      buffer->OnAssignment();
    }
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
  friend class FilterFactoryImpl1<MockVideoRenderer, const MockFilterConfig*>;

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
