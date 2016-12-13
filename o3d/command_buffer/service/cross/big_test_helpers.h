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


// This file contains a few helper functions for running big tests.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_BIG_TEST_HELPERS_H__
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_BIG_TEST_HELPERS_H__

#include <build/build_config.h>
#include "core/cross/types.h"
#include "command_buffer/common/cross/gapi_interface.h"

namespace o3d {
namespace command_buffer {

extern String *g_program_path;
extern GAPIInterface *g_gapi;

// very simple thread API (hides platform-specific implementations).
typedef void (* ThreadFunc)(void *param);
class Thread;

// Creates and starts a thread.
Thread *CreateThread(ThreadFunc func, void* param);

// Joins (waits for) a thread, destroying it.
void JoinThread(Thread *thread);

// Processes system messages. Should be called once a frame at least.
bool ProcessSystemMessages();

}  // namespace command_buffer
}  // namespace o3d

#ifdef OS_WIN
int big_test_main(int argc, wchar_t **argv);
#else
int big_test_main(int argc, char **argv);
#endif

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_BIG_TEST_HELPERS_H__
