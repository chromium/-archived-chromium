// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_ASSEMBLY_PROGRAM_H_
#define COURGETTE_ASSEMBLY_PROGRAM_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"

#include "courgette/image_info.h"

namespace courgette {

class EncodedProgram;
class Instruction;

// A Label is a symbolic reference to an address.  Unlike a conventional
// assembly language, we always know the address.  The address will later be
// stored in a table and the Label will be replaced with the index into the
// table.
//
// TODO(sra): Make fields private and add setters and getters.
class Label {
 public:
  static const int kNoIndex = -1;
  Label() : rva_(0), index_(kNoIndex) {}
  explicit Label(RVA rva) : rva_(rva), index_(kNoIndex) {}

  RVA rva_;    // Address refered to by the label.
  int index_;  // Index of address in address table, kNoIndex until assigned.
};

typedef std::map<RVA, Label*> RVAToLabel;

// An AssemblyProgram is the result of disassembling an executable file.
//
// * The disassembler creates labels in the AssemblyProgram and emits
//   'Instructions'.
// * The disassembler then calls DefaultAssignIndexes to assign
//   addresses to positions in the address tables.
// * [Optional step]
// * At this point the AssemblyProgram can be converted into an
//   EncodedProgram and serialized to an output stream.
// * Later, the EncodedProgram can be deserialized and assembled into
//   the original file.
//
// The optional step is to modify the AssemblyProgram.  One form of modification
// is to assign indexes in such a way as to make the EncodedProgram for this
// AssemblyProgram look more like the EncodedProgram for some other
// AssemblyProgram.  The modification process should call UnassignIndexes, do
// its own assignment, and then call AssignRemainingIndexes to ensure all
// indexes are assigned.
//
class AssemblyProgram {
 public:
  AssemblyProgram();
  ~AssemblyProgram();

  void set_image_base(uint64 image_base) { image_base_ = image_base; }

  // Instructions will be assembled in the order they are emitted.

  // Generates an entire base relocation table.
  void EmitMakeRelocsInstruction();

  // Following instruction will be assembled at address 'rva'.
  void EmitOriginInstruction(RVA rva);

  // Generates a single byte of data or machine instruction.
  void EmitByteInstruction(uint8 byte);

  // Generates 4-byte relative reference to address of 'label'.
  void EmitRel32(Label* label);

  // Generates 4-byte absolute reference to address of 'label'.
  void EmitAbs32(Label* label);

  Label* FindOrMakeAbs32Label(RVA rva);
  Label* FindOrMakeRel32Label(RVA rva);

  void DefaultAssignIndexes();
  void UnassignIndexes();
  void AssignRemainingIndexes();

  EncodedProgram* Encode() const;

  // Accessor for instruction list.
  const std::vector<Instruction*>& instructions() const {
    return instructions_;
  }

  // Returns the label if the instruction contains and absolute address,
  // otherwise returns NULL.
  Label* InstructionAbs32Label(const Instruction* instruction) const;

  // Returns the label if the instruction contains and rel32 offset,
  // otherwise returns NULL.
  Label* InstructionRel32Label(const Instruction* instruction) const;


 private:
  void Emit(Instruction* instruction) { instructions_.push_back(instruction); }

  Label* FindLabel(RVA rva, RVAToLabel* labels);

  // Helper methods for the public versions.
  static void UnassignIndexes(RVAToLabel* labels);
  static void DefaultAssignIndexes(RVAToLabel* labels);
  static void AssignRemainingIndexes(RVAToLabel* labels);

  // Sharing instructions that emit a single byte saves a lot of space.
  Instruction* GetByteInstruction(uint8 byte);
  Instruction** byte_instruction_cache_;

  uint64 image_base_;  // Desired or mandated base address of image.

  std::vector<Instruction*> instructions_;  // All the instructions in program.

  // These are lookup maps to find the label associated with a given address.
  // We have separate label spaces for addresses referenced by rel32 labels and
  // abs32 labels.  This is somewhat arbitrary.
  RVAToLabel rel32_labels_;
  RVAToLabel abs32_labels_;

  DISALLOW_COPY_AND_ASSIGN(AssemblyProgram);
};

}  // namespace courgette
#endif  // COURGETTE_ASSEMBLY_PROGRAM_H_
