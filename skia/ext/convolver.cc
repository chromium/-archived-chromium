// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "skia/ext/convolver.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace skia {

namespace {

// Converts the argument to an 8-bit unsigned value by clamping to the range
// 0-255.
inline unsigned char ClampTo8(int a) {
  if (static_cast<unsigned>(a) < 256)
    return a;  // Avoid the extra check in the common case.
  if (a < 0)
    return 0;
  return 255;
}

// Stores a list of rows in a circular buffer. The usage is you write into it
// by calling AdvanceRow. It will keep track of which row in the buffer it
// should use next, and the total number of rows added.
class CircularRowBuffer {
 public:
  // The number of pixels in each row is given in |source_row_pixel_width|.
  // The maximum number of rows needed in the buffer is |max_y_filter_size|
  // (we only need to store enough rows for the biggest filter).
  //
  // We use the |first_input_row| to compute the coordinates of all of the
  // following rows returned by Advance().
  CircularRowBuffer(int dest_row_pixel_width, int max_y_filter_size,
                    int first_input_row)
      : row_byte_width_(dest_row_pixel_width * 4),
        num_rows_(max_y_filter_size),
        next_row_(0),
        next_row_coordinate_(first_input_row) {
    buffer_.resize(row_byte_width_ * max_y_filter_size);
    row_addresses_.resize(num_rows_);
  }

  // Moves to the next row in the buffer, returning a pointer to the beginning
  // of it.
  unsigned char* AdvanceRow() {
    unsigned char* row = &buffer_[next_row_ * row_byte_width_];
    next_row_coordinate_++;

    // Set the pointer to the next row to use, wrapping around if necessary.
    next_row_++;
    if (next_row_ == num_rows_)
      next_row_ = 0;
    return row;
  }

  // Returns a pointer to an "unrolled" array of rows. These rows will start
  // at the y coordinate placed into |*first_row_index| and will continue in
  // order for the maximum number of rows in this circular buffer.
  //
  // The |first_row_index_| may be negative. This means the circular buffer
  // starts before the top of the image (it hasn't been filled yet).
  unsigned char* const* GetRowAddresses(int* first_row_index) {
    // Example for a 4-element circular buffer holding coords 6-9.
    //   Row 0   Coord 8
    //   Row 1   Coord 9
    //   Row 2   Coord 6  <- next_row_ = 2, next_row_coordinate_ = 10.
    //   Row 3   Coord 7
    //
    // The "next" row is also the first (lowest) coordinate. This computation
    // may yield a negative value, but that's OK, the math will work out
    // since the user of this buffer will compute the offset relative
    // to the first_row_index and the negative rows will never be used.
    *first_row_index = next_row_coordinate_ - num_rows_;

    int cur_row = next_row_;
    for (int i = 0; i < num_rows_; i++) {
      row_addresses_[i] = &buffer_[cur_row * row_byte_width_];

      // Advance to the next row, wrapping if necessary.
      cur_row++;
      if (cur_row == num_rows_)
        cur_row = 0;
    }
    return &row_addresses_[0];
  }

 private:
  // The buffer storing the rows. They are packed, each one row_byte_width_.
  std::vector<unsigned char> buffer_;

  // Number of bytes per row in the |buffer_|.
  int row_byte_width_;

  // The number of rows available in the buffer.
  int num_rows_;

  // The next row index we should write into. This wraps around as the
  // circular buffer is used.
  int next_row_;

  // The y coordinate of the |next_row_|. This is incremented each time a
  // new row is appended and does not wrap.
  int next_row_coordinate_;

