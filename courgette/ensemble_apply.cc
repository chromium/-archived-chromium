// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the code to apply a Courgette patch.

#include "courgette/ensemble.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"

#include "courgette/crc.h"
#include "courgette/image_info.h"
#include "courgette/region.h"
#include "courgette/streams.h"
#include "courgette/simple_delta.h"
#include "courgette/win32_x86_patcher.h"

namespace courgette {

// EnsemblePatchApplication is all the logic and data required to apply the
// multi-stage patch.
class EnsemblePatchApplication {
 public:
  EnsemblePatchApplication();
  ~EnsemblePatchApplication();

  Status ReadHeader(SourceStream* header_stream);

  Status InitBase(const Region& region);

  Status ValidateBase();

  Status ReadInitialParameters(SourceStream* initial_parameters);

  Status PredictTransformParameters(SinkStreamSet* predicted_parameters);

  Status SubpatchTransformParameters(SinkStreamSet* prediction,
                                     SourceStream* correction,
                                     SourceStreamSet* corrected_parameters);

  Status TransformUp(SourceStreamSet* parameters,
                     SinkStreamSet* transformed_elements);

  Status SubpatchTransformedElements(SinkStreamSet* elements,
                                     SourceStream* correction,
                                     SourceStreamSet* corrected_elements);

  Status TransformDown(SourceStreamSet* transformed_elements,
                       SinkStream* basic_elements);

  Status SubpatchFinalOutput(SourceStream* original,
                             SourceStream* correction,
                             SinkStream* corrected_ensemble);

 private:
  Status SubpatchStreamSets(SinkStreamSet* predicted_items,
                            SourceStream* correction,
                            SourceStreamSet* corrected_items,
                            SinkStream* corrected_items_storage);

  Region base_region_;       // Location of in-memory copy of 'old' version.

  uint32 source_checksum_;
  uint32 target_checksum_;

  std::vector<TransformationPatcher*> patchers_;

  SinkStream corrected_parameters_storage_;
  SinkStream corrected_elements_storage_;

