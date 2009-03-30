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
#include "media/base/video_frame_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Behaviors for MockDataSource filter.
enum MockDataSourceBehavior {
  MOCK_DATA_SOURCE_NORMAL_INIT,
  MOCK_DATA_SOURCE_NEVER_INIT,
  MOCK_DATA_SOURCE_TASK_INIT,
  MOCK_DATA_SOURCE_URL_ERROR_IN_INIT,
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
        data_source_value('!'),
        has_video(true),
        video_width(1280u),
        video_height(720u),
        video_surface_format(VideoSurface::YV12),
        has_audio(true),
        compressed_audio_mime_type(mime_type::kAACAudio),
        uncompressed_audio_mime_type(mime_type::kUncompressedAudio),
        compressed_video_mime_type(mime_type::kH264AnnexB),
        uncompressed_video_mime_type(mime_type::kUncompressedVideo),
        frame_duration(base::TimeDelta::FromMicroseconds(33333)),
        media_duration(base::TimeDelta::FromSeconds(5)),
        media_total_bytes(media_duration.InMilliseconds() * 250) {
  }

  MockDataSourceBehavior data_source_behavior;
  char data_source_value;
  bool has_video;
  size_t video_width;
  size_t video_height;
  VideoSurface::Format video_surface_format;
  bool has_audio;
  std::string compressed_audio_mime_type;
  std::string uncompressed_audio_mime_type;
  std::string compressed_video_mime_type;
  std::string uncompressed_video_mime_type;
  base::TimeDelta frame_duration;
  base::TimeDelta media_duration;
  int64 media_total_bytes;
};