  // Buffer used by GetRowAddresses().
  std::vector<unsigned char*> row_addresses_;
};

// Convolves horizontally along a single row. The row data is given in
// |src_data| and continues for the num_values() of the filter.
template<bool has_alpha>
void ConvolveHorizontally(const unsigned char* src_data,
                          const ConvolusionFilter1D& filter,
                          unsigned char* out_row) {
  // Loop over each pixel on this row in the output image.
  int num_values = filter.num_values();
  for (int out_x = 0; out_x < num_values; out_x++) {
    // Get the filter that determines the current output pixel.
    int filter_offset, filter_length;
    const ConvolusionFilter1D::Fixed* filter_values =
        filter.FilterForValue(out_x, &filter_offset, &filter_length);

    // Compute the first pixel in this row that the filter affects. It will
    // touch |filter_length| pixels (4 bytes each) after this.
    const unsigned char* row_to_filter = &src_data[filter_offset * 4];

    // Apply the filter to the row to get the destination pixel in |accum|.
    int accum[4] = {0};
    for (int filter_x = 0; filter_x < filter_length; filter_x++) {
      ConvolusionFilter1D::Fixed cur_filter = filter_values[filter_x];
      accum[0] += cur_filter * row_to_filter[filter_x * 4 + 0];
      accum[1] += cur_filter * row_to_filter[filter_x * 4 + 1];
      accum[2] += cur_filter * row_to_filter[filter_x * 4 + 2];
      if (has_alpha)
        accum[3] += cur_filter * row_to_filter[filter_x * 4 + 3];
    }

    // Bring this value back in range. All of the filter scaling factors
    // are in fixed point with kShiftBits bits of fractional part.
    accum[0] >>= ConvolusionFilter1D::kShiftBits;
    accum[1] >>= ConvolusionFilter1D::kShiftBits;
    accum[2] >>= ConvolusionFilter1D::kShiftBits;
    if (has_alpha)
      accum[3] >>= ConvolusionFilter1D::kShiftBits;

    // Store the new pixel.
    out_row[out_x * 4 + 0] = ClampTo8(accum[0]);
    out_row[out_x * 4 + 1] = ClampTo8(accum[1]);
    out_row[out_x * 4 + 2] = ClampTo8(accum[2]);
    if (has_alpha)
      out_row[out_x * 4 + 3] = ClampTo8(accum[3]);
  }
}

// Does vertical convolusion to produce one output row. The filter values and
// length are given in the first two parameters. These are applied to each
// of the rows pointed to in the |source_data_rows| array, with each row
// being |pixel_width| wide.
//
// The output must have room for |pixel_width * 4| bytes.
template<bool has_alpha>
void ConvolveVertically(const ConvolusionFilter1D::Fixed* filter_values,
                        int filter_length,
                        unsigned char* const* source_data_rows,
                        int pixel_width,
                        unsigned char* out_row) {
  // We go through each column in the output and do a vertical convolusion,
  // generating one output pixel each time.
  for (int out_x = 0; out_x < pixel_width; out_x++) {
    // Compute the number of bytes over in each row that the current column
    // we're convolving starts at. The pixel will cover the next 4 bytes.
    int byte_offset = out_x * 4;

    // Apply the filter to one column of pixels.
    int accum[4] = {0};
    for (int filter_y = 0; filter_y < filter_length; filter_y++) {
      ConvolusionFilter1D::Fixed cur_filter = filter_values[filter_y];
      accum[0] += cur_filter * source_data_rows[filter_y][byte_offset + 0];
      accum[1] += cur_filter * source_data_rows[filter_y][byte_offset + 1];
      accum[2] += cur_filter * source_data_rows[filter_y][byte_offset + 2];
      if (has_alpha)
        accum[3] += cur_filter * source_data_rows[filter_y][byte_offset + 3];
    }

    // Bring this value back in range. All of the filter scaling factors
    // are in fixed point with kShiftBits bits of precision.
    accum[0] >>= ConvolusionFilter1D::kShiftBits;
    accum[1] >>= ConvolusionFilter1D::kShiftBits;
    accum[2] >>= ConvolusionFilter1D::kShiftBits;
    if (has_alpha)
      accum[3] >>= ConvolusionFilter1D::kShiftBits;

    // Store the new pixel.
    out_row[byte_offset + 0] = ClampTo8(accum[0]);
    out_row[byte_offset + 1] = ClampTo8(accum[1]);
    out_row[byte_offset + 2] = ClampTo8(accum[2]);
    if (has_alpha) {
      unsigned char alpha = ClampTo8(accum[3]);

      // Make sure the alpha channel doesn't come out larger than any of the
      // color channels. We use premultipled alpha channels, so this should
      // never happen, but rounding errors will cause this from time to time.
      // These "impossible" colors will cause overflows (and hence random pixel
      // values) when the resulting bitmap is drawn to the screen.
      //
      // We only need to do this when generating the final output row (here).
      int max_color_channel = std::max(out_row[byte_offset + 0],
          std::max(out_row[byte_offset + 1], out_row[byte_offset + 2]));
      if (alpha < max_color_channel)
        out_row[byte_offset + 3] = max_color_channel;
      else
        out_row[byte_offset + 3] = alpha;
    } else {
      // No alpha channel, the image is opaque.
      out_row[byte_offset + 3] = 0xff;
    }
  }
}

}  // namespace

