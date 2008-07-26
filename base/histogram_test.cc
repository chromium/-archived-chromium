// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Test of Histogram class

#include "base/histogram.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HistogramTest : public testing::Test {
};

// Check for basic syntax and use.
TEST(HistogramTest, StartupShutdownTest) {
  // Try basic construction
  Histogram histogram(L"TestHistogram", 1, 1000, 10);
  Histogram histogram1(L"Test1Histogram", 1, 1000, 10);

  LinearHistogram linear_histogram(L"TestLinearHistogram", 1, 1000, 10);
  LinearHistogram linear_histogram1(L"Test1LinearHistogram", 1, 1000, 10);

  // Use standard macros (but with fixed samples)
  HISTOGRAM_TIMES(L"Test2Histogram", TimeDelta::FromDays(1));
  HISTOGRAM_COUNTS(L"Test3Histogram", 30);

  DHISTOGRAM_TIMES(L"Test4Histogram", TimeDelta::FromDays(1));
  DHISTOGRAM_COUNTS(L"Test5Histogram", 30);

  ASSET_HISTOGRAM_COUNTS(L"Test6Histogram", 129);

  // Try to construct samples.
  Histogram::SampleSet sample1;
  Histogram::SampleSet sample2;

  // Use copy constructor of SampleSet
  sample1 = sample2;
  Histogram::SampleSet sample3(sample1);

  // Finally test a statistics recorder, without really using it.
  StatisticsRecorder recorder;
}

// Repeat with a recorder present to register with.
TEST(HistogramTest, RecordedStartupTest) {
  // Test a statistics recorder, by letting histograms register.
  StatisticsRecorder recorder;  // This initializes the global state.

  StatisticsRecorder::Histograms histograms;
  EXPECT_EQ(0, histograms.size());
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(0, histograms.size());

  // Try basic construction
  Histogram histogram(L"TestHistogram", 1, 1000, 10);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(1, histograms.size());
  Histogram histogram1(L"Test1Histogram", 1, 1000, 10);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(2, histograms.size());

  LinearHistogram linear_histogram(L"TestLinearHistogram", 1, 1000, 10);
  LinearHistogram linear_histogram1(L"Test1LinearHistogram", 1, 1000, 10);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(4, histograms.size());

  // Use standard macros (but with fixed samples)
  HISTOGRAM_TIMES(L"Test2Histogram", TimeDelta::FromDays(1));
  HISTOGRAM_COUNTS(L"Test3Histogram", 30);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(6, histograms.size());

  ASSET_HISTOGRAM_COUNTS(L"TestAssetHistogram", 1000);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
  EXPECT_EQ(7, histograms.size());

  DHISTOGRAM_TIMES(L"Test4Histogram", TimeDelta::FromDays(1));
  DHISTOGRAM_COUNTS(L"Test5Histogram", 30);
  histograms.clear();
  StatisticsRecorder::GetHistograms(&histograms);  // Load up lists
#ifndef NDEBUG
  EXPECT_EQ(9, histograms.size());
#else
  EXPECT_EQ(7, histograms.size());
#endif
}

