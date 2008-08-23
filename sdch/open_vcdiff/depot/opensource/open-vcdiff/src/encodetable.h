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
//
// There are two different representations of a Code Table's contents:
// VCDiffCodeTableData is the same as the format given in section 7
// of the RFC, and is used for transmission and decoding.  However,
// on the encoding side, it is useful to have a representation that
// can map efficiently from delta instructions to opcodes:
// VCDiffInstructionMap.  A VCDiffInstructionMap is constructed
// using a VCDiffCodeTableData.  For a custom code table, it is recommended
// that the VCDiffCodeTableData be defined as a static struct and that the
// VCDiffInstructionMap be a static pointer that gets initialized only once.

#ifndef OPEN_VCDIFF_ENCODETABLE_H_
#define OPEN_VCDIFF_ENCODETABLE_H_

#include <config.h>
#include <stdint.h>             // int32_t
#include <cstddef>              // size_t
#include <string>
#include <vector>
#include "addrcache.h"
#include "codetable.h"
#include "checksum.h"
#include "vcdiff_defs.h"

namespace open_vcdiff {

using std::string;

class OutputStringInterface;
struct VCDiffCodeTableData;

// An alternate representation of the data in a VCDiffCodeTableData that
// optimizes for fast encoding, that is, for taking a delta instruction
// inst (also known as instruction type), size, and mode and arriving at
// the corresponding opcode.
//
class VCDiffInstructionMap {
 public:
  // Create a VCDiffInstructionMap from the information in code_table_data.
  // Does not save a pointer to code_table_data after using its contents
  // to create the instruction->opcode mappings.  The caller *must* have
  // verified that code_table_data->Validate() returned true before
  // attempting to use this constructor.
  // max_mode is the maximum value for the mode of a COPY instruction.
  //
  VCDiffInstructionMap(const VCDiffCodeTableData& code_table_data,
                       unsigned char max_mode);

  static VCDiffInstructionMap* GetDefaultInstructionMap();

  // Finds an opcode that has the given inst, size, and mode for its first
  // instruction  and NOOP for its second instruction (or vice versa.)
  // Returns kNoOpcode if the code table does not have any matching
  // opcode. Otherwise, returns an opcode value between 0 and 255.
  //
  // If this function returns kNoOpcode for size > 0, the caller will
  // usually want to try again with size == 0 to find an opcode that
  // doesn't have a fixed size value.
  //
  // If this function returns kNoOpcode for size == 0, it is an error condition,
  // because any code table that passed the Validate() check should have a way
  // of expressing all combinations of inst and mode with size=0.
  //
  OpcodeOrNone LookupFirstOpcode(unsigned char inst,
                                 unsigned char size,
                                 unsigned char mode) const {
    return first_instruction_map_.Lookup(inst, size, mode);
  }

  // Given a first opcode (presumed to have been returned by a previous call to
  // lookupFirstOpcode), finds an opcode that has the same first instruction as
  // the first opcode, and has the given inst, size, and mode for its second
  // instruction.
  //
  // If this function returns kNoOpcode for size > 0, the caller will
  // usually want to try again with size == 0 to find an opcode that
  // doesn't have a fixed size value.
  //
  OpcodeOrNone LookupSecondOpcode(unsigned char first_opcode,
                                  unsigned char inst,
                                  unsigned char size,
                                  unsigned char mode) const {
    return second_instruction_map_.Lookup(first_opcode, inst, size, mode);
  }

 private:
  // Data structure used to implement LookupFirstOpcode efficiently.
  //
  class FirstInstructionMap {
   public:
    FirstInstructionMap(int num_insts_and_modes, int max_size_1);
    ~FirstInstructionMap();

    void Add(unsigned char inst,
             unsigned char size,
             unsigned char mode,
             unsigned char opcode) {
      OpcodeOrNone* opcode_slot = &first_opcodes_[inst + mode][size];
      if (*opcode_slot == kNoOpcode) {
        *opcode_slot = opcode;
      }
    }

