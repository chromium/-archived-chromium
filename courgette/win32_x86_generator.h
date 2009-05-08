// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the transformation and adjustment for Windows X86 executables.

#ifndef COURGETTE_WIN32_X86_GENERATOR_H_
#define COURGETTE_WIN32_X86_GENERATOR_H_

#include "base/scoped_ptr.h"

#include "courgette/ensemble.h"

namespace courgette {

class CourgetteWin32X86PatchGenerator : public TransformationPatchGenerator {
 public:
  CourgetteWin32X86PatchGenerator(Element* old_element,
                                  Element* new_element,
                                  CourgetteWin32X86Patcher* patcher)
      : TransformationPatchGenerator(old_element, new_element, patcher) {
  }

  CourgettePatchFile::TransformationMethodId Kind() {
    return CourgettePatchFile::T_COURGETTE_WIN32_X86;
  }

  Status WriteInitialParameters(SinkStream* parameter_stream) {
    parameter_stream->WriteVarint32(old_element_->offset_in_ensemble());
    parameter_stream->WriteVarint32(old_element_->region().length());
    return C_OK;
    // TODO(sra): Initialize |patcher_| with these parameters.
  }

  Status PredictTransformParameters(SinkStreamSet* prediction) {
    return TransformationPatchGenerator::PredictTransformParameters(prediction);
  }

  Status CorrectedTransformParameters(SinkStreamSet* parameters) {
    // No code needed to write an 'empty' parameter set.
    return C_OK;
  }

  // The format of a transformed_element is a serialized EncodedProgram.  We
  // first disassemble the original old and new Elements into AssemblyPrograms.
  // Then we adjust the new AssemblyProgram to make it as much like the old one
  // as possible, before converting the AssemblyPrograms to EncodedPrograms and
  // serializing them.
  Status Transform(SourceStreamSet* corrected_parameters,
                   SinkStreamSet* old_transformed_element,
                   SinkStreamSet* new_transformed_element) {
    // Don't expect any corrected parameters.
    if (!corrected_parameters->Empty())
      return C_GENERAL_ERROR;

    // Generate old version of program using |corrected_parameters|.
    // TODO(sra): refactor to use same code from patcher_.
    AssemblyProgram* old_program = NULL;
    Status old_parse_status =
        ParseWin32X86PE(old_element_->region().start(),
                        old_element_->region().length(),
                        &old_program);
    if (old_parse_status != C_OK)
      return old_parse_status;

    AssemblyProgram* new_program = NULL;
    Status new_parse_status =
        ParseWin32X86PE(new_element_->region().start(),
                        new_element_->region().length(),
                        &new_program);
    if (new_parse_status != C_OK) {
      DeleteAssemblyProgram(old_program);
      return new_parse_status;
    }

    EncodedProgram* old_encoded = NULL;
    Status old_encode_status = Encode(old_program, &old_encoded);
    if (old_encode_status != C_OK) {
      DeleteAssemblyProgram(old_program);
      return old_encode_status;
    }

    Status old_write_status =
        WriteEncodedProgram(old_encoded, old_transformed_element);
    DeleteEncodedProgram(old_encoded);
    if (old_write_status != C_OK) {
      DeleteAssemblyProgram(old_program);
      return old_write_status;
    }

    Status adjust_status = Adjust(*old_program, new_program);
    DeleteAssemblyProgram(old_program);
    if (adjust_status != C_OK) {
      DeleteAssemblyProgram(new_program);
      return adjust_status;
    }

    EncodedProgram* new_encoded = NULL;
    Status new_encode_status = Encode(new_program, &new_encoded);
    DeleteAssemblyProgram(new_program);
    if (new_encode_status != C_OK)
      return new_encode_status;

    Status new_write_status =
        WriteEncodedProgram(new_encoded, new_transformed_element);
    DeleteEncodedProgram(new_encoded);
    if (new_write_status != C_OK)
      return new_write_status;

    return C_OK;
  }

  Status Reform(SourceStreamSet* transformed_element,
                SinkStream* reformed_element) {
    return TransformationPatchGenerator::Reform(transformed_element,
                                                reformed_element);
  }

 private:
  ~CourgetteWin32X86PatchGenerator() { }

  DISALLOW_COPY_AND_ASSIGN(CourgetteWin32X86PatchGenerator);
};

}  // namespace courgette
#endif  // COURGETTE_WIN32_X86_GENERATOR_H_
