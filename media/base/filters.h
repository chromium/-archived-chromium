// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Filters are connected in a strongly typed manner, with downstream filters
// always reading data from upstream filters.  Upstream filters have no clue
// who is actually reading from them, and return the results via OnAssignment
// using the AssignableInterface<SomeBufferType> interface:
//
//                         DemuxerStream(Video) <- VideoDecoder <- VideoRenderer
// DataSource <- Demuxer <
//                         DemuxerStream(Audio) <- AudioDecoder <- AudioRenderer
//
// Upstream -------------------------------------------------------> Downstream
//                         <- Reads flow this way
//                    Buffer assignments flow this way ->
//
// Every filter maintains a reference to the scheduler, who maintains data
// shared between filters (i.e., reference clock value, playback state).  The
// scheduler is also responsible for scheduling filter tasks (i.e., a read on
// a VideoDecoder would result in scheduling a Decode task).  Filters can also
// use the scheduler to signal errors and shutdown playback.

#ifndef MEDIA_BASE_FILTERS_H_
#define MEDIA_BASE_FILTERS_H_

#include <limits>
#include <string>
#include "base/ref_counted.h"

namespace media {

template <class BufferType> class AssignableInterface;
class BufferInterface;
class DecoderInterface;
class DemuxerStreamInterface;
class FilterHostInterface;
class MediaFormat;
class VideoFrameInterface;
class WritableBufferInterface;

// Identifies the type of filter implementation.  Used in conjunction with some
// template wizardry to enforce strongly typed operations.  More or less a
// knock off of MSVC's __uuidof() operator.
enum FilterType {
  FILTER_DATA_SOURCE,
  FILTER_DEMUXER,
  FILTER_AUDIO_DECODER,
  FILTER_VIDEO_DECODER,
  FILTER_AUDIO_RENDERER,
  FILTER_VIDEO_RENDERER,
  FILTER_MAX
};


class MediaFilterInterface :
    public base::RefCountedThreadSafe<MediaFilterInterface> {
 public:
  virtual void SetFilterHost(FilterHostInterface* filter_host) = 0;

 protected:
  friend class base::RefCountedThreadSafe<MediaFilterInterface>;
  virtual ~MediaFilterInterface() {}
};


class DataSourceInterface : public MediaFilterInterface {
 public:
  static const FilterType kFilterType = FILTER_DATA_SOURCE;
  static const size_t kReadError = static_cast<size_t>(-1);

  // Initializes this filter, returns true if successful, false otherwise.
  virtual bool Initialize(const std::string& uri) = 0;

  // Returns the MediaFormat for this filter.
  virtual const MediaFormat* GetMediaFormat() = 0;

  // Read the given amount of bytes into data, returns the number of bytes read
  // if successful, kReadError otherwise.
  virtual size_t Read(char* data, size_t size) = 0;

  // Returns true and the current file position for this file, false if the
  // file position could not be retrieved.
  virtual bool GetPosition(int64* position_out) = 0;

  // Returns true if the file position could be set, false otherwise.
  virtual bool SetPosition(int64 position) = 0;

  // Returns true and the file size, false if the file size could not be
  // retrieved.
  virtual bool GetSize(int64* size_out) = 0;
};


class DemuxerInterface : public MediaFilterInterface {
 public:
  static const FilterType kFilterType = FILTER_DEMUXER;

  // Initializes this filter, returns true if successful, false otherwise. 
  virtual bool Initialize(DataSourceInterface* data_source) = 0;

  // Returns the number of streams available
  virtual size_t GetNumberOfStreams() = 0;

  // Returns the stream for the given index, NULL otherwise
  virtual DemuxerStreamInterface* GetStream(int stream_id) = 0;
};


class DemuxerStreamInterface {
 public:
  // Returns the MediaFormat for this filter.
  virtual const MediaFormat* GetMediaFormat() = 0;

  // Schedules a read and takes ownership of the given buffer.
  virtual void Read(AssignableInterface<BufferInterface>* buffer) = 0;
};


class VideoDecoderInterface : public MediaFilterInterface {
 public:
  static const FilterType kFilterType = FILTER_VIDEO_DECODER;

  // Initializes this filter, returns true if successful, false otherwise.
  virtual bool Initialize(DemuxerStreamInterface* demuxer_stream) = 0;

  // Returns the MediaFormat for this filter.
  virtual const MediaFormat* GetMediaFormat() = 0;

  // Schedules a read and takes ownership of the given buffer.
  virtual void Read(AssignableInterface<VideoFrameInterface>* video_frame) = 0;
};


class AudioDecoderInterface : public MediaFilterInterface {
 public:
  static const FilterType kFilterType = FILTER_AUDIO_DECODER;

  // Initializes this filter, returns true if successful, false otherwise.
  virtual bool Initialize(DemuxerStreamInterface* demuxer_stream) = 0;

  // Returns the MediaFormat for this filter.
  virtual const MediaFormat* GetMediaFormat() = 0;

  // Schedules a read and takes ownership of the given buffer.
  virtual void Read(AssignableInterface<BufferInterface>* buffer) = 0;
};


class VideoRendererInterface : public MediaFilterInterface {
 public:
  static const FilterType kFilterType = FILTER_VIDEO_RENDERER;

  // Initializes this filter, returns true if successful, false otherwise.
  virtual bool Initialize(VideoDecoderInterface* decoder) = 0;
};


class AudioRendererInterface : public MediaFilterInterface {
 public:
  static const FilterType kFilterType = FILTER_AUDIO_RENDERER;

  // Initializes this filter, returns true if successful, false otherwise.
  virtual bool Initialize(AudioDecoderInterface* decoder) = 0;
};

} // namespace media

#endif // MEDIA_BASE_FILTERS_H_
