/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the image codec operations for JPEG files.

// precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include <csetjmp>
#include "core/cross/bitmap.h"
#include "utils/cross/file_path_utils.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"

using file_util::OpenFile;
using file_util::CloseFile;

#define XMD_H     // Prevent redefinition of UINT32
#undef FAR        // Prevent redefinition of macro "FAR"

extern "C" {
// NOTE: Some win32 header that is included by DirectX's headers
// typedefs boolean to a type incompatible with libjpeg's definition. I can't
// figure out how to disable that header. So scope the 'boolean' definition
// with a typedef, only in this file.
#define boolean jpg_boolean
#include "jpeglib.h"
#include "jerror.h"
}

namespace {
// JPEGMemoryReader acts as a data source for libjpeg
// Create one of these objects after calling jpeg_create_decompress()
// and pass a pointer to the jpeg_decompress_struct initialized
// from that function.  |jpeg_data| is a pointer to the JPEG data
// which is of length |jpeg_data_length| bytes
//
// NB: don't add any virtual methods to this class as we depend on
// the initial memory layout of this object matching a jpeg_source_mgr
class JPEGMemoryReader : public jpeg_source_mgr {
 public:
  JPEGMemoryReader(j_decompress_ptr cinfo,
                   const uint8 *jpeg_data,
                   size_t jpeg_data_length) {
    // Store a pointer to ourselves that we'll get in the callbacks
    if (cinfo->src == NULL) {
      cinfo->src = (struct jpeg_source_mgr *)this;
    }

    // Setup our custom function callbacks
    init_source = InitSource;
    fill_input_buffer = FillInputBuffer;
    skip_input_data = SkipInputData;
    resync_to_restart = jpeg_resync_to_restart; // default method from libjpeg
    term_source = TermSource;

    bytes_in_buffer = jpeg_data_length;

    uint8 *p = const_cast<uint8*>(jpeg_data);
    next_input_byte = reinterpret_cast<JOCTET*>(p);
  }

 private:
  // Callback functions, most of them do nothing
  METHODDEF(void) InitSource(j_decompress_ptr cinfo) {};  // nop

  METHODDEF(boolean) FillInputBuffer(j_decompress_ptr cinfo) {
    // Should not be called because we already have all the data
    ERREXIT(cinfo, JERR_INPUT_EOF);
    return TRUE;
  }

  // Here's the one which essentially "reads" from the memory buffer
  METHODDEF(void) SkipInputData(j_decompress_ptr cinfo, long num_bytes) {
    JPEGMemoryReader *This = (JPEGMemoryReader*)cinfo->src;

    int i = This->bytes_in_buffer - num_bytes;
    if (i < 0) i = 0;
    This->bytes_in_buffer = i;
    This->next_input_byte += num_bytes;
  }

  METHODDEF(void) TermSource(j_decompress_ptr cinfo) {};  // nop
};
}  // namespace

