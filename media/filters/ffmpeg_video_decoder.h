// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.


#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include <queue>

#include "media/base/factory.h"
#include "media/filters/decoder_base.h"

// FFmpeg types.
struct AVCodecContext;
struct AVFrame;

namespace media {

//------------------------------------------------------------------------------

class FFmpegVideoDecoder : public DecoderBase<VideoDecoder, VideoFrame> {
 public:
  static FilterFactory* CreateFactory() {
    return new FilterFactoryImpl0<FFmpegVideoDecoder>();
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  virtual bool OnInitialize(DemuxerStream* demuxer_stream);

  virtual void OnDecode(Buffer* buffer);

 private:
  friend class FilterFactoryImpl0<FFmpegVideoDecoder>;
  FFmpegVideoDecoder();
  virtual ~FFmpegVideoDecoder();

  bool EnqueueVideoFrame(VideoSurface::Format surface_format,
                         const AVFrame& frame);

  void CopyPlane(size_t plane, const VideoSurface& surface,
                 const AVFrame& frame);

  size_t width_;
  size_t height_;

  // FFmpeg outputs packets in decode timestamp (dts) order, which may not
  // always be in presentation timestamp (pts) order.  Therefore, when Process
  // is called we cannot assume that the pts of the input |buffer| passed to the
  // OnDecode() method is necessarily the pts of video frame.  For example:
  //
  // Process()    Timestamp     Timestamp
  //  Call #      Buffer In     Buffer Out
  //    1             1             1
  //    2             3            --- <--- frame 3 buffered by FFmpeg
  //    3             2             2
  //    4             4             3  <--- copying timestamp 4 and 6 would be
  //    5             6             4  <-'  incorrect, which is why we sort and
  //    6             5             5       queue incoming timestamps

  // A queue entry that holds a timestamp and a duration.
  struct TimeTuple {
    base::TimeDelta timestamp;
    base::TimeDelta duration;

    bool operator<(const TimeTuple& other) const {
      return timestamp >= other.timestamp;
    }
  };

  // A priority queue of presentation TimeTuples.
  typedef std::priority_queue<TimeTuple> TimeQueue;
  TimeQueue time_queue_;

  AVCodecContext* codec_context_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
