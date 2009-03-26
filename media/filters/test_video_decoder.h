// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.
//

#ifndef MEDIA_FILTERS_TEST_VIDEO_DECODER_H_
#define MEDIA_FILTERS_TEST_VIDEO_DECODER_H_

#include <string>

#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/video_frame_impl.h"
#include "media/filters/decoder_base.h"

namespace media {

class TestVideoDecoder : public DecoderBase<VideoDecoder, VideoFrame> {
 public:
  TestVideoDecoder()
      : DecoderBase<VideoDecoder, VideoFrame>(NULL),
        video_width_(0),
        video_height_(0) {
  }

  bool OnInitialize(DemuxerStream* demuxer_stream) {
    const MediaFormat& media_format = demuxer_stream->media_format();
    std::string mime_type;
    int width, height;
    if (media_format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
        mime_type.compare(mime_type::kH264AnnexB) == 0 &&
        media_format.GetAsInteger(MediaFormat::kWidth, &width) &&
        media_format.GetAsInteger(MediaFormat::kHeight, &height)) {
      video_width_ = width;
      video_height_ = height;
      media_format_.SetAsString(MediaFormat::kMimeType,
                                mime_type::kUncompressedVideo);
      media_format_.SetAsInteger(MediaFormat::kWidth, width);
      media_format_.SetAsInteger(MediaFormat::kHeight, height);
      return true;
    }
    return false;
  }

  void OnDecode(Buffer* buffer) {
    scoped_refptr<VideoFrame> frame;
    VideoFrameImpl::CreateFrame(VideoSurface::YV12,
                                video_width_,
                                video_height_,
                                buffer->GetTimestamp(),
                                buffer->GetDuration(),
                                &frame);
    if (frame) {
      MockVideoDecoder::InitializeYV12Frame(frame, 0.5f);
      EnqueueResult(frame);
    } else {
      host_->Error(PIPELINE_ERROR_OUT_OF_MEMORY);
    }
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format) {
    std::string mime_type;
    return (media_format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
            mime_type == mime_type::kH264AnnexB);
  }

 private:
  friend class scoped_refptr<TestVideoDecoder>;
  virtual ~TestVideoDecoder() {}

  size_t video_width_;
  size_t video_height_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoDecoder);
};

}  // namespace

#endif  // MEDIA_FILTERS_TEST_VIDEO_DECODER_H_