class MockDataSource : public DataSource {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
     return new FilterFactoryImpl1<MockDataSource,
                                   const MockFilterConfig*>(config);
  }

  explicit MockDataSource(const MockFilterConfig* config)
      : config_(config),
        position_(0),
        deleted_(NULL) {
  }

  MockDataSource(const MockFilterConfig* config, bool* deleted)
      : config_(config),
        position_(0),
        deleted_(deleted) {
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(*deleted);
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of DataSource.
  virtual bool Initialize(const std::string& url) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kApplicationOctetStream);
    media_format_.SetAsString(MediaFormat::kURL, url);
    host_->SetTotalBytes(config_->media_total_bytes);
    switch (config_->data_source_behavior) {
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
      case MOCK_DATA_SOURCE_URL_ERROR_IN_INIT:
        host_->Error(PIPELINE_ERROR_URL_NOT_FOUND);
        return false;
      case MOCK_DATA_SOURCE_INIT_RETURN_FALSE:
        return false;
      default:
        NOTREACHED();
        return false;
    }
  }

  virtual const MediaFormat& media_format() {
    return media_format_;
  }

  virtual size_t Read(uint8* data, size_t size) {
    size_t read = static_cast<size_t>(config_->media_total_bytes - position_);
    if (size < read) {
      read = size;
    }
    memset(data, config_->data_source_value, read);
    return read;
  }

  virtual bool GetPosition(int64* position_out) {
    *position_out = position_;
    return true;
  }

  virtual bool SetPosition(int64 position) {
    if (position < 0u || position > config_->media_total_bytes) {
      return false;
    }
    position_ = position;
    return true;
  }

  virtual bool GetSize(int64* size_out) {
    if (config_->media_total_bytes >= 0) {
      *size_out = config_->media_total_bytes;
      return true;
    }
    return false;
  }

  virtual bool IsSeekable() {
    return true;
  }

  // Simple position getter for unit testing.
  int64 position() const { return position_; }

 private:
  virtual ~MockDataSource() {
    if (deleted_) {
      *deleted_ = true;
    }
  }

  void TaskBehavior() {
    switch (config_->data_source_behavior) {
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

  const MockFilterConfig* config_;
  int64 position_;
  MediaFormat media_format_;

  // Set to true inside the destructor.  Used in FFmpegGlue unit tests for
  // testing proper reference counting.
  bool* deleted_;

  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

//------------------------------------------------------------------------------

class MockDemuxerStream : public DemuxerStream {
 public:
  MockDemuxerStream(const MockFilterConfig* config, bool is_audio) {
    if (is_audio) {
      media_format_.SetAsString(MediaFormat::kMimeType,
                                config->compressed_audio_mime_type);
    } else {
      media_format_.SetAsString(MediaFormat::kMimeType,
                                config->compressed_video_mime_type);
      media_format_.SetAsInteger(MediaFormat::kWidth, config->video_width);
      media_format_.SetAsInteger(MediaFormat::kHeight, config->video_height);
    }
  }

  // Implementation of DemuxerStream.
  virtual const MediaFormat& media_format() {
    return media_format_;
  }

  virtual void Read(Callback1<Buffer*>::Type* read_callback) {
    NOTREACHED();  // TODO(ralphl): fix me!!
  }

 private:
  virtual ~MockDemuxerStream() {}

  MediaFormat media_format_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
};

//------------------------------------------------------------------------------

class MockDemuxer : public Demuxer {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
     return new FilterFactoryImpl1<MockDemuxer,
                                   const MockFilterConfig*>(config);
  }

  explicit MockDemuxer(const MockFilterConfig* config)
      : config_(config),
        mock_audio_stream_(new MockDemuxerStream(config, true)),
        mock_video_stream_(new MockDemuxerStream(config, false)) {
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of Demuxer.
  virtual bool Initialize(DataSource* data_source) {
    host_->InitializationComplete();
    return true;
  }

  virtual size_t GetNumberOfStreams() {
    size_t num_streams = 0;
    if (config_->has_audio) {
      ++num_streams;
    }
    if (config_->has_video) {
      ++num_streams;
    }
    return num_streams;
  }

  virtual scoped_refptr<DemuxerStream> GetStream(int stream_id) {
    switch (stream_id) {
      case 0:
        if (config_->has_audio) {
          return mock_audio_stream_;
        } else if (config_->has_video) {
          return mock_video_stream_;
        }
        break;
      case 1:
        if (config_->has_audio && config_->has_video) {
          return mock_video_stream_;
        }
        break;
    }
    ADD_FAILURE();
    return NULL;
  }

 private:
  virtual ~MockDemuxer() {}

  const MockFilterConfig* config_;
  scoped_refptr<DemuxerStream> mock_audio_stream_;
  scoped_refptr<DemuxerStream> mock_video_stream_;

  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

//------------------------------------------------------------------------------

class MockAudioDecoder : public AudioDecoder {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
    return new FilterFactoryImpl1<MockAudioDecoder,
                                  const MockFilterConfig*>(config);
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format) {
    return true;  // TODO(ralphl): check for a supported format.
  }

  explicit MockAudioDecoder(const MockFilterConfig* config) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              config->uncompressed_audio_mime_type);
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of AudioDecoder.
  virtual bool Initialize(DemuxerStream* stream) {
    host_->InitializationComplete();
    return true;
  }

  virtual const MediaFormat& media_format() {
    return media_format_;
  }

  virtual void Read(Callback1<Buffer*>::Type* callback) {
    // TODO(ralphl): implement mock read.
    NOTREACHED();
  }

 private:
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

  static bool IsMediaFormatSupported(const MediaFormat& media_format) {
    return true;  // TODO(ralphl): check for a supported format
  }

  explicit MockAudioRenderer(const MockFilterConfig* config) {}

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of AudioRenderer.
  virtual bool Initialize(AudioDecoder* decoder) {
    host_->InitializationComplete();
    return true;
  }

  virtual void SetVolume(float volume) {}

 private:
  virtual ~MockAudioRenderer() {}

  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

//------------------------------------------------------------------------------

class MockVideoDecoder : public VideoDecoder {
 public:
  static FilterFactory* CreateFactory(const MockFilterConfig* config) {
    return new FilterFactoryImpl1<MockVideoDecoder,
                                  const MockFilterConfig*>(config);
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format) {
    return true;  // TODO(ralphl): check for a supported format.
  }

  // Helper function that initializes a YV12 frame with white and black scan
  // lines based on the |white_to_black| parameter.  If 0, then the entire
  // frame will be black, if 1 then the entire frame will be white.
  static void InitializeYV12Frame(VideoFrame* frame, double white_to_black) {
    VideoSurface surface;
    if (!frame->Lock(&surface)) {
      ADD_FAILURE();
    } else {
      EXPECT_EQ(surface.format, VideoSurface::YV12);
      size_t first_black_row = static_cast<size_t>(surface.height *
                                                   white_to_black);
      uint8* y_plane = surface.data[VideoSurface::kYPlane];
      for (size_t row = 0; row < surface.height; ++row) {
        int color = (row < first_black_row) ? 0xFF : 0x00;
        memset(y_plane, color, surface.width);
        y_plane += surface.strides[VideoSurface::kYPlane];
      }
      uint8* u_plane = surface.data[VideoSurface::kUPlane];
      uint8* v_plane = surface.data[VideoSurface::kVPlane];
      for (size_t row = 0; row < surface.height; row += 2) {
        memset(u_plane, 0x80, surface.width / 2);
        memset(v_plane, 0x80, surface.width / 2);
        u_plane += surface.strides[VideoSurface::kUPlane];
        v_plane += surface.strides[VideoSurface::kVPlane];
      }
      frame->Unlock();
    }
  }

  explicit MockVideoDecoder(const MockFilterConfig* config)
      : config_(config) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              config->uncompressed_video_mime_type);
    media_format_.SetAsInteger(MediaFormat::kWidth, config->video_width);
    media_format_.SetAsInteger(MediaFormat::kHeight, config->video_height);
  }

  // Implementation of MediaFilter.
  virtual void Stop() {}

  // Implementation of VideoDecoder.
  virtual bool Initialize(DemuxerStream* stream) {
    host_->InitializationComplete();
    return true;
  }

  virtual const MediaFormat& media_format() {
    return media_format_;
  }

  virtual void Read(Callback1<VideoFrame*>::Type* callback) {
    host_->PostTask(NewRunnableMethod(
        this, &MockVideoDecoder::DoRead, callback));
  }

 private:
  virtual ~MockVideoDecoder() {}

  void DoRead(Callback1<VideoFrame*>::Type* callback) {
    scoped_ptr<Callback1<VideoFrame*>::Type> scoped_callback(callback);
    if (mock_frame_time_ < config_->media_duration) {
      // TODO(ralphl): Mock video decoder only works with YV12.  Implement other
      // formats as needed.
      EXPECT_EQ(config_->video_surface_format, VideoSurface::YV12);
      scoped_refptr<VideoFrame> frame;
      VideoFrameImpl::CreateFrame(config_->video_surface_format,
                                  config_->video_width,
                                  config_->video_height,
                                  mock_frame_time_,
                                  config_->frame_duration,
                                  &frame);
      if (!frame) {
        host_->Error(PIPELINE_ERROR_OUT_OF_MEMORY);
        ADD_FAILURE();
      } else {
        mock_frame_time_ += config_->frame_duration;
        if (mock_frame_time_ >= config_->media_duration) {
          frame->SetEndOfStream(true);
        }
        InitializeYV12Frame(frame, (mock_frame_time_.InSecondsF() /
                                    config_->media_duration.InSecondsF()));
        callback->Run(frame);
      }
    }
  }

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

  static bool IsMediaFormatSupported(const MediaFormat& media_format) {
    return true;  // TODO(ralphl): check for a supported format
  }

  explicit MockVideoRenderer(const MockFilterConfig* config)
      : config_(config) {
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
  virtual ~MockVideoRenderer() {}

  const MockFilterConfig* config_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoRenderer);
};