namespace o3d {

struct my_error_mgr {
  struct jpeg_error_mgr pub;  // "public" fields
  jmp_buf setjmp_buffer;      // for return to caller
};

typedef struct my_error_mgr* my_error_ptr;

METHODDEF(void) my_error_exit(j_common_ptr cinfo) {
  // return control to the original error handler.
  my_error_ptr myerr = reinterpret_cast<my_error_ptr>(cinfo->err);
  std::longjmp(myerr->setjmp_buffer, 1);
}

// Loads the raw RGB bitmap data from a compressed JPEG stream and converts
// the result to 24- or 32-bit bitmap data.
bool Bitmap::LoadFromJPEGStream(MemoryReadStream *stream,
                                const String &filename,
                                bool generate_mipmaps) {
  // Workspace for libjpeg decompression.
  struct jpeg_decompress_struct cinfo;
  // create our custom error handler.
  struct my_error_mgr jerr;
  JSAMPARRAY buffer;  // Output buffer - a row of pixels.
  int row_stride;     // Physical row width in output buffer.


  // Step 1: allocate and initialize JPEG decompression object

  // We set up the normal JPEG error routines, then override error_exit.
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;

  // NOTE: The following smart pointer needs to be declared before the
  // setjmp so that it is properly destroyed if we jump back.
  scoped_array<unsigned char> image_data;

  // Establish the setjmp return context for my_error_exit to use.
  if (setjmp(jerr.setjmp_buffer)) {
    // If control is returned here, an error occurred inside libjpeg.
    // Log the error message.
    char buffer[JMSG_LENGTH_MAX];
    (*cinfo.err->format_message) (reinterpret_cast<j_common_ptr>(&cinfo),
                                  buffer);
    DLOG(ERROR) << "JPEG load error: " << buffer;
    // Clean up.
    jpeg_destroy_decompress(&cinfo);
    return false;
  }

  // Initialize the JPEG decompression object.
  jpeg_create_decompress(&cinfo);

  // Creating a JPEGMemoryReader instance modifies some state in |cinfo|
  // in such a way to read JPEG data from memory buffer |jpeg_data| of
  // length |jpeg_data_length|
  size_t jpeg_data_length = stream->GetTotalStreamLength();
  const uint8 *jpeg_data = stream->GetDirectMemoryPointer();
  JPEGMemoryReader(&cinfo, jpeg_data, jpeg_data_length);

  // Step 3: read the JPEG header and allocate storage
  jpeg_read_header(&cinfo, TRUE);

  // Set the Bitmap member variables from the jpeg_decompress_struct fields.
  unsigned int width = cinfo.image_width;
  unsigned int height = cinfo.image_height;
  if (!CheckImageDimensions(width, height)) {
    DLOG(ERROR) << "Failed to load " << filename
                << ": dimensions are too large (" << width
                << ", " << height << ").";
    // Use the jpeg error system to clean up and exit.
    ERREXIT(&cinfo, JERR_QUANT_COMPONENTS);
  }

  if (cinfo.num_components != 3) {
    DLOG(ERROR) << "JPEG load error: Bad number of pixel channels ("
                << cinfo.num_components
                << ") in file \"" << filename << "\"";
    // Use the jpeg error system to clean up and exit.
    ERREXIT(&cinfo, JERR_QUANT_COMPONENTS);
  }
  unsigned int image_components = 4;
  unsigned int num_mipmaps =
      generate_mipmaps ? GetMipMapCount(width, height) : 1;
  Texture::Format format = Texture::XRGB8;
  // Allocate storage for the pixels.
  unsigned int image_size = GetMipChainSize(width, height, format,
                                            num_mipmaps);
  image_data.reset(new unsigned char[image_size]);
  if (image_data.get() == NULL) {
    DLOG(ERROR) << "JPEG memory allocation error \"" << filename << "\"";
    // Invoke the longjmp() error handler.
    ERREXIT(&cinfo, JERR_OUT_OF_MEMORY);
  }

  // Step 4: set parameters for decompression
  // Use the default decompression settings.

  // Step 5: Start decompressor
  jpeg_start_decompress(&cinfo);
  // These should be equal because we don't use scaling.
  // TODO: use libjpeg scaling for NPOT->POT ?
  DCHECK_EQ(width, cinfo.output_width);
  DCHECK_EQ(height, cinfo.output_height);

  // Create an output work buffer of the right size.
  row_stride = cinfo.output_width * cinfo.output_components;

  // Make a one-row-high sample array, using the JPEG library memory manager.
  // This buffer will be deallocated when decompression is finished.
  buffer = (*cinfo.mem->alloc_sarray)( reinterpret_cast<j_common_ptr>(&cinfo),
                                       JPOOL_IMAGE,
                                       row_stride,
                                       1);

  // Step 6: while (scan lines remain to be read)
  //           jpeg_read_scanlines(...);

  // Use the library's state variable cinfo.output_scanline as the
  // loop counter, so that we don't have to keep track ourselves.
  while (cinfo.output_scanline < height) {
    // jpeg_read_scanlines() expects an array of pointers to scanlines.
    // Here we ask for only one scanline to be read into "buffer".
    jpeg_read_scanlines(&cinfo, buffer, 1);

    // output_scanline is the numbe of scanlines that have been emitted.
    DCHECK_LE(cinfo.output_scanline, height);

    // Initialise the buffer write location.
    // NOTE: we load images bottom to up to respect max/maya's UV
    // orientation.
    unsigned char *image_write_ptr = image_data.get()
        + (height - cinfo.output_scanline) * width * image_components;
    // copy the scanline to its final destination
    for (unsigned int i = 0; i < width; ++i) {
      // RGB -> BGRX
      image_write_ptr[i*image_components+0] =
          buffer[0][i*cinfo.output_components+2];
      image_write_ptr[i*image_components+1] =
          buffer[0][i*cinfo.output_components+1];
      image_write_ptr[i*image_components+2] =
          buffer[0][i*cinfo.output_components+0];
      image_write_ptr[i*image_components+3] = 0xff;
    }
  }

  // Step 7: Finish decompression.
  jpeg_finish_decompress(&cinfo);

  // Step 8: Release JPEG decompression object nd free workspace.
  jpeg_destroy_decompress(&cinfo);

  // Check for jpeg decompression warnings.
  DLOG(WARNING) << "JPEG decompression warnings: " << jerr.pub.num_warnings;

  if (generate_mipmaps) {
    if (!GenerateMipmaps(width, height, format, num_mipmaps,
                         image_data.get())) {
      DLOG(ERROR) << "mip-map generation failed for \"" << filename << "\"";
      return false;
    }
  }

  // Success.
  image_data_.swap(image_data);
  width_ = width;
  height_ = height;
  format_ = format;
  num_mipmaps_ = num_mipmaps;

  return true;
}

}  // namespace o3d
