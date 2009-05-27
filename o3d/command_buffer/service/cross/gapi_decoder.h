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


// This file contains the GAPI decoder class.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_GAPI_DECODER_H__
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_GAPI_DECODER_H__

#include "core/cross/types.h"
#include "command_buffer/service/cross/cmd_parser.h"

namespace o3d {
namespace command_buffer {

class GAPIInterface;
class CommandBufferEngine;

// This class implements the AsyncAPIInterface interface, decoding GAPI
// commands and sending them to a GAPI interface.
class GAPIDecoder : public AsyncAPIInterface {
 public:
  typedef BufferSyncInterface::ParseError ParseError;

  explicit GAPIDecoder(GAPIInterface *gapi) : gapi_(gapi), engine_(NULL) {}
  virtual ~GAPIDecoder() {}
  // Executes a command.
  // Parameters:
  //    command: the command index.
  //    arg_count: the number of CommandBufferEntry arguments.
  //    args: the arguments.
  // Returns:
  //   BufferSyncInterface::NO_ERROR if no error was found, one of
  //   BufferSyncInterface::ParseError otherwise.
  virtual ParseError DoCommand(unsigned int command,
                               unsigned int arg_count,
                               CommandBufferEntry *args);

  // Sets the engine, to get shared memory buffers from, and to set the token
  // to.
  void set_engine(CommandBufferEngine *engine) { engine_ = engine; }
 private:
  // Decodes the SET_VERTEX_INPUT command.
  ParseError DecodeSetVertexInput(unsigned int arg_count,
                                  CommandBufferEntry *args);

  // Decodes the CREATE_TEXTURE_2D command.
  ParseError DecodeCreateTexture2D(unsigned int arg_count,
                                   CommandBufferEntry *args);

  // Decodes the CREATE_TEXTURE_3D command.
  ParseError DecodeCreateTexture3D(unsigned int arg_count,
                                   CommandBufferEntry *args);

  // Decodes the CREATE_TEXTURE_CUBE command.
  ParseError DecodeCreateTextureCube(unsigned int arg_count,
                                     CommandBufferEntry *args);

  // Decodes the SET_TEXTURE_DATA command.
  ParseError DecodeSetTextureData(unsigned int arg_count,
                                  CommandBufferEntry *args);

  // Decodes the SET_TEXTURE_DATA_IMMEDIATE command.
  ParseError DecodeSetTextureDataImmediate(unsigned int arg_count,
                                           CommandBufferEntry *args);

  // Decodes the GET_TEXTURE_DATA command.
  ParseError DecodeGetTextureData(unsigned int arg_count,
                                  CommandBufferEntry *args);

  // Decodes the SET_SAMPLER_STATES command.
  ParseError DecodeSetSamplerStates(unsigned int arg_count,
                                    CommandBufferEntry *args);

  // Decodes the SET_STENCIL_TEST command.
  ParseError DecodeSetStencilTest(unsigned int arg_count,
                                  CommandBufferEntry *args);

  // Decodes the SET_BLENDING command.
  ParseError DecodeSetBlending(unsigned int arg_count,
                               CommandBufferEntry *args);

  // Gets the address of shared memory data, given a shared memory ID and an
  // offset. Also checks that the size is consistent with the shared memory
  // size.
  // Parameters:
  //   shm_id: the id of the shared memory buffer.
  //   offset: the offset of the data in the shared memory buffer.
  //   size: the size of the data.
  // Returns:
  //   NULL if shm_id isn't a valid shared memory buffer ID or if the size
  //   check fails. Return a pointer to the data otherwise.
  void *GetAddressAndCheckSize(unsigned int shm_id,
                               unsigned int offset,
                               unsigned int size);
  GAPIInterface *gapi_;
  CommandBufferEngine *engine_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_GAPI_DECODER_H__
