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


// This file implements the EffectHelper class.

#include "command_buffer/common/cross/cmd_buffer_format.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/client/cross/effect_helper.h"
#include "command_buffer/client/cross/fenced_allocator.h"
#include "command_buffer/client/cross/id_allocator.h"

// TODO: write a unit test.

namespace o3d {
namespace command_buffer {

bool EffectHelper::CreateEffectParameters(ResourceID effect_id,
                                          std::vector<EffectParamDesc> *descs) {
  using effect_param::Desc;
  DCHECK_NE(effect_id, kInvalidResource);
  DCHECK(descs);
  descs->clear();

  // Get the param count.
  Uint32 *retval = shm_allocator_->AllocTyped<Uint32>(1);
  CommandBufferEntry args[4];
  args[0].value_uint32 = effect_id;
  args[1].value_uint32 = sizeof(*retval);
  args[2].value_uint32 = shm_id_;
  args[3].value_uint32 = shm_allocator_->GetOffset(retval);
  helper_->AddCommand(GET_PARAM_COUNT, 4, args);
  // Finish has to be called to get the result.
  helper_->Finish();

  // We could have failed if the effect_id is invalid.
  if (helper_->interface()->GetParseError() !=
      BufferSyncInterface::PARSE_NO_ERROR) {
    shm_allocator_->Free(retval);
    return false;
  }
  unsigned int param_count = *retval;

  shm_allocator_->Free(retval);
  unsigned int max_buffer_size = shm_allocator_->GetLargestFreeOrPendingSize();
  if (max_buffer_size < sizeof(Desc)) {  // NOLINT
    // Not enough memory to get at least 1 param desc.
    return false;
  }
  descs->resize(param_count);
  for (unsigned int i = 0; i < param_count; ++i) {
    EffectParamDesc *desc = &((*descs)[i]);
    desc->id = param_id_allocator_->AllocateID();
    args[0].value_uint32 = desc->id;
    args[1].value_uint32 = effect_id;
    args[2].value_uint32 = i;
    helper_->AddCommand(CREATE_PARAM, 3, args);
  }

  // Read param descriptions in batches. We use as much shared memory as
  // possible so that we only call Finish as little as possible.
  unsigned int max_param_per_batch =
      std::min(param_count, max_buffer_size / sizeof(Desc));  // NOLINT
  Desc *raw_descs = shm_allocator_->AllocTyped<Desc>(max_param_per_batch);
  DCHECK(raw_descs);
  for (unsigned int i = 0; i < param_count; i += max_param_per_batch) {
    unsigned int count = std::min(param_count - i, max_param_per_batch);
    for (unsigned int j = 0 ; j < count; ++j) {
      EffectParamDesc *desc = &((*descs)[i + j]);
      Desc *raw_desc = raw_descs + j;
      args[0].value_uint32 = desc->id;
      args[1].value_uint32 = sizeof(*raw_desc);
      args[2].value_uint32 = shm_id_;
      args[3].value_uint32 = shm_allocator_->GetOffset(raw_desc);
      helper_->AddCommand(GET_PARAM_DESC, 4, args);
    }
    // Finish to get the results.
    helper_->Finish();
    DCHECK_EQ(helper_->interface()->GetParseError(),
              BufferSyncInterface::PARSE_NO_ERROR);
    for (unsigned int j = 0 ; j < count; ++j) {
      EffectParamDesc *desc = &((*descs)[i + j]);
      Desc *raw_desc = raw_descs + j;
      desc->data_type = raw_desc->data_type;
      desc->data_size = raw_desc->data_size;
      desc->cmd_desc_size = raw_desc->size;
    }
  }
  shm_allocator_->Free(raw_descs);
  return true;
}

bool EffectHelper::GetParamStrings(EffectParamDesc *desc) {
  using effect_param::Desc;
  DCHECK(desc);
  DCHECK_NE(desc->id, kInvalidResource);
  // desc may not have come directly from CreateEffectParameters, so it may be
  // less than the minimum required size.
  unsigned int size = std::max(desc->cmd_desc_size, sizeof(Desc));  // NOLINT
  Desc *raw_desc = static_cast<Desc *>(shm_allocator_->Alloc(size));
  if (!raw_desc) {
    // Not enough memory to get the param desc.
    return false;
  }
  CommandBufferEntry args[4];
  args[0].value_uint32 = desc->id;
  args[1].value_uint32 = size;
  args[2].value_uint32 = shm_id_;
  args[3].value_uint32 = shm_allocator_->GetOffset(raw_desc);
  helper_->AddCommand(GET_PARAM_DESC, 4, args);
  // Finish to get the results.
  helper_->Finish();

  // We could have failed if the param ID is invalid.
  if (helper_->interface()->GetParseError() !=
      BufferSyncInterface::PARSE_NO_ERROR) {
    shm_allocator_->Free(raw_desc);
    return false;
  }

  if (raw_desc->size > size) {
    // We had not allocated enough memory the first time (e.g. if the
    // EffectParamDesc didn't come from CreateEffectParameters, so the user had
    // no way of knowing what size was needed for the strings), so re-allocate
    // and try again.
    size = raw_desc->size;
    desc->cmd_desc_size = size;
    shm_allocator_->Free(raw_desc);
    raw_desc = static_cast<Desc *>(shm_allocator_->Alloc(size));
    if (!raw_desc) {
      // Not enough memory to get the param desc.
      return false;
    }
    args[1].value_uint32 = size;
    args[3].value_uint32 = shm_allocator_->GetOffset(raw_desc);
    helper_->AddCommand(GET_PARAM_DESC, 4, args);
    // Finish to get the results.
    helper_->Finish();
    DCHECK_EQ(helper_->interface()->GetParseError(),
              BufferSyncInterface::PARSE_NO_ERROR);
    DCHECK_EQ(raw_desc->size, size);
  }

  const char *raw_desc_string = reinterpret_cast<char *>(raw_desc);
  if (raw_desc->name_offset) {
    DCHECK_LE(raw_desc->name_offset + raw_desc->name_size, raw_desc->size);
    DCHECK_GT(raw_desc->name_size, 0);
    DCHECK_EQ(raw_desc_string[raw_desc->name_offset + raw_desc->name_size - 1],
              0);
    desc->name = String(raw_desc_string + raw_desc->name_offset,
                        raw_desc->name_size - 1);
  } else {
    desc->name.clear();
  }
  if (raw_desc->semantic_offset) {
    DCHECK_LE(raw_desc->semantic_offset + raw_desc->semantic_size,
              raw_desc->size);
    DCHECK_GT(raw_desc->semantic_size, 0);
    DCHECK_EQ(raw_desc_string[raw_desc->semantic_offset +
                              raw_desc->semantic_size - 1],
              0);
    desc->semantic = String(raw_desc_string + raw_desc->semantic_offset,
                            raw_desc->semantic_size - 1);
  } else {
    desc->semantic.clear();
  }
  shm_allocator_->Free(raw_desc);
  return true;
}

void EffectHelper::DestroyEffectParameters(
    const std::vector<EffectParamDesc> &descs) {
  CommandBufferEntry args[1];
  for (unsigned int i = 0; i < descs.size(); ++i) {
    const EffectParamDesc &desc = descs[i];
    args[0].value_uint32 = desc.id;
    helper_->AddCommand(DESTROY_PARAM, 1, args);
    param_id_allocator_->FreeID(desc.id);
  }
}

bool EffectHelper::GetEffectStreams(ResourceID effect_id,
                                    std::vector<EffectStreamDesc> *descs) {
  using effect_stream::Desc;
  DCHECK_NE(effect_id, kInvalidResource);

  // Get the param count.
  Uint32 *retval = shm_allocator_->AllocTyped<Uint32>(1);
  CommandBufferEntry args[5];
  args[0].value_uint32 = effect_id;
  args[1].value_uint32 = sizeof(*retval);
  args[2].value_uint32 = shm_id_;
  args[3].value_uint32 = shm_allocator_->GetOffset(retval);
  helper_->AddCommand(GET_STREAM_COUNT, 4, args);
  // Finish has to be called to get the result.
  helper_->Finish();

  // We could have failed if the effect_id is invalid.
  if (helper_->interface()->GetParseError() !=
      BufferSyncInterface::PARSE_NO_ERROR) {
    shm_allocator_->Free(retval);
    return false;
  }
  unsigned int stream_count = *retval;
  shm_allocator_->Free(retval);
  unsigned int max_buffer_size = shm_allocator_->GetLargestFreeOrPendingSize();
  if (max_buffer_size < sizeof(Desc)) {  // NOLINT
    // Not enough memory to get at least 1 stream desc.
    return false;
  }
  descs->resize(stream_count);

  // Read stream descriptions in batches. We use as much shared memory as
  // possible so that we only call Finish as little as possible.
  unsigned int max_stream_per_batch =
      std::min(stream_count, max_buffer_size / sizeof(Desc));  // NOLINT
  Desc *raw_descs = shm_allocator_->AllocTyped<Desc>(max_stream_per_batch);
  DCHECK(raw_descs);
  for (unsigned int i = 0; i < stream_count; i += max_stream_per_batch) {
    unsigned int count = std::min(stream_count - i, max_stream_per_batch);
    for (unsigned int j = 0 ; j < count; ++j) {
      EffectStreamDesc *desc = &((*descs)[i + j]);
      Desc *raw_desc = raw_descs + j;
      args[0].value_uint32 = effect_id;
      args[1].value_uint32 = i+j;
      args[2].value_uint32 = sizeof(*raw_desc);
      args[3].value_uint32 = shm_id_;
      args[4].value_uint32 = shm_allocator_->GetOffset(raw_desc);
      helper_->AddCommand(GET_STREAM_DESC, 5, args);
    }
    // Finish to get the results.
    helper_->Finish();
    DCHECK_EQ(helper_->interface()->GetParseError(),
              BufferSyncInterface::PARSE_NO_ERROR);
    for (unsigned int j = 0 ; j < count; ++j) {
      EffectStreamDesc *desc = &((*descs)[i + j]);
      Desc *raw_desc = raw_descs + j;
      desc->semantic = static_cast<vertex_struct::Semantic>(raw_desc->semantic);
      desc->semantic_index = raw_desc->semantic_index;
    }
  }
  shm_allocator_->Free(raw_descs);
  return true;
}
}  // namespace command_buffer
}  // namespace o3d
