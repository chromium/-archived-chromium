// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Demuxer interface using FFmpeg's libavformat.  At this time
// will support demuxing any audio/video format thrown at it.  The streams
// output mime types audio/x-ffmpeg and video/x-ffmpeg and include an integer
// key FFmpegCodecID which contains the CodecID enumeration value.  The CodecIDs
// can be used to create and initialize the corresponding FFmpeg decoder.
//
// FFmpegDemuxer sets the duration of pipeline during initialization by using
// the duration of the longest audio/video stream.
//
// NOTE: since FFmpegDemuxer reads packets sequentially without seeking, media
// files with very large drift between audio/video streams may result in
// excessive memory consumption.

#ifndef MEDIA_FILTERS_FFMPEG_DEMUXER_H_
#define MEDIA_FILTERS_FFMPEG_DEMUXER_H_

#include <deque>
#include <vector>

#include "base/lock.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"

// FFmpeg forward declarations.
struct AVCodecContext;
struct AVBitStreamFilterContext;
struct AVFormatContext;
struct AVPacket;
struct AVStream;
enum CodecID;

namespace media {

class FFmpegDemuxer;

class FFmpegDemuxerStream : public DemuxerStream {
 public:
  // Maintains a reference to |demuxer| and initializes itself using information
  // inside |stream|.
  FFmpegDemuxerStream(FFmpegDemuxer* demuxer, const AVStream& stream);

  virtual ~FFmpegDemuxerStream();

  // Returns true is this stream has pending reads, false otherwise.
  bool HasPendingReads();

  // Enqueues and takes ownership over the given AVPacket.
  void EnqueuePacket(AVPacket* packet);

  // Returns the duration of this stream.
  base::TimeDelta duration() { return duration_; }

  // DemuxerStream implementation.
  virtual const MediaFormat* GetMediaFormat();
  virtual void Read(Assignable<Buffer>* buffer);

 private:
  // Returns true if there are still pending reads.
  bool FulfillPendingReads();

  FFmpegDemuxer* demuxer_;
  MediaFormat media_format_;
  base::TimeDelta time_base_;
  base::TimeDelta duration_;
  Lock lock_;

  typedef std::deque< scoped_refptr<Buffer> > InputQueue;
  InputQueue input_queue_;

  typedef std::deque< scoped_refptr< Assignable<Buffer> > > OutputQueue;
  OutputQueue output_queue_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerStream);
};

class FFmpegDemuxer : public Demuxer {
 public:
  // FilterFactory provider.
  static FilterFactory* CreateFilterFactory() {
    return new FilterFactoryImpl0<FFmpegDemuxer>();
  }

  // Called by FFmpegDemuxerStreams to schedule a Demux() task.
  void ScheduleDemux();

  // MediaFilter implementation.
  virtual void Stop();

  // Demuxer implementation.
  virtual bool Initialize(DataSource* data_source);
  virtual size_t GetNumberOfStreams();
  virtual DemuxerStream* GetStream(int stream_id);

 private:
  // Only allow a factory to create this class.
  friend class FilterFactoryImpl0<FFmpegDemuxer>;
  FFmpegDemuxer();
  virtual ~FFmpegDemuxer();

  // Demuxing task scheduled by streams.
  void Demux();

  // Returns true if any of the streams have pending reads.
  bool StreamsHavePendingReads();

  // Flag to prevent multiple Demux() tasks from being scheduled.
  bool demuxing_;

  // FFmpeg context handle.
  AVFormatContext* format_context_;

  // Vector of streams.
  typedef std::vector<FFmpegDemuxerStream*> StreamVector;
  StreamVector streams_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DEMUXER_H_
