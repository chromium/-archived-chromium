// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//
// A new breed of mock media filters, this time using gmock!  Feel free to add
// actions if you need interesting side-effects (i.e., copying data to the
// buffer passed into MockDataSource::Read()).
//
// Don't forget you can use StrictMock<> and NiceMock<> if you want the mock
// filters to fail the test or do nothing when an unexpected method is called.
// http://code.google.com/p/googlemock/wiki/CookBook#Nice_Mocks_and_Strict_Mocks

#ifndef MEDIA_BASE_MOCK_FILTERS_H_
#define MEDIA_BASE_MOCK_FILTERS_H_

#include "media/base/factory.h"
#include "media/base/filters.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockDataSource : public DataSource {
 public:
  MockDataSource() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));

  // DataSource implementation.
  MOCK_METHOD1(Initialize, bool(const std::string& url));
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD2(Read, size_t(uint8* data, size_t size));
  MOCK_METHOD1(GetPosition, bool(int64* position_out));
  MOCK_METHOD1(SetPosition, bool(int64 position));
  MOCK_METHOD1(GetSize, bool(int64* size_out));
  MOCK_METHOD0(IsSeekable, bool());

 protected:
  virtual ~MockDataSource() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

class MockDemuxer : public Demuxer {
 public:
  MockDemuxer() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));

  // Demuxer implementation.
  MOCK_METHOD1(Initialize, bool(DataSource* data_source));
  MOCK_METHOD0(GetNumberOfStreams, size_t());
  MOCK_METHOD1(GetStream, scoped_refptr<DemuxerStream>(int stream_id));

 protected:
  virtual ~MockDemuxer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxer);
};

class MockDemuxerStream : public DemuxerStream {
 public:
  MockDemuxerStream() {}

  // DemuxerStream implementation.
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD1(Read, void(Callback1<Buffer*>::Type* read_callback));
  MOCK_METHOD1(QueryInterface, void*(const char* interface_id));

 protected:
  virtual ~MockDemuxerStream() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDemuxerStream);
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));

  // VideoDecoder implementation.
  MOCK_METHOD1(Initialize, bool(DemuxerStream* demuxer_stream));
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD1(Read, void(Callback1<VideoFrame*>::Type* read_callback));

 protected:
  virtual ~MockVideoDecoder() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecoder);
};

class MockAudioDecoder : public AudioDecoder {
 public:
  MockAudioDecoder() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));

  // AudioDecoder implementation.
  MOCK_METHOD1(Initialize, bool(DemuxerStream* demuxer_stream));
  MOCK_METHOD0(media_format, const MediaFormat&());
  MOCK_METHOD1(Read, void(Callback1<Buffer*>::Type* read_callback));

 protected:
  virtual ~MockAudioDecoder() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioDecoder);
};

class MockVideoRenderer : public VideoRenderer {
 public:
  MockVideoRenderer() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));

  // VideoRenderer implementation.
  MOCK_METHOD1(Initialize, bool(VideoDecoder* decoder));

 protected:
  virtual ~MockVideoRenderer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRenderer);
};

class MockAudioRenderer : public AudioRenderer {
 public:
  MockAudioRenderer() {}

  // MediaFilter implementation.
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetPlaybackRate, void(float playback_rate));
  MOCK_METHOD1(Seek, void(base::TimeDelta time));

  // AudioRenderer implementation.
  MOCK_METHOD1(Initialize, bool(AudioDecoder* decoder));
  MOCK_METHOD1(SetVolume, void(float volume));

 protected:
  virtual ~MockAudioRenderer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderer);
};

// FilterFactory that returns canned instances of mock filters.  You can set
// expectations on the filters and then pass the factory into a pipeline.
class MockFilterFactory : public FilterFactory {
 public:
  MockFilterFactory()
      : data_source_(new MockDataSource()),
        demuxer_(new MockDemuxer()),
        video_decoder_(new MockVideoDecoder()),
        audio_decoder_(new MockAudioDecoder()),
        video_renderer_(new MockVideoRenderer()),
        audio_renderer_(new MockAudioRenderer()) {
  }

  virtual ~MockFilterFactory() {}

  // Mock accessors.
  MockDataSource* data_source() const { return data_source_; }
  MockDemuxer* demuxer() const { return demuxer_; }
  MockVideoDecoder* video_decoder() const { return video_decoder_; }
  MockAudioDecoder* audio_decoder() const { return audio_decoder_; }
  MockVideoRenderer* video_renderer() const { return video_renderer_; }
  MockAudioRenderer* audio_renderer() const { return audio_renderer_; }

 protected:
  MediaFilter* Create(FilterType filter_type, const MediaFormat& media_format) {
    switch (filter_type) {
      case FILTER_DATA_SOURCE:
        return data_source_;
      case FILTER_DEMUXER:
        return demuxer_;
      case FILTER_VIDEO_DECODER:
        return video_decoder_;
      case FILTER_AUDIO_DECODER:
        return audio_decoder_;
      case FILTER_VIDEO_RENDERER:
        return video_renderer_;
      case FILTER_AUDIO_RENDERER:
        return audio_renderer_;
      default:
        NOTREACHED() << "Unknown filter type: " << filter_type;
    }
    return NULL;
  }

 private:
  scoped_refptr<MockDataSource> data_source_;
  scoped_refptr<MockDemuxer> demuxer_;
  scoped_refptr<MockVideoDecoder> video_decoder_;
  scoped_refptr<MockAudioDecoder> audio_decoder_;
  scoped_refptr<MockVideoRenderer> video_renderer_;
  scoped_refptr<MockAudioRenderer> audio_renderer_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterFactory);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTERS_H_