    // See comments for LookupFirstOpcode, above.
    //
    OpcodeOrNone Lookup(unsigned char inst,
                        unsigned char size,
                        unsigned char mode) const {
      int inst_mode = (inst == VCD_COPY) ? (inst + mode) : inst;
      if (size > max_size_1_) {
        return kNoOpcode;
      }
      // Lookup specific-sized opcode
      return first_opcodes_[inst_mode][size];
    }

   private:
    // The number of possible combinations of inst (a VCDiffInstructionType) and
    // mode.  Since the mode is only used for COPY instructions, this number
    // is not (number of VCDiffInstructionType values) * (number of modes), but
    // rather (number of VCDiffInstructionType values other than VCD_COPY)
    // + (number of COPY modes).
    //
    // Compressing inst and mode into a single integer relies on
    // VCD_COPY being the last instruction type.  The inst+mode values are:
    // 0 (NOOP), 1 (ADD), 2 (RUN), 3 (COPY mode 0), 4 (COPY mode 1), ...
    //
    const int num_instruction_type_modes_;

    // The maximum value of a size1 element in code_table_data
    //
    const int max_size_1_;

    // There are two levels to first_opcodes_:
    // 1) A dynamically-allocated pointer array of size
    //    num_instruction_type_modes_ (one element for each combination of inst
    //    and mode.)  Every element of this array is non-NULL and contains
    //    a pointer to:
    // 2) A dynamically-allocated array of OpcodeOrNone values, with one element
    //    for each possible first instruction size (size1) in the code table.
    //    (In the default code table, for example, the maximum size used is 18,
    //    so these arrays would have 19 elements representing values 0
    //    through 18.)
    //
    OpcodeOrNone** first_opcodes_;

    // Making these private avoids implicit copy constructor
    // and assignment operator
    FirstInstructionMap(const FirstInstructionMap&);  // NOLINT
    void operator=(const FirstInstructionMap&);
  } first_instruction_map_;

  // Data structure used to implement LookupSecondOpcode efficiently.
  //
  class SecondInstructionMap {
   public:
    SecondInstructionMap(int num_insts_and_modes, int max_size_2);
    ~SecondInstructionMap();
    void Add(unsigned char first_opcode,
             unsigned char inst,
             unsigned char size,
             unsigned char mode,
             unsigned char second_opcode);

    // See comments for LookupSecondOpcode, above.
    OpcodeOrNone Lookup(unsigned char first_opcode,
                        unsigned char inst,
                        unsigned char size,
                        unsigned char mode) const;
   private:
    // See the member of the same name in FirstInstructionMap.
    const int num_instruction_type_modes_;

    // The maximum value of a size2 element in code_table_data
    const int max_size_2_;

    // There are three levels to second_opcodes_:
    // 1) A statically-allocated pointer array with one element
    //    for each possible opcode.  Each element can be NULL, or can point to:
    // 2) A dynamically-allocated pointer array of size
    //    num_instruction_type_modes_ (one element for each combination of inst
    //    and mode.)  Each element can be NULL, or can point to:
    // 3) A dynamically-allocated array with one element for each possible
    //    second instruction size in the code table.  (In the default code
    //    table, for example, the maximum size used is 6, so these arrays would
    //    have 7 elements representing values 0 through 6.)
    //
    OpcodeOrNone** second_opcodes_[VCDiffCodeTableData::kCodeTableSize];

    // Making these private avoids implicit copy constructor
    // and assignment operator
    SecondInstructionMap(const SecondInstructionMap&);  // NOLINT
    void operator=(const SecondInstructionMap&);
  } second_instruction_map_;

  static VCDiffInstructionMap* default_instruction_map;

