// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains macros to simplify histogram reporting from the disk
// cache. The main issue is that we want to have separate histograms for each
// type of cache (regular vs. media, etc), without adding the complexity of
// keeping track of a potentially large number of histogram objects that have to
// survive the backend object that created them.

#ifndef NET_DISK_CACHE_HISTOGRAM_MACROS_H_
#define NET_DISK_CACHE_HISTOGRAM_MACROS_H_

// HISTOGRAM_HOURS will collect time related data with a granularity of hours
// and normal values of a few months.
#define UMA_HISTOGRAM_HOURS UMA_HISTOGRAM_COUNTS_10000

// HISTOGRAM_AGE will collect time elapsed since |initial_time|, with a
// granularity of hours and normal values of a few months.
#define UMA_HISTOGRAM_AGE(name, initial_time)\
    UMA_HISTOGRAM_COUNTS_10000(name, (Time::Now() - initial_time).InHours())

// HISTOGRAM_AGE_MS will collect time elapsed since |initial_time|, with the
// normal resolution of the UMA_HISTOGRAM_TIMES.
#define UMA_HISTOGRAM_AGE_MS(name, initial_time)\
    UMA_HISTOGRAM_TIMES(name, Time::Now() - initial_time)

#define UMA_HISTOGRAM_CACHE_ERROR(name, sample) do { \
    static LinearHistogram counter((name), 0, 49, 50); \
    counter.SetFlags(kUmaTargetedHistogramFlag); \
    counter.Add(sample); \
  } while (0)

#ifdef NET_DISK_CACHE_BACKEND_IMPL_CC_
#define BACKEND_OBJ this
#else
#define BACKEND_OBJ backend_
#endif

// Generates a UMA histogram of the given type, generating the proper name for
// it (asking backend_->HistogramName), and adding the provided sample.
// For example, to generate a regualar UMA_HISTOGRAM_COUNTS, this macro would
// be used as:
//  CACHE_UMA(COUNTS, "MyName", 0, 20);
//  CACHE_UMA(COUNTS, "MyExperiment", 530, 55);
// which roughly translates to:
//  UMA_HISTOGRAM_COUNTS("DiskCache.2.MyName", 20);  // "2" is the CacheType.
//  UMA_HISTOGRAM_COUNTS("DiskCache.2.MyExperiment_530", 55);
//
#define CACHE_UMA(type, name, experiment, sample) {\
    const std::string my_name = BACKEND_OBJ->HistogramName(name, experiment);\
    switch (BACKEND_OBJ->cache_type()) {\
      case net::DISK_CACHE:\
        UMA_HISTOGRAM_##type(my_name.data(), sample);\
        break;\
      case net::MEDIA_CACHE:\
        UMA_HISTOGRAM_##type(my_name.data(), sample);\
        break;\
      default:\
        NOTREACHED();\
        break;\
    }\
  }

#endif  // NET_DISK_CACHE_HISTOGRAM_MACROS_H_
