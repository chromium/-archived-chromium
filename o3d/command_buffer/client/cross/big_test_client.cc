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


#include <math.h>
#ifdef __native_client__
#include <sys/nacl_syscalls.h>
#else
#include "third_party/native_client/googleclient/native_client/src/trusted/desc/nrd_all_modules.h"
#endif
#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/common/cross/rpc_imc.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/client/cross/buffer_sync_proxy.h"
#include "third_party/vectormath/files/vectormathlibrary/include/vectormath/scalar/cpp/vectormath_aos.h"  // NOLINT

namespace o3d {
namespace command_buffer {

namespace math = Vectormath::Aos;

// Adds a Clear command into the command buffer.
// Parameters:
//   cmd_buffer: the command buffer helper.
//   buffers: a bitfield of which buffers to clear (a combination of
//     GAPIInterface::COLOR, GAPIInterface::DEPTH and GAPIInterface::STENCIL).
//   color: the color buffer clear value.
//   depth: the depth buffer clear value.
//   stencil: the stencil buffer clear value.
void ClearCmd(CommandBufferHelper *cmd_buffer,
              const unsigned int buffers,
              const RGBA &color,
              float depth,
              unsigned int stencil) {
  CommandBufferEntry args[7];
  args[0].value_uint32 = buffers;
  args[1].value_float = color.red;
  args[2].value_float = color.green;
  args[3].value_float = color.blue;
  args[4].value_float = color.alpha;
  args[5].value_float = depth;
  args[6].value_uint32 = stencil;
  cmd_buffer->AddCommand(command_buffer::CLEAR, 7, args);
}

// Adds a SetViewport command into the buffer.
// Parameters:
//   cmd_buffer: the command buffer helper.
//   x, y, width, height: the dimensions of the Viewport.
//   z_near, z_far: the near and far clip plane distances.
void SetViewportCmd(CommandBufferHelper *cmd_buffer,
                    unsigned int x,
                    unsigned int y,
                    unsigned int width,
                    unsigned int height,
                    float z_near,
                    float z_far) {
  CommandBufferEntry args[6];
  args[0].value_uint32 = x;
  args[1].value_uint32 = y;
  args[2].value_uint32 = width;
  args[3].value_uint32 = height;
  args[4].value_float = z_near;
  args[5].value_float = z_far;
  cmd_buffer->AddCommand(command_buffer::SET_VIEWPORT, 6, args);
}

// Copy a data buffer to args, for IMMEDIATE commands. Returns the number of
// args used.
unsigned int CopyToArgs(CommandBufferEntry *args,
                        const void *data,
                        size_t size) {
  memcpy(args, data, size);
  const unsigned int arg_size = sizeof(args[0]);
  return static_cast<unsigned int>((size + arg_size - 1) / arg_size);
}

// Our effect: pass through position and UV, look up texture.
// This follows the command buffer effect format :
// vertex_program_entry \0 fragment_program_entry \0 effect_code.
const char effect_data[] =
    "vs\0"  // Vertex program entry point
    "ps\0"  // Fragment program entry point
    "struct a2v {float4 pos: POSITION; float2 uv: TEXCOORD0;};\n"
    "struct v2f {float4 pos: POSITION; float2 uv: TEXCOORD0;};\n"
    "float4x4 worldViewProj : WorldViewProjection;\n"
    "v2f vs(a2v i) {\n"
    "  v2f o;\n"
    "  o.pos = mul(i.pos, worldViewProj);\n"
    "  o.uv = i.uv;\n"
    "  return o;\n"
    "}\n"
    "sampler s0;\n"
    "float4 ps(v2f i) : COLOR { return tex2D(s0, i.uv); }\n";

// Custom vertex, with position and color.
struct CustomVertex {
  float x, y, z, w;
  float u, v;
};

void BigTestClient(nacl::HtpHandle handle) {
  IMCSender sender(handle);
  BufferSyncProxy proxy(&sender);

  proxy.InitConnection();
  const int kShmSize = 2048;
  RPCShmHandle shm = CreateShm(kShmSize);
  void *shm_address = MapShm(shm, kShmSize);
  unsigned int shm_id = proxy.RegisterSharedMemory(shm, kShmSize);

  {
    CommandBufferHelper cmd_buffer(&proxy);
    cmd_buffer.Init(500);

    // Clear the buffers.
    RGBA color = {0.2f, 0.2f, 0.2f, 1.f};
    ClearCmd(&cmd_buffer, GAPIInterface::COLOR | GAPIInterface::DEPTH, color,
             1.f, 0);

    const ResourceID vertex_buffer_id = 1;
    const ResourceID vertex_struct_id = 1;

    // AddCommand copies the args, so it is safe to re-use args across various
    // calls.
    // 20 is the largest command we use (SET_PARAM_DATA_IMMEDIATE for matrices).
    CommandBufferEntry args[20];

    CustomVertex vertices[4] = {
      {-.5f, -.5f, 0.f, 1.f,  0, 0},
      {.5f,  -.5f, 0.f, 1.f,  1, 0},
      {-.5f,  .5f, 0.f, 1.f,  0, 1},
      {.5f,   .5f, 0.f, 1.f,  1, 1},
    };
    args[0].value_uint32 = vertex_buffer_id;
    args[1].value_uint32 = sizeof(vertices);  // size
    args[2].value_uint32 = 0;  // flags
    cmd_buffer.AddCommand(command_buffer::CREATE_VERTEX_BUFFER, 3, args);

    memcpy(shm_address, vertices, sizeof(vertices));
    args[0].value_uint32 = vertex_buffer_id;
    args[1].value_uint32 = 0;  // offset in VB
    args[2].value_uint32 = sizeof(vertices);  // size
    args[3].value_uint32 = shm_id;  // shm
    args[4].value_uint32 = 0;  // offset in shm
    cmd_buffer.AddCommand(command_buffer::SET_VERTEX_BUFFER_DATA, 5, args);
    unsigned int token = cmd_buffer.InsertToken();

    args[0].value_uint32 = vertex_struct_id;
    args[1].value_uint32 = 2;  // input count
    cmd_buffer.AddCommand(command_buffer::CREATE_VERTEX_STRUCT, 2, args);

    // Set POSITION input stream
    args[0].value_uint32 = vertex_struct_id;
    args[1].value_uint32 = 0;                               // input
    args[2].value_uint32 = vertex_buffer_id;                // buffer
    args[3].value_uint32 = 0;                               // offset
    args[4].value_uint32 =
        set_vertex_input_cmd::Stride::MakeValue(sizeof(CustomVertex)) |
        set_vertex_input_cmd::Type::MakeValue(vertex_struct::FLOAT4) |
        set_vertex_input_cmd::Semantic::MakeValue(vertex_struct::POSITION) |
        set_vertex_input_cmd::SemanticIndex::MakeValue(0);
    cmd_buffer.AddCommand(command_buffer::SET_VERTEX_INPUT, 5, args);

    // Set TEXCOORD0 input stream
    args[1].value_uint32 = 1;                               // input
    args[3].value_uint32 = 16;                              // offset
    args[4].value_uint32 =
        set_vertex_input_cmd::Stride::MakeValue(sizeof(CustomVertex)) |
        set_vertex_input_cmd::Type::MakeValue(vertex_struct::FLOAT2) |
        set_vertex_input_cmd::Semantic::MakeValue(vertex_struct::TEX_COORD) |
        set_vertex_input_cmd::SemanticIndex::MakeValue(0);
    cmd_buffer.AddCommand(command_buffer::SET_VERTEX_INPUT, 5, args);

    // wait for previous transfer to be executed, so that we can re-use the
    // transfer shared memory buffer.
    cmd_buffer.WaitForToken(token);
    memcpy(shm_address, effect_data, sizeof(effect_data));
    const ResourceID effect_id = 1;
    args[0].value_uint32 = effect_id;
    args[1].value_uint32 = sizeof(effect_data);  // size
    args[2].value_uint32 = shm_id;  // shm
    args[3].value_uint32 = 0;  // offset in shm
    cmd_buffer.AddCommand(command_buffer::CREATE_EFFECT, 4, args);
    token = cmd_buffer.InsertToken();

    // Create a 4x4 2D texture.
    const ResourceID texture_id = 1;
    args[0].value_uint32 = texture_id;
    args[1].value_uint32 =
        create_texture_2d_cmd::Width::MakeValue(4) |
        create_texture_2d_cmd::Height::MakeValue(4);
    args[2].value_uint32 =
        create_texture_2d_cmd::Levels::MakeValue(0) |
        create_texture_2d_cmd::Format::MakeValue(texture::ARGB8) |
        create_texture_2d_cmd::Flags::MakeValue(0);
    cmd_buffer.AddCommand(command_buffer::CREATE_TEXTURE_2D, 3, args);

    unsigned int texels[4] = {
      0xff0000ff,
      0xffff00ff,
      0xff00ffff,
      0xffffffff,
    };
    // wait for previous transfer to be executed, so that we can re-use the
    // transfer shared memory buffer.
    cmd_buffer.WaitForToken(token);
    memcpy(shm_address, texels, sizeof(texels));
    // Creates a 4x4 texture by uploading 2x2 data in each quadrant.
    for (unsigned int x = 0; x < 2; ++x)
      for (unsigned int y = 0; y < 2; ++y) {
        args[0].value_uint32 = texture_id;
        args[1].value_uint32 =
            set_texture_data_cmd::X::MakeValue(x*2) |
            set_texture_data_cmd::Y::MakeValue(y*2);
        args[2].value_uint32 =
            set_texture_data_cmd::Width::MakeValue(2) |
            set_texture_data_cmd::Height::MakeValue(2);
        args[3].value_uint32 =
            set_texture_data_cmd::Z::MakeValue(0) |
            set_texture_data_cmd::Depth::MakeValue(1);
        args[4].value_uint32 = set_texture_data_cmd::Level::MakeValue(0);
        args[5].value_uint32 = sizeof(texels[0]) * 2;  // row_pitch
        args[6].value_uint32 = 0;  // slice_pitch
        args[7].value_uint32 = sizeof(texels);  // size
        args[8].value_uint32 = shm_id;
        args[9].value_uint32 = 0;
        cmd_buffer.AddCommand(command_buffer::SET_TEXTURE_DATA, 10, args);
      }
    token = cmd_buffer.InsertToken();

    const ResourceID sampler_id = 1;
    args[0].value_uint32 = sampler_id;
    cmd_buffer.AddCommand(command_buffer::CREATE_SAMPLER, 1, args);

    args[0].value_uint32 = sampler_id;
    args[1].value_uint32 = texture_id;
    cmd_buffer.AddCommand(command_buffer::SET_SAMPLER_TEXTURE, 2, args);

    args[0].value_uint32 = sampler_id;
    args[1].value_uint32 =
        set_sampler_states::AddressingU::MakeValue(sampler::CLAMP_TO_EDGE) |
        set_sampler_states::AddressingV::MakeValue(sampler::CLAMP_TO_EDGE) |
        set_sampler_states::AddressingW::MakeValue(sampler::CLAMP_TO_EDGE) |
        set_sampler_states::MagFilter::MakeValue(sampler::POINT) |
        set_sampler_states::MinFilter::MakeValue(sampler::POINT) |
        set_sampler_states::MipFilter::MakeValue(sampler::NONE) |
        set_sampler_states::MaxAnisotropy::MakeValue(1);
    cmd_buffer.AddCommand(command_buffer::SET_SAMPLER_STATES, 2, args);

    // Create a parameter for the sampler.
    const ResourceID sampler_param_id = 1;
    {
      const char param_name[] = "s0";
      args[0].value_uint32 = sampler_param_id;
      args[1].value_uint32 = effect_id;
      args[2].value_uint32 = sizeof(param_name);
      unsigned int arg_count = CopyToArgs(args + 3, param_name,
                                          sizeof(param_name));
      cmd_buffer.AddCommand(command_buffer::CREATE_PARAM_BY_NAME_IMMEDIATE,
                            3 + arg_count, args);
    }

    const ResourceID matrix_param_id = 2;
    {
      const char param_name[] = "worldViewProj";
      args[0].value_uint32 = matrix_param_id;
      args[1].value_uint32 = effect_id;
      args[2].value_uint32 = sizeof(param_name);
      unsigned int arg_count = CopyToArgs(args + 3, param_name,
                                          sizeof(param_name));
      cmd_buffer.AddCommand(command_buffer::CREATE_PARAM_BY_NAME_IMMEDIATE,
                            3 + arg_count, args);
    }

    float t = 0.f;
    while (true) {
      t = fmodf(t + .01f, 1.f);
      math::Matrix4 m =
          math::Matrix4::translation(math::Vector3(0.f, 0.f, .5f));
      m *= math::Matrix4::rotationY(t * 2 * 3.1415926f);
      cmd_buffer.AddCommand(command_buffer::BEGIN_FRAME, 0 , NULL);
      // Clear the background with an animated color (black to red).
      ClearCmd(&cmd_buffer, GAPIInterface::COLOR | GAPIInterface::DEPTH, color,
               1.f, 0);

      args[0].value_uint32 = vertex_struct_id;
      cmd_buffer.AddCommand(command_buffer::SET_VERTEX_STRUCT, 1, args);

      args[0].value_uint32 = effect_id;
      cmd_buffer.AddCommand(command_buffer::SET_EFFECT, 1, args);

      args[0].value_uint32 = sampler_param_id;
      args[1].value_uint32 = sizeof(Uint32);  // NOLINT
      args[2].value_uint32 = sampler_id;
      cmd_buffer.AddCommand(command_buffer::SET_PARAM_DATA_IMMEDIATE, 3, args);

      args[0].value_uint32 = matrix_param_id;
      args[1].value_uint32 = sizeof(m);
      unsigned int arg_count = CopyToArgs(args + 2, &m, sizeof(m));
      cmd_buffer.AddCommand(command_buffer::SET_PARAM_DATA_IMMEDIATE,
                            2 + arg_count, args);

      args[0].value_uint32 = GAPIInterface::TRIANGLE_STRIPS;
      args[1].value_uint32 = 0;  // first
      args[2].value_uint32 = 2;  // primitive count
      cmd_buffer.AddCommand(command_buffer::DRAW, 3, args);

      cmd_buffer.AddCommand(command_buffer::END_FRAME, 0 , NULL);
      cmd_buffer.Flush();
    }

    cmd_buffer.Finish();
  }

  proxy.CloseConnection();
  proxy.UnregisterSharedMemory(shm_id);
  DestroyShm(shm);

  sender.SendCall(POISONED_MESSAGE_ID, NULL, 0, NULL, 0);
}

}  // namespace command_buffer
}  // namespace o3d