  // Making these private avoids implicit copy constructor & assignment operator
  VCDiffInstructionMap(const VCDiffInstructionMap&);  // NOLINT
  void operator=(const VCDiffInstructionMap&);
};

// The method calls after construction *must* conform
// to the following pattern:
//    {{Add|Copy|Run}* [AddChecksum] Output}*
//
// When Output has been called in this sequence, a complete target window
// (as defined in RFC 3284 section 4.3) will have been appended to
// out (unless no calls to Add, Run, or Copy were made, in which
// case Output will do nothing.)  The output will not be available for use
// until after each call to Output().
//
// This class can also write a compressed
// representation of a non-standard code table to an string so that it
// can be sent with the data that was encoded using that code table.
//
// NOT threadsafe.
//
class VCDiffCodeTableWriter {
 public:
  // This constructor uses the default code table.
  // If interleaved is true, the encoder writes each delta file window
  // by interleaving instructions and sizes with their corresponding
  // addresses and data, rather than placing these elements into three
  // separate sections.  This facilitates providing partially
  // decoded results when only a portion of a delta file window
  // is received (e.g. when HTTP over TCP is used as the
  // transmission protocol.)  The interleaved format is
  // not consistent with the VCDIFF draft standard.
  //
  explicit VCDiffCodeTableWriter(bool interleaved);

  // Uses a non-standard code table and non-standard cache sizes.  The caller
  // must guarantee that code_table_data remains allocated for the lifetime of
  // the VCDiffCodeTableWriter object.  Note that this is different from how
  // VCDiffCodeTableReader::UseCodeTable works.  It is assumed that a given
  // encoder will use either the default code table or a statically-defined
  // non-standard code table, whereas the decoder must have the ability to read
  // an arbitrary non-standard code table from a delta file and discard it once
  // the file has been decoded.
  //
  VCDiffCodeTableWriter(bool interleaved,
                        int near_cache_size,
                        int same_cache_size,
                        const VCDiffCodeTableData& code_table_data,
                        unsigned char max_mode);

  ~VCDiffCodeTableWriter();

  // Initializes the constructed object for use.
  // This method must be called after a VCDiffCodeTableWriter is constructed
  // and before any of its other methods can be called.  It will return
  // false if there was an error initializing the object, or true if it
  // was successful.  After the object has been initialized and used,
  // Init() can be called again to restore the initial state of the object.
  //
  bool Init(size_t dictionary_size);

  size_t target_length() const { return target_length_; }

  // Encode an ADD opcode with the "size" bytes starting at data
  //
  void Add(const char* data, size_t size);

  // Encode a COPY opcode with args "offset" (into dictionary) and "size" bytes.
  //
  void Copy(int32_t offset, size_t size);

  // Encode a RUN opcode for "size" copies of the value "byte".
  //
  void Run(size_t size, unsigned char byte);

  void AddChecksum(VCDChecksum checksum) {
    add_checksum_ = true;
    checksum_ = checksum;
  }

  // Finishes encoding and appends the encoded delta window to the output
  // string.  The output string is not null-terminated and may contain embedded
  // '\0' characters.
  void Output(OutputStringInterface* out);

  const std::vector<int>& match_counts() const { return match_counts_; }

 private:
  // This is an estimate of the longest match size the encoder expects to find.
  // It is used to determine the initial size of the vector match_counts_.
  // If it is too large, then some space will be wasted on vector elements
  // that are not used.  If it is too small, then some time will be wasted
  // expanding match_counts_ to accommodate larger match sizes.
  static const size_t kMaxMatchSize = 2000;

  // The maximum value for the mode of a COPY instruction.
  const unsigned char max_mode_;

  // If interleaved is true, sets data_for_add_and_run_ and
  // addresses_for_copy_ to point at instructions_and_sizes_,
  // so that instructions, sizes, addresses and data will be
  // combined into a single interleaved stream.
  // If interleaved is false, sets data_for_add_and_run_ and
  // addresses_for_copy_ to point at their corresponding
  // separate_... strings, so that the three sections will
  // be generated separately from one another.
  //
  void InitSectionPointers(bool interleaved);

  // Determines the best opcode to encode an instruction, and appends
  // or substitutes that opcode and its size into the
  // instructions_and_sizes_ string.
  //
  void EncodeInstruction(VCDiffInstructionType inst,
                         size_t size,
                         unsigned char mode);

