// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FFmpegGlue is an adapter for FFmpeg's URLProtocol interface that allows us to
// use a DataSource implementation with FFmpeg. For convenience we use FFmpeg's
// av_open_input_file function, which analyzes the filename given to it and
// automatically initializes the appropriate URLProtocol.
//
// Since the DataSource is already open by time we call av_open_input_file, we
// need a way for av_open_input_file to find the correct DataSource instance.
// The solution is to maintain a map of "filenames" to DataSource instances,
// where filenames are actually just a unique identifier.  For simplicity,
// FFmpegGlue is registered as an HTTP handler and generates filenames based on
// the memory address of the DataSource, i.e., http://0xc0bf4870.  Since there
// may be multiple FFmpegDemuxers active at one time, FFmpegGlue is a
// thread-safe singleton.
//
// Usage: FFmpegDemuxer adds the DataSource to FFmpegGlue's map and is given a
// filename to pass to av_open_input_file.  FFmpegDemuxer calls
// av_open_input_file with the filename, which results in FFmpegGlue returning
// the DataSource as a URLProtocol instance to FFmpeg.  Since FFmpegGlue is only
// needed for opening files, when av_open_input_file returns FFmpegDemuxer
// removes the DataSource from FFmpegGlue's map.

#ifndef MEDIA_FILTERS_FFMPEG_GLUE_H_
#define MEDIA_FILTERS_FFMPEG_GLUE_H_

#include <map>
#include <string>

#include "base/lock.h"
#include "base/singleton.h"

// FFmpeg forward declarations.
struct URLContext;
typedef int64 offset_t;

namespace media {

class DataSource;

class FFmpegGlue : public Singleton<FFmpegGlue> {
 public:
  // Adds a DataSource to the FFmpeg glue layer and returns a unique string that
  // can be passed to FFmpeg to identify the data source.
  std::string AddDataSource(DataSource* data_source);

  // Removes a DataSource from the FFmpeg glue layer.  Using strings from
  // previously added DataSources will no longer work.
  void RemoveDataSource(DataSource* data_source);

  // Assigns the DataSource identified with by the given key to |data_source|,
  // or assigns NULL if no such DataSource could be found.
  void GetDataSource(const std::string& key,
                     scoped_refptr<DataSource>* data_source);

 private:
  // Only allow Singleton to create and delete FFmpegGlue.
  friend struct DefaultSingletonTraits<FFmpegGlue>;
  FFmpegGlue();
  virtual ~FFmpegGlue();

  // Returns the unique key for this data source, which can be passed to
  // av_open_input_file as the filename.
  std::string GetDataSourceKey(DataSource* data_source);

  // Mutual exclusion while adding/removing items from the map.
  Lock lock_;

  // Map between keys and DataSource references.
  typedef std::map< std::string, scoped_refptr<DataSource> > DataSourceMap;
  DataSourceMap data_sources_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegGlue);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_GLUE_H_