  DISALLOW_COPY_AND_ASSIGN(EnsemblePatchApplication);
};

EnsemblePatchApplication::EnsemblePatchApplication()
    : source_checksum_(0), target_checksum_(0) {
}

EnsemblePatchApplication::~EnsemblePatchApplication() {
  for (size_t i = 0;  i < patchers_.size();  ++i) {
    delete patchers_[i];
  }
}

Status EnsemblePatchApplication::ReadHeader(SourceStream* header_stream) {
  uint32 magic;
  if (!header_stream->ReadVarint32(&magic))
    return C_BAD_ENSEMBLE_MAGIC;

  if (magic != CourgettePatchFile::kMagic)
    return C_BAD_ENSEMBLE_MAGIC;

  uint32 version;
  if (!header_stream->ReadVarint32(&version))
    return C_BAD_ENSEMBLE_VERSION;

  if (version != CourgettePatchFile::kVersion)
    return C_BAD_ENSEMBLE_VERSION;

  if (!header_stream->ReadVarint32(&source_checksum_))
    return C_BAD_ENSEMBLE_HEADER;

  if (!header_stream->ReadVarint32(&target_checksum_))
    return C_BAD_ENSEMBLE_HEADER;

  return C_OK;
}

Status EnsemblePatchApplication::InitBase(const Region& region) {
  base_region_.assign(region);
  return C_OK;
}

Status EnsemblePatchApplication::ValidateBase() {
  uint32 checksum = CalculateCrc(base_region_.start(), base_region_.length());
  if (source_checksum_ != checksum)
    return C_BAD_ENSEMBLE_CRC;

  return C_OK;
}

Status EnsemblePatchApplication::ReadInitialParameters(
    SourceStream* transformation_parameters) {
  uint32 number_of_transformations = 0;
  if (!transformation_parameters->ReadVarint32(&number_of_transformations))
    return C_BAD_ENSEMBLE_HEADER;

  for (size_t i = 0;  i < number_of_transformations;  ++i) {
    uint32 kind;
    if (!transformation_parameters->ReadVarint32(&kind))
      return C_BAD_ENSEMBLE_HEADER;

    if (kind == CourgettePatchFile::T_COURGETTE_WIN32_X86) {
      TransformationPatcher* patcher =
          new CourgetteWin32X86Patcher(base_region_);
      patchers_.push_back(patcher);
    } else {
      return C_BAD_ENSEMBLE_HEADER;
    }
  }

  for (size_t i = 0;  i < patchers_.size();  ++i) {
    Status status = patchers_[i]->Init(transformation_parameters);
    if (status != C_OK)
      return status;
  }

  // All transformation_parameters should have been consumed by the above loop.
  if (!transformation_parameters->Empty())
    return C_BAD_ENSEMBLE_HEADER;

  return C_OK;
}

Status EnsemblePatchApplication::PredictTransformParameters(
    SinkStreamSet* all_predicted_parameters) {
  for (size_t i = 0;  i < patchers_.size();  ++i) {
    SinkStreamSet single_predicted_parameters;
    Status status =
        patchers_[i]->PredictTransformParameters(&single_predicted_parameters);
    if (status != C_OK)
      return status;
    if (!all_predicted_parameters->WriteSet(&single_predicted_parameters))
      return C_STREAM_ERROR;
  }
  return C_OK;
}

Status EnsemblePatchApplication::SubpatchTransformParameters(
    SinkStreamSet* predicted_parameters,
    SourceStream* correction,
    SourceStreamSet* corrected_parameters) {
  return SubpatchStreamSets(predicted_parameters,
                            correction,
                            corrected_parameters,
                            &corrected_parameters_storage_);
}

Status EnsemblePatchApplication::TransformUp(
    SourceStreamSet* parameters,
    SinkStreamSet* transformed_elements) {
  for (size_t i = 0;  i < patchers_.size();  ++i) {
    SourceStreamSet single_parameters;
    if (!parameters->ReadSet(&single_parameters))
      return C_STREAM_ERROR;
    SinkStreamSet single_transformed_element;
    Status status = patchers_[i]->Transform(&single_parameters,
                                            &single_transformed_element);
    if (status != C_OK)
      return status;
    if (!single_parameters.Empty())
      return C_STREAM_NOT_CONSUMED;
    if (!transformed_elements->WriteSet(&single_transformed_element))
      return C_STREAM_ERROR;
  }

  if (!parameters->Empty())
    return C_STREAM_NOT_CONSUMED;
  return C_OK;
}

Status EnsemblePatchApplication::SubpatchTransformedElements(
    SinkStreamSet* predicted_elements,
    SourceStream* correction,
    SourceStreamSet* corrected_elements) {
  return SubpatchStreamSets(predicted_elements,
                            correction,
                            corrected_elements,
                            &corrected_elements_storage_);
}

Status EnsemblePatchApplication::TransformDown(
    SourceStreamSet* transformed_elements,
    SinkStream* basic_elements) {
  // Construct blob of original input followed by reformed elements.

  // The original input:
  basic_elements->Write(base_region_.start(), base_region_.length());

  for (size_t i = 0;  i < patchers_.size();  ++i) {
    SourceStreamSet single_corrected_element;
    if (!transformed_elements->ReadSet(&single_corrected_element))
      return C_STREAM_ERROR;
    Status status = patchers_[i]->Reform(&single_corrected_element,
                                         basic_elements);
    if (status != C_OK)
      return status;
    if (!single_corrected_element.Empty())
      return C_STREAM_NOT_CONSUMED;
  }

  if (!transformed_elements->Empty())
    return C_STREAM_NOT_CONSUMED;

  return C_OK;
}

Status EnsemblePatchApplication::SubpatchFinalOutput(
    SourceStream* original,
    SourceStream* correction,
    SinkStream* corrected_ensemble) {
  Status delta_status = ApplySimpleDelta(original, correction,
                                         corrected_ensemble);
  if (delta_status != C_OK)
    return delta_status;

  if (CalculateCrc(corrected_ensemble->Buffer(),
                   corrected_ensemble->Length()) != target_checksum_)
    return C_BAD_ENSEMBLE_CRC;

  return C_OK;
}

Status EnsemblePatchApplication::SubpatchStreamSets(
    SinkStreamSet* predicted_items,
    SourceStream* correction,
    SourceStreamSet* corrected_items,
    SinkStream* corrected_items_storage) {
  SinkStream linearized_predicted_items;
  if (!predicted_items->CopyTo(&linearized_predicted_items))
    return C_STREAM_ERROR;

  SourceStream prediction;
  prediction.Init(linearized_predicted_items);

  Status status = ApplySimpleDelta(&prediction,
                                   correction,
                                   corrected_items_storage);
  if (status != C_OK)
    return status;

  if (!corrected_items->Init(corrected_items_storage->Buffer(),
                             corrected_items_storage->Length()))
    return C_STREAM_ERROR;

  return C_OK;
}

Status ApplyEnsemblePatch(SourceStream* base,
                          SourceStream* patch,
                          SinkStream* output) {
  Status status;
  EnsemblePatchApplication patch_process;

  status = patch_process.ReadHeader(patch);
  if (status != C_OK)
    return status;

  status = patch_process.InitBase(Region(base->Buffer(), base->Remaining()));
  if (status != C_OK)
    return status;

  status = patch_process.ValidateBase();
  if (status != C_OK)
    return status;

  // The rest of the patch stream is a StreamSet.
  SourceStreamSet patch_streams;
  patch_streams.Init(patch);

  SourceStream* transformation_descriptions     = patch_streams.stream(0);
  SourceStream* parameter_correction            = patch_streams.stream(1);
  SourceStream* transformed_elements_correction = patch_streams.stream(2);
  SourceStream* ensemble_correction             = patch_streams.stream(3);

  status = patch_process.ReadInitialParameters(transformation_descriptions);
  if (status != C_OK)
    return status;

  SinkStreamSet predicted_parameters;
  status = patch_process.PredictTransformParameters(&predicted_parameters);
  if (status != C_OK)
    return status;

  SourceStreamSet corrected_parameters;
  status = patch_process.SubpatchTransformParameters(&predicted_parameters,
                                                     parameter_correction,
                                                     &corrected_parameters);
  if (status != C_OK)
    return status;

  SinkStreamSet transformed_elements;
  status = patch_process.TransformUp(&corrected_parameters,
                                     &transformed_elements);
  if (status != C_OK)
    return status;

  SourceStreamSet corrected_transformed_elements;
  status = patch_process.SubpatchTransformedElements(
          &transformed_elements,
          transformed_elements_correction,
          &corrected_transformed_elements);
  if (status != C_OK)
    return status;

  SinkStream original_ensemble_and_corrected_base_elements;
  status = patch_process.TransformDown(
      &corrected_transformed_elements,
      &original_ensemble_and_corrected_base_elements);
  if (status != C_OK)
    return status;

  SourceStream final_patch_prediction;
  final_patch_prediction.Init(original_ensemble_and_corrected_base_elements);
  status = patch_process.SubpatchFinalOutput(&final_patch_prediction,
                                             ensemble_correction, output);
  if (status != C_OK)
    return status;

  return C_OK;
}

Status ApplyEnsemblePatch(const wchar_t* old_file_name,
                          const wchar_t* patch_file_name,
                          const wchar_t* new_file_name) {
  Status status;

  // First read enough of the patch file to validate the header is well-formed.
  // A few varint32 numbers should fit in 100.
  FilePath patch_file_path(patch_file_name);
  const int BIG_ENOUGH_FOR_HEADER = 100;
  char buffer[BIG_ENOUGH_FOR_HEADER];
  int read_count =
      file_util::ReadFile(patch_file_path, buffer, sizeof(buffer));
  if (read_count < 0)
    return C_READ_OPEN_ERROR;

  // 'Dry-run' the first step of the patch process to validate format of header.
  SourceStream patch_header_stream;
  patch_header_stream.Init(buffer, read_count);
  EnsemblePatchApplication patch_process;
  status = patch_process.ReadHeader(&patch_header_stream);
  if (status != C_OK)
    return status;

  // Header smells good so read the whole patch file for real.
  std::string patch_file_buffer;
  if (!file_util::ReadFileToString(patch_file_path, &patch_file_buffer))
    return C_READ_ERROR;

  // Read the old_file.
  FilePath old_file_path(old_file_name);
  std::string old_file_buffer;
  if (!file_util::ReadFileToString(old_file_path, &old_file_buffer))
    return C_READ_ERROR;

  // Apply patch on streams.
  SourceStream old_source_stream;
  SourceStream patch_source_stream;
  old_source_stream.Init(old_file_buffer);
  patch_source_stream.Init(patch_file_buffer);
  SinkStream new_sink_stream;
  status = ApplyEnsemblePatch(&old_source_stream, &patch_source_stream,
                              &new_sink_stream);

  // Write the patched data to |new_file_name|.
  FilePath new_file_path(new_file_name);
  int written =
      file_util::WriteFile(
          new_file_path,
          reinterpret_cast<const char*>(new_sink_stream.Buffer()),
          new_sink_stream.Length());
  if (written == -1)
    return C_WRITE_OPEN_ERROR;
  if (written != new_sink_stream.Length())
    return C_WRITE_ERROR;

  return C_OK;
}

}  // namespace