  void EncodeInstruction(VCDiffInstructionType inst, size_t size) {
    return EncodeInstruction(inst, size, 0);
  }

  // Calculates the number of bytes needed to store the given size value as a
  // variable-length integer (VarintBE).
  static size_t CalculateLengthOfSizeAsVarint(size_t size);

  // Appends the size value to the string as a variable-length integer.
  static void AppendSizeToString(size_t size, string* out);

  // Appends the size value to the output string as a variable-length integer.
  static void AppendSizeToOutputString(size_t size, OutputStringInterface* out);

  // Calculates the "Length of the delta encoding" field for the delta window
  // header, based on the sizes of the sections and of the other header
  // elements.
  size_t CalculateLengthOfTheDeltaEncoding() const;

  // None of the following 'string' objects are null-terminated.

  // A series of instruction opcodes, each of which may be followed
  // by one or two Varint values representing the size parameters
  // of the first and second instruction in the opcode.
  string instructions_and_sizes_;

  // A series of data arguments (byte values) used for ADD and RUN
  // instructions.  Depending on whether interleaved output is used
  // for streaming or not, the pointer may point to
  // separate_data_for_add_and_run_ or to instructions_and_sizes_.
  string *data_for_add_and_run_;
  string separate_data_for_add_and_run_;

  // A series of Varint addresses used for COPY instructions.
  // For the SAME mode, a byte value is stored instead of a Varint.
  // Depending on whether interleaved output is used
  // for streaming or not, the pointer may point to
  // separate_addresses_for_copy_ or to instructions_and_sizes_.
  string *addresses_for_copy_;
  string separate_addresses_for_copy_;

  VCDiffAddressCache address_cache_;

  size_t dictionary_size_;

  // The number of bytes of target data that has been encoded so far.
  // Each time Add(), Copy(), or Run() is called, this will be incremented.
  // The target length is used to compute HERE mode addresses
  // for COPY instructions, and is also written into the header
  // of the delta window when Output() is called.
  //
  size_t target_length_;

  const VCDiffCodeTableData* code_table_data_;

  // The instruction map facilitates finding an opcode quickly given an
  // instruction inst, size, and mode.  This is an alternate representation
  // of the same information that is found in code_table_data_.
  //
  const VCDiffInstructionMap* instruction_map_;

  // The zero-based index within instructions_and_sizes_ of the byte
  // that contains the last single-instruction opcode generated by
  // EncodeInstruction().  (See that function for exhaustive details.)
  // It is necessary to use an index rather than a pointer for this value
  // because instructions_and_sizes_ may be resized, which would invalidate
  // any pointers into its data buffer.  The value -1 is reserved to mean that
  // either no opcodes have been generated yet, or else the last opcode
  // generated was a double-instruction opcode.
  //
  int last_opcode_index_;

  // If true, an Adler32 checksum of the target window data will be written as
  // a variable-length integer, just after the size of the addresses section.
  //
  bool add_checksum_;

  // The checksum to be written to the current target window,
  // if add_checksum_ is true.
  // This will not be calculated based on the individual calls to Add(), Run(),
  // and Copy(), which would be unnecessarily expensive.  Instead, the code
  // that uses the VCDiffCodeTableWriter object is expected to calculate
  // the checksum all at once and to call AddChecksum() with that value.
  // Must be called sometime before calling Output(), though it can be called
  // either before or after the calls to Add(), Run(), and Copy().
  //
  VCDChecksum checksum_;

  // The value of match_counts_[n] is equal to the number of matches
  // of length n (that is, COPY instructions of size n) found so far.
  std::vector<int> match_counts_;

  // Making these private avoids implicit copy constructor & assignment operator
  VCDiffCodeTableWriter(const VCDiffCodeTableWriter&);  // NOLINT
  void operator=(const VCDiffCodeTableWriter&);
};

};  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_ENCODETABLE_H_