TEST(HistogramTest, RangeTest) {
  StatisticsRecorder recorder;
  StatisticsRecorder::Histograms histograms;

  recorder.GetHistograms(&histograms);
  EXPECT_EQ(0, histograms.size());

  Histogram histogram(L"Histogram", 1, 64, 8);  // As mentioned in header file.
  // Check that we got a nice exponential when there was enough rooom.
  EXPECT_EQ(0, histogram.ranges(0));
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    EXPECT_EQ(power_of_2, histogram.ranges(i));
    power_of_2 *= 2;
  }
  EXPECT_EQ(INT_MAX, histogram.ranges(8));

  Histogram short_histogram(L"Histogram Shortened", 1, 7, 8);
  // Check that when the number of buckets is short, we get a linear histogram
  // for lack of space to do otherwise.
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i, short_histogram.ranges(i));
  EXPECT_EQ(INT_MAX, short_histogram.ranges(8));

  LinearHistogram linear_histogram(L"Linear", 1, 7, 8);
  // We also get a nice linear set of bucket ranges when we ask for it
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i, linear_histogram.ranges(i));
  EXPECT_EQ(INT_MAX, linear_histogram.ranges(8));

  LinearHistogram linear_broad_histogram(L"Linear widened", 2, 14, 8);
  // ...but when the list has more space, then the ranges naturally spread out.
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(2 * i, linear_broad_histogram.ranges(i));
  EXPECT_EQ(INT_MAX, linear_broad_histogram.ranges(8));

  ThreadSafeHistogram threadsafe_histogram(L"ThreadSafe", 1, 32, 15);
  // When space is a little tight, we transition from linear to exponential.
  // This is what happens in both the basic histogram, and the threadsafe
  // variant (which is derived).
  EXPECT_EQ(0, threadsafe_histogram.ranges(0));
  EXPECT_EQ(1, threadsafe_histogram.ranges(1));
  EXPECT_EQ(2, threadsafe_histogram.ranges(2));
  EXPECT_EQ(3, threadsafe_histogram.ranges(3));
  EXPECT_EQ(4, threadsafe_histogram.ranges(4));
  EXPECT_EQ(5, threadsafe_histogram.ranges(5));
  EXPECT_EQ(6, threadsafe_histogram.ranges(6));
  EXPECT_EQ(7, threadsafe_histogram.ranges(7));
  EXPECT_EQ(9, threadsafe_histogram.ranges(8));
  EXPECT_EQ(11, threadsafe_histogram.ranges(9));
  EXPECT_EQ(14, threadsafe_histogram.ranges(10));
  EXPECT_EQ(17, threadsafe_histogram.ranges(11));
  EXPECT_EQ(21, threadsafe_histogram.ranges(12));
  EXPECT_EQ(26, threadsafe_histogram.ranges(13));
  EXPECT_EQ(32, threadsafe_histogram.ranges(14));
  EXPECT_EQ(INT_MAX, threadsafe_histogram.ranges(15));

  recorder.GetHistograms(&histograms);
  EXPECT_EQ(5, histograms.size());
}

// Make sure histogram handles out-of-bounds data gracefully.
TEST(HistogramTest, BoundsTest) {
  const int kBucketCount = 50;
  Histogram histogram(L"Bounded", 10, 100, kBucketCount);

  // Put two samples "out of bounds" above and below.
  histogram.Add(5);
  histogram.Add(-50);

  histogram.Add(100);
  histogram.Add(10000);

  // Verify they landed in the underflow, and overflow buckets.
  Histogram::SampleSet sample;
  histogram.SnapshotSample(&sample);
  EXPECT_EQ(2, sample.counts(0));
  EXPECT_EQ(0, sample.counts(1));
  size_t array_size = histogram.bucket_count();
  EXPECT_EQ(kBucketCount, array_size);
  EXPECT_EQ(0, sample.counts(array_size - 2));
  EXPECT_EQ(2, sample.counts(array_size - 1));
}

// Check to be sure samples land as expected is "correct" buckets.
TEST(HistogramTest, BucketPlacementTest) {
  Histogram histogram(L"Histogram", 1, 64, 8);  // As mentioned in header file.

  // Check that we got a nice exponential since there was enough rooom.
  EXPECT_EQ(0, histogram.ranges(0));
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    EXPECT_EQ(power_of_2, histogram.ranges(i));
    power_of_2 *= 2;
  }
  EXPECT_EQ(INT_MAX, histogram.ranges(8));

  // Add i+1 samples to the i'th bucket.
  histogram.Add(0);
  power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    for (int j = 0; j <= i; j++)
      histogram.Add(power_of_2);
    power_of_2 *= 2;
  }
  // Leave overflow bucket empty.

  // Check to see that the bucket counts reflect our additions.
  Histogram::SampleSet sample;
  histogram.SnapshotSample(&sample);
  EXPECT_EQ(INT_MAX, histogram.ranges(8));
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i + 1, sample.counts(i));
}