nacl::HtpHandle InitConnection(int argc, char **argv) {
  nacl::Handle handle = nacl::kInvalidHandle;
#ifndef __native_client__
  NaClNrdAllModulesInit();

  static nacl::SocketAddress g_address = { "command-buffer" };
  static nacl::SocketAddress g_local_address = { "cb-client" };

  nacl::Handle sockets[2];
  nacl::SocketPair(sockets);

  nacl::MessageHeader msg;
  msg.iov = NULL;
  msg.iov_length = 0;
  msg.handles = &sockets[1];
  msg.handle_count = 1;
  nacl::Handle local_socket = nacl::BoundSocket(&g_local_address);
  nacl::SendDatagramTo(local_socket, &msg, 0, &g_address);
  nacl::Close(local_socket);
  nacl::Close(sockets[1]);
  handle = sockets[0];
#else
  if (argc < 3 || strcmp(argv[1], "-fd") != 0) {
    fprintf(stderr, "Usage: %s -fd file_descriptor\n", argv[0]);
    return nacl::kInvalidHtpHandle;
  }
  int fd = atoi(argv[2]);
  handle = imc_connect(fd);
  if (handle < 0) {
    fprintf(stderr, "Could not connect to file descriptor %d.\n"
            "Did you use the -a and -X options to sel_ldr ?\n", fd);
    return nacl::kInvalidHtpHandle;
  }
#endif
  return nacl::CreateImcDesc(handle);
}

void CloseConnection(nacl::HtpHandle handle) {
  nacl::Close(handle);
#ifndef __native_client__
  NaClNrdAllModulesFini();
#endif
}

int main(int argc, char **argv) {
  nacl::HtpHandle htp_handle = InitConnection(argc, argv);
  if (htp_handle == nacl::kInvalidHtpHandle) {
    return 1;
  }

  o3d::command_buffer::BigTestClient(htp_handle);
  CloseConnection(htp_handle);
  return 0;
}
