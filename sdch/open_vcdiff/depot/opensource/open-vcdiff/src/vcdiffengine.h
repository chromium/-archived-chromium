// Copyright 2006 Google Inc.
// Authors: Sanjay Ghemawat, Jeff Dean, Chandra Chereddi, Lincoln Smith
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

#ifndef OPEN_VCDIFF_VCDIFFENGINE_H_
#define OPEN_VCDIFF_VCDIFFENGINE_H_

#include <config.h>
#include <stdint.h>  // uint32_t
#include <string>

namespace open_vcdiff {

using std::string;

class BlockHash;
class OutputStringInterface;
class VCDiffCodeTableWriter;

// The VCDiffEngine class is used to find the optimal encoding (in terms of
// COPY and ADD instructions) for a given dictionary and target window.
// To write the instructions for this encoding, it calls the Copy()
// and Add() methods of the VCDiffCodeTableWriter object which is passed
// as an argument to Encode().
//
class VCDiffEngine {
 public:
  VCDiffEngine(const char* dictionary,
               size_t dictionary_size)
      // If dictionary_size == 0, then dictionary could be NULL.  Guard against
      // using a NULL value.
      : dictionary_((dictionary_size > 0) ? dictionary : "", dictionary_size),
        hashed_dictionary_(NULL) { }

  ~VCDiffEngine();

  // Initializes the object before use.
  // This method must be called after constructing a VCDiffEngine object,
  // and before any other method may be called.  It should not be called
  // twice on the same object.
  // Returns true if initialization succeeded, or false if an error occurred,
  // in which case no other method except the destructor may then be used
  // on the object.
  // The Init() method is the only one allowed to treat hashed_dictionary_
  // as non-const.
  bool Init();

  const size_t dictionary_size() const { return dictionary_.size(); }

  // Main worker function.  Finds the best matches between the dictionary
  // (source) and target data, and uses the coder to write a
  // delta file window into *diff.
  // Because it is a const function, many threads
  // can call Encode() at once for the same VCDiffEngine object.
  // All thread-specific data will be stored in the coder and diff arguments.
  //
  // look_for_target_matches determines whether to look for matches
  // within the previously encoded target data, or just within the source
  // (dictionary) data.  Please see vcencoder.h for a full explanation
  // of this parameter.
  void Encode(const char* target_data,
              size_t target_size,
              bool look_for_target_matches,
              OutputStringInterface* diff,
              VCDiffCodeTableWriter* coder) const;

 private:
  // The minimum size of a string match that is worth putting into
  // a COPY instruction.  This value is taken from the RFC, which states:
  // "The choice of the minimum size 4 for COPY instructions in the default
  //  code table was made from experiments that showed that excluding small
  //  matches (less than 4 bytes long) improved the compression rates."
  static const size_t kMinimumMatchSize = 4;

  static bool ShouldGenerateCopyInstructionForMatchOfSize(size_t size) {
    return size >= kMinimumMatchSize;
  }

  size_t EncodeCopyForBestMatch(uint32_t hash_value,
                                const char* target_candidate_start,
                                const char* unencoded_target_start,
                                size_t unencoded_target_size,
                                const BlockHash* target_hash,
                                VCDiffCodeTableWriter* coder) const;

  void AddUnmatchedRemainder(const char* unencoded_target_start,
                             size_t unencoded_target_size,
                             VCDiffCodeTableWriter* coder) const;

  void FinishEncoding(size_t target_size,
                      OutputStringInterface* diff,
                      VCDiffCodeTableWriter* coder) const;

  const string dictionary_;  // A copy of the dictionary contents

  // A hash that contains one element for every kBlockSize bytes of dictionary_.
  // This can be reused to encode many different target strings using the
  // same dictionary, without the need to compute the hash values each time.
  const BlockHash* hashed_dictionary_;

  // Making these private avoids implicit copy constructor & assignment operator
  VCDiffEngine(const VCDiffEngine&);
  void operator=(const VCDiffEngine&);
};

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_VCDIFFENGINE_H_
