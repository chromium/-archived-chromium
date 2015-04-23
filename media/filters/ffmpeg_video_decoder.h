// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include <queue>

#include "media/base/factory.h"
#include "media/filters/decoder_base.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

// FFmpeg types.
struct AVCodecContext;
struct AVFrame;
struct AVRational;

namespace media {

class FFmpegVideoDecoder : public DecoderBase<VideoDecoder, VideoFrame> {
 public:
  static FilterFactory* CreateFactory() {
    return new FilterFactoryImpl0<FFmpegVideoDecoder>();
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  virtual bool OnInitialize(DemuxerStream* demuxer_stream);

  virtual void OnSeek(base::TimeDelta time);

  virtual void OnDecode(Buffer* buffer);

 private:
  friend class FilterFactoryImpl0<FFmpegVideoDecoder>;
  friend class DecoderPrivateMock;
  friend class FFmpegVideoDecoderTest;
  FRIEND_TEST(FFmpegVideoDecoderTest, DecodeFrame_0ByteFrame);
  FRIEND_TEST(FFmpegVideoDecoderTest, DecodeFrame_DecodeError);
  FRIEND_TEST(FFmpegVideoDecoderTest, DecodeFrame_DiscontinuousBuffer);
  FRIEND_TEST(FFmpegVideoDecoderTest, DecodeFrame_Normal);
  FRIEND_TEST(FFmpegVideoDecoderTest, FindPtsAndDuration);
  FRIEND_TEST(FFmpegVideoDecoderTest, GetSurfaceFormat);
  FRIEND_TEST(FFmpegVideoDecoderTest, OnDecode_EnqueueVideoFrameError);
  FRIEND_TEST(FFmpegVideoDecoderTest, OnDecode_FinishEnqueuesEmptyFrames);
  FRIEND_TEST(FFmpegVideoDecoderTest, OnDecode_TestStateTransition);
  FRIEND_TEST(FFmpegVideoDecoderTest, TimeQueue_Ordering);

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
  //
  // The TimeQueue is used to reorder these types.
  struct PtsHeapOrdering {
    bool operator()(const base::TimeDelta& lhs,
                    const base::TimeDelta& rhs) const;
  };
  typedef std::priority_queue<base::TimeDelta,
                              std::vector<base::TimeDelta>,
                              PtsHeapOrdering> TimeQueue;

  // The TimeTuple struct is used to hold the needed timestamp data needed for
  // enqueuing a video frame.
  struct TimeTuple {
    base::TimeDelta timestamp;
    base::TimeDelta duration;
  };

  FFmpegVideoDecoder();
  virtual ~FFmpegVideoDecoder();

  virtual bool EnqueueVideoFrame(VideoSurface::Format surface_format,
                                 const TimeTuple& time,
                                 const AVFrame* frame);

  // Create an empty video frame and queue it.
  virtual void EnqueueEmptyFrame();

  virtual void CopyPlane(size_t plane, const VideoSurface& surface,
                         const AVFrame* frame);

  // Converts a AVCodecContext |pix_fmt| to a VideoSurface::Format.
  virtual VideoSurface::Format GetSurfaceFormat(
      const AVCodecContext& codec_context);

  // Decodes one frame of video with the given buffer.  Returns false if there
  // was a decode error, or a zero-byte frame was produced.
  virtual bool DecodeFrame(const Buffer& buffer, AVCodecContext* codec_context,
                           AVFrame* yuv_frame);

  // Attempt to get the PTS and Duration for this frame by examining the time
  // info provided via packet stream (stored in |pts_queue|), or the info
  // writen into the AVFrame itself.  If no data is available in either, then
  // attempt to generate a best guess of the pts based on the last known pts.
  //
  // Data inside the AVFrame (if provided) is trusted the most, followed
  // by data from the packet stream.  Estimation based on the |last_pts| is
  // reserved as a last-ditch effort.
  virtual TimeTuple FindPtsAndDuration(const AVRational& time_base,
                                       const TimeQueue& pts_queue,
                                       const TimeTuple& last_pts,
                                       const AVFrame* frame);

  // Signals the pipeline that a decode error occurs, and moves the decoder
  // into the kDecodeFinished state.
  virtual void SignalPipelineError();

  size_t width_;
  size_t height_;

  // A priority queue of presentation TimeTuples.
  TimeQueue pts_queue_;
  TimeTuple last_pts_;
  scoped_ptr<AVRational> time_base_;  // Pointer to avoid needing full type.

  enum DecoderState {
    kNormal,
    kFlushCodec,
    kDecodeFinished,
  };

  DecoderState state_;

  AVCodecContext* codec_context_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
