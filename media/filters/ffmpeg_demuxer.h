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
#include "base/thread.h"
#include "base/waitable_event.h"
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
  FFmpegDemuxerStream(FFmpegDemuxer* demuxer, AVStream* stream);

  virtual ~FFmpegDemuxerStream();

  // Returns true is this stream has pending reads, false otherwise.
  //
  // Safe to call on any thread.
  bool HasPendingReads();

  // Enqueues and takes ownership over the given AVPacket.
  void EnqueuePacket(AVPacket* packet);

  // Returns the duration of this stream.
  base::TimeDelta duration() { return duration_; }

  // DemuxerStream implementation.
  virtual const MediaFormat& media_format();
  virtual void Read(Callback1<Buffer*>::Type* read_callback);

  AVStream* av_stream() {
    return av_stream_;
  }

  static const char* interface_id();

 protected:
  virtual void* QueryInterface(const char* interface_id);

 private:
  // Returns true if there are still pending reads.
  bool FulfillPendingReads();

  FFmpegDemuxer* demuxer_;
  AVStream* av_stream_;
  MediaFormat media_format_;
  base::TimeDelta time_base_;
  base::TimeDelta duration_;
  Lock lock_;

  typedef std::deque< scoped_refptr<Buffer> > BufferQueue;
  BufferQueue buffer_queue_;

  typedef std::deque<Callback1<Buffer*>::Type*> ReadQueue;
  ReadQueue read_queue_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerStream);
};

class FFmpegDemuxer : public Demuxer, public PlatformThread::Delegate {
 public:
  // FilterFactory provider.
  static FilterFactory* CreateFilterFactory() {
    return new FilterFactoryImpl0<FFmpegDemuxer>();
  }

  // Called by FFmpegDemuxerStreams to signal the demux event.
  void SignalDemux();

  // MediaFilter implementation.
  virtual void Stop();

  // Demuxer implementation.
  virtual bool Initialize(DataSource* data_source);
  virtual size_t GetNumberOfStreams();
  virtual scoped_refptr<DemuxerStream> GetStream(int stream_id);

  // PlatformThread::Delegate implementation.
  virtual void ThreadMain();

 private:
  // Only allow a factory to create this class.
  friend class FilterFactoryImpl0<FFmpegDemuxer>;
  FFmpegDemuxer();
  virtual ~FFmpegDemuxer();

  // Returns true if any of the streams have pending reads.
  //
  // Safe to call on any thread.
  bool StreamsHavePendingReads();

  // FFmpeg context handle.
  AVFormatContext* format_context_;

  // Vector of streams.
  typedef std::vector< scoped_refptr<FFmpegDemuxerStream> > StreamVector;
  StreamVector streams_;

  // Thread handle.
  PlatformThreadHandle thread_;

  // Event to signal demux.
  base::WaitableEvent wait_for_demux_;

  // Used to signal |thread_| to terminate.
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DEMUXER_H_
