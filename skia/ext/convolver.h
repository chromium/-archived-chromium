// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CONVOLVER_H_
#define SKIA_EXT_CONVOLVER_H_

#include <vector>

// avoid confusion with Mac OS X's math library (Carbon)
#if defined(__APPLE__)
#undef FloatToFixed
#endif

namespace skia {

// Represents a filter in one dimension. Each output pixel has one entry in this
// object for the filter values contributing to it. You build up the filter
// list by calling AddFilter for each output pixel (in order).
//
// We do 2-dimensional convolusion by first convolving each row by one
// ConvolusionFilter1D, then convolving each column by another one.
//
// Entries are stored in fixed point, shifted left by kShiftBits.
class ConvolusionFilter1D {
 public:
  // The number of bits that fixed point values are shifted by.
  enum { kShiftBits = 14 };

  typedef short Fixed;

  ConvolusionFilter1D() : max_filter_(0) {
  }

  // Convert between floating point and our fixed point representation.
  static Fixed FloatToFixed(float f) {
    return static_cast<Fixed>(f * (1 << kShiftBits));
  }
  static unsigned char FixedToChar(Fixed x) {
    return static_cast<unsigned char>(x >> kShiftBits);
  }

  // Returns the maximum pixel span of a filter.
  int max_filter() const { return max_filter_; }

  // Returns the number of filters in this filter. This is the dimension of the
  // output image.
  int num_values() const { return static_cast<int>(filters_.size()); }

  // Appends the given list of scaling values for generating a given output
  // pixel. |filter_offset| is the distance from the edge of the image to where
  // the scaling factors start. The scaling factors apply to the source pixels
  // starting from this position, and going for the next |filter_length| pixels.
  //
  // You will probably want to make sure your input is normalized (that is,
  // all entries in |filter_values| sub to one) to prevent affecting the overall
  // brighness of the image.
  //
  // The filter_length must be > 0.
  //
  // This version will automatically convert your input to fixed point.
  void AddFilter(int filter_offset,
                 const float* filter_values,
                 int filter_length);

  // Same as the above version, but the input is already fixed point.
  void AddFilter(int filter_offset,
                 const Fixed* filter_values,
                 int filter_length);

  // Retrieves a filter for the given |value_offset|, a position in the output
  // image in the direction we're convolving. The offset and length of the
  // filter values are put into the corresponding out arguments (see AddFilter
  // above for what these mean), and a pointer to the first scaling factor is
  // returned. There will be |filter_length| values in this array.
  inline const Fixed* FilterForValue(int value_offset,
                                     int* filter_offset,
                                     int* filter_length) const {
    const FilterInstance& filter = filters_[value_offset];
    *filter_offset = filter.offset;
    *filter_length = filter.length;
    return &filter_values_[filter.data_location];
  }

 private:
  struct FilterInstance {
    // Offset within filter_values for this instance of the filter.
    int data_location;

    // Distance from the left of the filter to the center. IN PIXELS
    int offset;

    // Number of values in this filter instance.
    int length;
  };

  // Stores the information for each filter added to this class.
  std::vector<FilterInstance> filters_;

  // We store all the filter values in this flat list, indexed by
  // |FilterInstance.data_location| to avoid the mallocs required for storing
  // each one separately.
  std::vector<Fixed> filter_values_;

  // The maximum size of any filter we've added.
  int max_filter_;
};

// Does a two-dimensional convolusion on the given source image.
//
// It is assumed the source pixel offsets referenced in the input filters
// reference only valid pixels, so the source image size is not required. Each
// row of the source image starts |source_byte_row_stride| after the previous
// one (this allows you to have rows with some padding at the end).
//
// The result will be put into the given output buffer. The destination image
// size will be xfilter.num_values() * yfilter.num_values() pixels. It will be
// in rows of exactly xfilter.num_values() * 4 bytes.
//
// |source_has_alpha| is a hint that allows us to avoid doing computations on
// the alpha channel if the image is opaque. If you don't know, set this to
// true and it will work properly, but setting this to false will be a few
// percent faster if you know the image is opaque.
//
// The layout in memory is assumed to be 4-bytes per pixel in B-G-R-A order
// (this is ARGB when loaded into 32-bit words on a little-endian machine).
void BGRAConvolve2D(const unsigned char* source_data,
                    int source_byte_row_stride,
                    bool source_has_alpha,
                    const ConvolusionFilter1D& xfilter,
                    const ConvolusionFilter1D& yfilter,
                    unsigned char* output);

}  // namespace skia

#endif  // SKIA_EXT_CONVOLVER_H_