// ConvolusionFilter1D ---------------------------------------------------------

void ConvolusionFilter1D::AddFilter(int filter_offset,
                                    const float* filter_values,
                                    int filter_length) {
  FilterInstance instance;
  instance.data_location = static_cast<int>(filter_values_.size());
  instance.offset = filter_offset;
  instance.length = filter_length;
  filters_.push_back(instance);

  SkASSERT(filter_length > 0);
  for (int i = 0; i < filter_length; i++)
    filter_values_.push_back(FloatToFixed(filter_values[i]));

  max_filter_ = std::max(max_filter_, filter_length);
}

void ConvolusionFilter1D::AddFilter(int filter_offset,
                                    const Fixed* filter_values,
                                    int filter_length) {
  FilterInstance instance;
  instance.data_location = static_cast<int>(filter_values_.size());
  instance.offset = filter_offset;
  instance.length = filter_length;
  filters_.push_back(instance);

  SkASSERT(filter_length > 0);
  for (int i = 0; i < filter_length; i++)
    filter_values_.push_back(filter_values[i]);

  max_filter_ = std::max(max_filter_, filter_length);
}

// BGRAConvolve2D -------------------------------------------------------------

void BGRAConvolve2D(const unsigned char* source_data,
                    int source_byte_row_stride,
                    bool source_has_alpha,
                    const ConvolusionFilter1D& filter_x,
                    const ConvolusionFilter1D& filter_y,
                    unsigned char* output) {
  int max_y_filter_size = filter_y.max_filter();

  // The next row in the input that we will generate a horizontally
  // convolved row for. If the filter doesn't start at the beginning of the
  // image (this is the case when we are only resizing a subset), then we
  // don't want to generate any output rows before that. Compute the starting
  // row for convolusion as the first pixel for the first vertical filter.
  int filter_offset, filter_length;
  const ConvolusionFilter1D::Fixed* filter_values =
      filter_y.FilterForValue(0, &filter_offset, &filter_length);
  int next_x_row = filter_offset;

  // We loop over each row in the input doing a horizontal convolusion. This
  // will result in a horizontally convolved image. We write the results into
  // a circular buffer of convolved rows and do vertical convolusion as rows
  // are available. This prevents us from having to store the entire
  // intermediate image and helps cache coherency.
  CircularRowBuffer row_buffer(filter_x.num_values(), max_y_filter_size,
                               filter_offset);

  // Loop over every possible output row, processing just enough horizontal
  // convolusions to run each subsequent vertical convolusion.
  int output_row_byte_width = filter_x.num_values() * 4;
  int num_output_rows = filter_y.num_values();
  for (int out_y = 0; out_y < num_output_rows; out_y++) {
    filter_values = filter_y.FilterForValue(out_y,
                                            &filter_offset, &filter_length);

    // Generate output rows until we have enough to run the current filter.
    while (next_x_row < filter_offset + filter_length) {
      if (source_has_alpha) {
        ConvolveHorizontally<true>(
            &source_data[next_x_row * source_byte_row_stride],
            filter_x, row_buffer.AdvanceRow());
      } else {
        ConvolveHorizontally<false>(
            &source_data[next_x_row * source_byte_row_stride],
            filter_x, row_buffer.AdvanceRow());
      }
      next_x_row++;
    }

    // Compute where in the output image this row of final data will go.
    unsigned char* cur_output_row = &output[out_y * output_row_byte_width];

    // Get the list of rows that the circular buffer has, in order.
    int first_row_in_circular_buffer;
    unsigned char* const* rows_to_convolve =
        row_buffer.GetRowAddresses(&first_row_in_circular_buffer);

    // Now compute the start of the subset of those rows that the filter
    // needs.
    unsigned char* const* first_row_for_filter =
        &rows_to_convolve[filter_offset - first_row_in_circular_buffer];

    if (source_has_alpha) {
      ConvolveVertically<true>(filter_values, filter_length,
                               first_row_for_filter,
                               filter_x.num_values(), cur_output_row);
    } else {
      ConvolveVertically<false>(filter_values, filter_length,
                                first_row_for_filter,
                                filter_x.num_values(), cur_output_row);
    }
  }
}

}  // namespace skia

