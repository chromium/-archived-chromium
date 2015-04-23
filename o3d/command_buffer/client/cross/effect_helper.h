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


// This file defines the EffectHelper class.

#ifndef O3D_COMMAND_BUFFER_CLIENT_CROSS_EFFECT_HELPER_H_
#define O3D_COMMAND_BUFFER_CLIENT_CROSS_EFFECT_HELPER_H_

#include <vector>
#include "command_buffer/common/cross/resource.h"

namespace o3d {
namespace command_buffer {

class FencedAllocatorWrapper;
class IdAllocator;
class CommandBufferHelper;

// A helper class to find parameters in an effect.
class EffectHelper {
 public:
  // A more usable version of effect_param::Desc
  struct EffectParamDesc {
    ResourceID id;                     // The resource ID for the param.
    String name;                       // The name of the param.
    String semantic;                   // The semantic of the param.
    effect_param::DataType data_type;  // The data type of a param.
    unsigned int data_size;            // The size of the data for a param.
    unsigned int cmd_desc_size;        // The size of the effect_param::Desc
                                       // structure (counting strings) for a
                                       // param.
  };
  struct EffectStreamDesc {
    vertex_struct::Semantic semantic;  // The semantic enum type.
    unsigned int semantic_index;
  };

  EffectHelper(CommandBufferHelper *helper,
               FencedAllocatorWrapper *shm_allocator,
               unsigned int shm_id,
               IdAllocator *param_id_allocator)
      : helper_(helper),
        shm_allocator_(shm_allocator),
        shm_id_(shm_id),
        param_id_allocator_(param_id_allocator) {
    DCHECK(helper);
    DCHECK(shm_allocator);
    DCHECK(param_id_allocator);
  }

  // Creates all the parameters in an effect and gets their descriptions. The
  // strings will not be retrieved, so name and semantic will be empty. The
  // cmd_desc_size field will be set to the proper size to be able to get the
  // strings with a single command within GetParamStrings, so it should be left
  // alone.
  //
  // The ResourceIDs will be allocated in the param_id_allocator.
  // Temporary buffers will be allocated in the shm_allocator, but they will be
  // freed before the function returns (possibly pending a token). At least
  // sizeof(effect_param::Desc) must be available for this function to succeed.
  // This function will call Finish(), hence will block.
  //
  // Parameters:
  //   effect_id: the ResourceID of the effect.
  //   descs: A pointer to a vector containing the returned descriptions.
  //   The pointed vector will be cleared.
  // Returns:
  //   true if successful. Reasons for failure are:
  //   - invalid effect_id,
  //   - not enough memory in the shm_allocator_.
  bool CreateEffectParameters(ResourceID effect_id,
                              std::vector<EffectParamDesc> *descs);

  // Gets the strings for a desc. This will fill in the values for the name and
  // semantic fields.
  // Temporary buffers will be allocated in the shm_allocator, but they will be
  // freed before the function returns (possibly pending a token). At least
  // desc.cmd_desc_size (as returned by CreateEffectParameters) must be
  // available for this function to succeed.
  // This function will call Finish(), hence will block.
  //
  // Parameters:
  //   desc: a pointer to the description for a parameter. The id field should
  //   be set to the ResourceID of the parameter.
  // Returns:
  //   true if successful. Reasons for failure are:
  //   - invalid parameter ResourceID,
  //   - not enough memory in the shm_allocator_.
  bool GetParamStrings(EffectParamDesc *desc);

  // Destroys all parameter resources referenced by the descriptions. The
  // ResourceID will be freed from the param_id_allocator.
  // Parameters:
  //   descs: the vector of descriptions containing the ResourceIDs of the
  //   parameters to destroy.
  void DestroyEffectParameters(const std::vector<EffectParamDesc> &descs);

  // Gets all the input stream semantics and semantic indices in an
  // array. These will be retrieved as many as possible at a time. At least
  // sizeof(effect_param::Desc) must be available for this function to succeed.
  // This function will call Finish(), hence will block.
  //
  // Parameters:
  //   effect_id: the ResourceID of the effect.
  //   descs:  A pointer to a vector containing the returned descriptions.
  //   The pointed vector will be cleared.
  // Returns:
  //   true if successful. Reasons for failure are:
  //   - invalid effect_id,
  //   - not enough memory in the shm_allocator_.
  bool GetEffectStreams(ResourceID effect_id,
                        std::vector<EffectStreamDesc> *descs);

 private:
  CommandBufferHelper *helper_;
  FencedAllocatorWrapper *shm_allocator_;
  unsigned int shm_id_;
  IdAllocator *param_id_allocator_;

  DISALLOW_COPY_AND_ASSIGN(EffectHelper);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_CLIENT_CROSS_EFFECT_HELPER_H_
