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


// This file declares the glue for FileRequest actions.

#ifndef O3D_PLUGIN_ARCHIVE_REQUEST_STATIC_GLUE_H_
#define O3D_PLUGIN_ARCHIVE_REQUEST_STATIC_GLUE_H_

#include "core/cross/callback.h"
#include "core/cross/types.h"

namespace o3d {
class ArchiveRequest;
}  // namespace o3d

namespace glue {
namespace namespace_o3d {
namespace class_ArchiveRequest {

using o3d::ArchiveRequest;
using o3d::String;

// Sets up the parameters required for all FileRequests.
void userglue_method_open(void *plugin_data,
                          ArchiveRequest *request,
                          const String &method,
                          const String &uri);

// Starts downloading or reading the requested file, passing in a callback that
// will parse and incorporate the file upon success.
void userglue_method_send(void *plugin_data,
                          ArchiveRequest *request);
}  // namespace class_FileRequest
}  // namespace namespace_o3d
}  // namespace glue

#endif  // O3D_PLUGIN_ARCHIVE_REQUEST_STATIC_GLUE_H_
