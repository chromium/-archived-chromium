// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_HISTOGRAM_SNAPSHOTS_H_
#define CHROME_RENDERER_HISTOGRAM_SNAPSHOTS_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/histogram.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/task.h"

class RendererHistogramSnapshots {
 public:
  RendererHistogramSnapshots();

  ~RendererHistogramSnapshots() {}

  // Send the histogram data.
  void SendHistograms(int sequence_number);

  // Maintain a map of histogram names to the sample stats we've sent.
  typedef std::map<std::string, Histogram::SampleSet> LoggedSampleMap;
  typedef std::vector<std::string> HistogramPickledList;

 private:
  // Extract snapshot data and then send it off the the Browser process.
  // Send only a delta to what we have already sent.
  void UploadAllHistrograms(int sequence_number);
  void UploadHistrogram(const Histogram& histogram,
                        HistogramPickledList* histograms);
  void UploadHistogramDelta(const Histogram& histogram,
                            const Histogram::SampleSet& snapshot,
                            HistogramPickledList* histograms);

  ScopedRunnableMethodFactory<RendererHistogramSnapshots>
      renderer_histogram_snapshots_factory_;

  // For histograms, record what we've already logged (as a sample for each
  // histogram) so that we can send only the delta with the next log.
  LoggedSampleMap logged_samples_;

  DISALLOW_COPY_AND_ASSIGN(RendererHistogramSnapshots);
};

#endif  // CHROME_RENDERER_HISTOGRAM_SNAPSHOTS_H_