//------------------------------------------------------------------------------
// A simple class that waits for a pipeline to be started and checks some
// basic initialization values.  The Start() method will not return until
// either a pre-dermined amount of time has passed or the pipeline calls the
// InitCallback() callback.  A typical use would be:
//   Pipeline p;
//   FilterFactoryCollection f;
//   f->AddFactory(a);
//   f->AddFactory(b);
//   ...
//   InitializationHelper h;
//   h.Start(&p, f, uri);
//
// If the test is expecting to produce an error use would be:
//   h.Start(&p, f, uri, PIPELINE_ERROR_REQUIRED_FILTER_MISSING)
//
// If the test expects the pipeline to hang during initialization (a filter
// never calls FilterHost::InitializationComplete()) then the use would be:
//   h.Start(&p, f, uri, PIPELINE_OK, true);
class InitializationHelper {
 public:
  InitializationHelper()
    : event_(true, false),
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
             const std::string& uri,
             PipelineError expect_error = PIPELINE_OK,
             bool expect_hang = false) {
    // For tests that we expect to hang in initialization, we want to
    // wait a short time.  If a hang is not expected, then wait long enough
    // to make sure that the filters have time to initalize.  1/2 second if
    // we expect to hang, and 3 seconds if we expect success.
    base::TimeDelta max_wait = base::TimeDelta::FromMilliseconds(expect_hang ?
                                                                 500 : 3000);
    EXPECT_FALSE(waiting_for_callback_);
    waiting_for_callback_ = true;
    callback_success_status_ = false;
    event_.Reset();
    pipeline->Start(filter_factory, uri,
                    NewCallback(this, &InitializationHelper::InitCallback));
    bool signaled = event_.TimedWait(max_wait);
    if (expect_hang) {
      EXPECT_FALSE(signaled);
      EXPECT_FALSE(pipeline->IsInitialized());
      EXPECT_TRUE(waiting_for_callback_);
    } else {
      EXPECT_TRUE(signaled);
      EXPECT_FALSE(waiting_for_callback_);
      EXPECT_EQ(pipeline->GetError(), expect_error);
      EXPECT_EQ(callback_success_status_, (expect_error == PIPELINE_OK));
      EXPECT_EQ(pipeline->IsInitialized(), (expect_error == PIPELINE_OK));
    }
  }

 private:
  void InitCallback(bool success) {
    EXPECT_TRUE(waiting_for_callback_);
    EXPECT_FALSE(event_.IsSignaled());
    waiting_for_callback_ = false;
    callback_success_status_ = success;
    event_.Signal();
  }

  base::WaitableEvent event_;
  bool callback_success_status_;
  bool waiting_for_callback_;

  DISALLOW_COPY_AND_ASSIGN(InitializationHelper);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_MEDIA_FILTERS_H_
