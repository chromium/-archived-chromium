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
//
// When stopped, FFmpegDemuxer and FFmpegDemuxerStream release all callbacks
// and buffered packets.  Reads from a stopped FFmpegDemuxerStream will not be
// replied to.

#ifndef MEDIA_FILTERS_FFMPEG_DEMUXER_H_
#define MEDIA_FILTERS_FFMPEG_DEMUXER_H_

#include <deque>
#include <vector>

#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/filters/ffmpeg_interfaces.h"

// FFmpeg forward declarations.
struct AVCodecContext;
struct AVBitStreamFilterContext;
struct AVFormatContext;
struct AVPacket;
struct AVStream;

namespace media {

class FFmpegDemuxer;

// Forward declaration for scoped_ptr_malloc.
class ScopedPtrAVFree;

class FFmpegDemuxerStream : public DemuxerStream, public AVStreamProvider {
 public:
  // Maintains a reference to |demuxer| and initializes itself using information
  // inside |stream|.
  FFmpegDemuxerStream(FFmpegDemuxer* demuxer, AVStream* stream);

  virtual ~FFmpegDemuxerStream();

  // Returns true is this stream has pending reads, false otherwise.
  //
  // Safe to call on any thread.
  virtual bool HasPendingReads();

  // Enqueues and takes ownership over the given AVPacket, returns the timestamp
  // of the enqueued packet.
  virtual base::TimeDelta EnqueuePacket(AVPacket* packet);

  // Signals to empty the buffer queue and mark next packet as discontinuous.
  virtual void FlushBuffers();

  // Empties the queues and ignores any additional calls to Read().
  virtual void Stop();

  // Returns the duration of this stream.
  virtual base::TimeDelta duration() { return duration_; }

  // DemuxerStream implementation.
  virtual const MediaFormat& media_format();
  virtual void Read(Callback1<Buffer*>::Type* read_callback);

  // AVStreamProvider implementation.
  virtual AVStream* GetAVStream() { return stream_; }

 protected:
  virtual void* QueryInterface(const char* interface_id);

 private:
  // Carries out enqueuing a pending read on the demuxer thread.
  void ReadTask(Callback1<Buffer*>::Type* read_callback);

  // Attempts to fulfill a single pending read by dequeueing a buffer and read
  // callback pair and executing the callback.
  void FulfillPendingRead();

  // Converts an FFmpeg stream timestamp into a base::TimeDelta.
  base::TimeDelta ConvertTimestamp(int64 timestamp);

  FFmpegDemuxer* demuxer_;
  AVStream* stream_;
  MediaFormat media_format_;
  base::TimeDelta duration_;
  bool discontinuous_;
  bool stopped_;

  typedef std::deque<scoped_refptr<Buffer> > BufferQueue;
  BufferQueue buffer_queue_;

  typedef std::deque<Callback1<Buffer*>::Type*> ReadQueue;
  ReadQueue read_queue_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerStream);
};

class FFmpegDemuxer : public Demuxer {
 public:
  // FilterFactory provider.
  static FilterFactory* CreateFilterFactory() {
    return new FilterFactoryImpl0<FFmpegDemuxer>();
  }

  // Posts a task to perform additional demuxing.
  virtual void PostDemuxTask();

  // MediaFilter implementation.
  virtual void Stop();
  virtual void Seek(base::TimeDelta time);

  // Demuxer implementation.
  virtual bool Initialize(DataSource* data_source);
  virtual size_t GetNumberOfStreams();
  virtual scoped_refptr<DemuxerStream> GetStream(int stream_id);

 private:
  // Allow FFmpegDemuxerStream to post tasks to our message loop.
  friend class FFmpegDemuxerStream;

  // Only allow a factory to create this class.
  friend class FilterFactoryImpl0<FFmpegDemuxer>;
  FFmpegDemuxer();
  virtual ~FFmpegDemuxer();

  // Carries out initialization on the demuxer thread.
  void InititalizeTask(DataSource* data_source);

  // Carries out a seek on the demuxer thread.
  void SeekTask(base::TimeDelta time);

  // Carries out demuxing and satisfying stream reads on the demuxer thread.
  void DemuxTask();

  // Carries out stopping the demuxer streams on the demuxer thread.
  void StopTask();

  // Returns true if any of the streams have pending reads.  Since we lazily
  // post a DemuxTask() for every read, we use this method to quickly terminate
  // the tasks if there is no work to do.
  //
  // Must be called on the demuxer thread.
  bool StreamsHavePendingReads();

  // Signal all FFmpegDemuxerStream that the stream has ended.
  //
  // Must be called on the demuxer thread.
  void StreamHasEnded();

  // FFmpeg context handle.
  AVFormatContext* format_context_;

  // Latest timestamp read on the demuxer thread.
  base::TimeDelta current_timestamp_;

  // Two vector of streams:
  //   - |streams_| is indexed for the Demuxer interface GetStream(), which only
  //     contains supported streams and no NULL entries.
  //   - |packet_streams_| is indexed to mirror AVFormatContext when dealing
  //     with AVPackets returned from av_read_frame() and contain NULL entries
  //     representing unsupported streams where we throw away the data.
  //
  // Ownership is handled via reference counting.
  //
  // Once initialized, operations on FFmpegDemuxerStreams should be carried out
  // on the demuxer thread.
  typedef std::vector< scoped_refptr<FFmpegDemuxerStream> > StreamVector;
  StreamVector streams_;
  StreamVector packet_streams_;

  // Used for debugging.
  PlatformThreadId thread_id_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DEMUXER_H_