static const wchar_t* kAssetTestHistogramName = L"AssetCountTest";
static const wchar_t* kAssetTestDebugHistogramName = L"DAssetCountTest";
void AssetCountFunction(int sample) {
  ASSET_HISTOGRAM_COUNTS(kAssetTestHistogramName, sample);
  DASSET_HISTOGRAM_COUNTS(kAssetTestDebugHistogramName, sample);
}
// Check that asset can be added and removed from buckets.
TEST(HistogramTest, AssetCountTest) {
  // Start up a recorder system to identify all histograms.
  StatisticsRecorder recorder;

  // Call through the macro to instantiate the static variables.
  AssetCountFunction(100);  // Put a sample in the bucket for 100.

  // Find the histogram.
  StatisticsRecorder::Histograms histogram_list;
  StatisticsRecorder::GetHistograms(&histogram_list);
  ASSERT_GT(histogram_list.size(), 0u);
  std::string ascii_name = WideToASCII(kAssetTestHistogramName);
  std::string debug_ascii_name = WideToASCII(kAssetTestDebugHistogramName);
  const Histogram* our_histogram = NULL;
  const Histogram* our_debug_histogram = NULL;
  for (StatisticsRecorder::Histograms::iterator it = histogram_list.begin();
       it != histogram_list.end();
       ++it) {
    if (!(*it)->histogram_name().compare(ascii_name))
      our_histogram = *it;
    else if (!(*it)->histogram_name().compare(debug_ascii_name)) {
      our_debug_histogram = *it;
    }
  }
  ASSERT_TRUE(our_histogram);
#ifndef NDEBUG
  EXPECT_TRUE(our_debug_histogram);
#else
  EXPECT_FALSE(our_debug_histogram);
#endif
  // Verify it has a 1 in exactly one bucket (where we put the sample).
  Histogram::SampleSet sample;
  our_histogram->SnapshotSample(&sample);
  int match_count = 0;
  for (size_t i = 0; i < our_histogram->bucket_count(); ++i) {
    if (sample.counts(i) > 0) {
      EXPECT_LT(++match_count, 2) << "extra count in bucket " << i;
    }
  }
  EXPECT_EQ(1u, match_count);

  // Remove our sample.
  AssetCountFunction(-100);  // Remove a sample from the bucket for 100.
  our_histogram->SnapshotSample(&sample);  // Extract data set.

  // Verify that the bucket is now empty, as are all the other buckets.
  for (size_t i = 0; i < our_histogram->bucket_count(); ++i) {
    EXPECT_EQ(0, sample.counts(i)) << "extra count in bucket " << i;
  }

  if (!our_debug_histogram)
    return;  // This is a production build.

  // Repeat test with debug histogram.  Note that insertion and deletion above
  // should have cancelled each other out.
  AssetCountFunction(100);  // Add a sample into the bucket for 100.
  our_debug_histogram->SnapshotSample(&sample);
  match_count = 0;
  for (size_t i = 0; i < our_debug_histogram->bucket_count(); ++i) {
    if (sample.counts(i) > 0) {
      EXPECT_LT(++match_count, 2) << "extra count in bucket " << i;
    }
  }
  EXPECT_EQ(1u, match_count);

  // Remove our sample.
  AssetCountFunction(-100);  // Remove a sample from the bucket for 100.
  our_debug_histogram->SnapshotSample(&sample);  // Extract data set.

  // Verify that the bucket is now empty, as are all the other buckets.
  for (size_t i = 0; i < our_debug_histogram->bucket_count(); ++i) {
    EXPECT_EQ(0, sample.counts(i)) << "extra count in bucket " << i;
  }
}

}  // namespace
