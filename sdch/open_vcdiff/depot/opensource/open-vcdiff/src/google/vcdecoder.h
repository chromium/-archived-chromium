// Copyright 2008 Google Inc.
// Author: Lincoln Smith
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPEN_VCDIFF_VCDECODER_H_
#define OPEN_VCDIFF_VCDECODER_H_

#include <cstddef>  // size_t
#include <string>
#include "google/output_string.h"

namespace open_vcdiff {

using std::string;

class VCDiffStreamingDecoderImpl;

// A streaming decoder class.  Takes a dictionary (source) file and a delta
// file, and produces the original target file.  It is intended to process
// the partial contents of the delta file as they arrive, in "chunks".
// As soon as a chunk of bytes is received from a file read or from a network
// transmission, it can be passed to DecodeChunk(), which will then output
// as much of the target file as it can.
//
// The client should use this class as follows:
//    VCDiffStreamingDecoder v;
//    v.StartDecoding(dictionary_ptr, dictionary_size);
//    while (any data left) {
//      if (!v.DecodeChunk(data, len, &output_string)) {
//        handle error;
//        break;
//      }
//      process(output_string);  // might have no new data, though
//    }
//    if (!v.FinishDecoding()) { ... handle error ... }
//
// I.e., the allowed pattern of calls is
//    StartDecoding DecodeChunk* FinishDecoding
//
// NOTE: It is not necessary to call FinishDecoding if DecodeChunk
//       returns false.  When DecodeChunk returns false to signal an
//       error, it resets its state and is ready for a new StartDecoding.
//       If FinishDecoding is called, it will also return false.
//
class VCDiffStreamingDecoder {
 public:
  VCDiffStreamingDecoder();
  ~VCDiffStreamingDecoder();

  // Resets the dictionary contents to "dictionary_ptr[0,dictionary_size-1]"
  // and sets up the data structures for decoding.  Note that the dictionary
  // contents are not copied, and the client is responsible for ensuring that
  // dictionary_ptr is valid until FinishDecoding is called.
  //
  void StartDecoding(const char* dictionary_ptr, size_t dictionary_size);

  // Accepts "data[0,len-1]" as additional data received in the
  // compressed stream.  If any chunks of data can be fully decoded,
  // they are appended to output_string.
  //
  // Returns true on success, and false if the data was malformed
  // or if there was an error in decoding it (e.g. out of memory, etc.).
  //
  // Note: we *append*, so the old contents of output_string stick around.
  // This convention differs from the non-streaming Encode/Decode
  // interfaces in VCDiffDecoder.
  //
  // output_string is guaranteed to be resized no more than once for each
  // window in the VCDIFF delta file.  This rule is irrespective
  // of the number of calls to DecodeString().
  //
  template<class OutputType>
  bool DecodeChunk(const char* data, size_t len, OutputType* output) {
    OutputString<OutputType> output_string(output);
    return DecodeChunkToInterface(data, len, &output_string);
  }

  bool DecodeChunkToInterface(const char* data, size_t len,
                              OutputStringInterface* output_string);

  // Finishes decoding after all data has been received.  Returns true
  // if decoding of the entire stream was successful.  FinishDecoding()
  // must be called for the current target before StartDecoding() can be
  // called for a different target.
  //
  bool FinishDecoding();

  // The decoder can create a version of the output target string with XML tags
  // added to indicate where each section of the decoded text came from.  This
  // can assist in debugging the decoder and/or determining the effectiveness of
  // a particular dictionary.  The following XML tags will be added.  Despite
  // the formatting of this example, newlines will not be added between tags.
  //     <dmatch>This text matched with the dictionary</dmatch>
  //     <bmatch>This text matched earlier target output</bmatch>
  //     <literal>This text found no match</literal>
  //
  // Calling EnableAnnotatedOutput() will enable this feature.  The interface
  // GetAnnotatedOutput() can be used to retrieve the annotated text.   It is
  // recommended to use this feature only when the target data consists of HTML
  // or other human-readable text.

  // Enables the annotated output feature.  After this method is called, new
  // target windows added to output_string by DecodeChunk() will also be added
  // to the annotated output, and can be retrieved using GetAnnotatedOutput().
  // If annotated output is already enabled, this function has no effect.
  void EnableAnnotatedOutput();

  // Disables the annotated output feature.  After calling this method,
  // GetAnnotatedOutput() will produce an empty string until
  // EnableAnnotatedOutput() is called again.
  void DisableAnnotatedOutput();

  // Replaces annotated_output with a copy of the annotated output string.
  // Annotated output collection begins when EnableAnnotatedOutput() is called.
  // The annotated output will be cleared each time StartDecoding() is called,
  // but not when FinishDecoding() is called.
  template<class OutputType>
  void GetAnnotatedOutput(OutputType* annotated_output) {
    OutputString<OutputType> output_string(annotated_output);
    GetAnnotatedOutputToInterface(&output_string);
  }

  void GetAnnotatedOutputToInterface(OutputStringInterface* annotated_output);

 private:
  VCDiffStreamingDecoderImpl* const impl_;

  // Make the copy constructor and assignment operator private
  // so that they don't inadvertently get used.
  explicit VCDiffStreamingDecoder(const VCDiffStreamingDecoder&);
  void operator=(const VCDiffStreamingDecoder&);
};

// A simpler (non-streaming) interface to the VCDIFF decoder that can be used
// if the entire delta file is available.
//
class VCDiffDecoder {
 public:
  VCDiffDecoder() { }
  ~VCDiffDecoder() { }

  /***** Simple interface *****/

  // Replaces old contents of "*target" with the result of decoding
  // the bytes found in "encoding."
  //
  // Returns true if "encoding" was a well-formed sequence of
  // instructions, and returns false if not.
  //
  template<class OutputType>
  bool Decode(const char* dictionary_ptr,
              size_t dictionary_size,
              const string& encoding,
              OutputType* target) {
    OutputString<OutputType> output_string(target);
    return DecodeToInterface(dictionary_ptr,
                             dictionary_size,
                             encoding,
                             &output_string);
  }

 private:
  bool DecodeToInterface(const char* dictionary_ptr,
                         size_t dictionary_size,
                         const string& encoding,
                         OutputStringInterface* target);

  VCDiffStreamingDecoder decoder_;

  // Make the copy constructor and assignment operator private
  // so that they don't inadvertently get used.
  explicit VCDiffDecoder(const VCDiffDecoder&);
  void operator=(const VCDiffDecoder&);
};

};  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_VCDECODER_H_
