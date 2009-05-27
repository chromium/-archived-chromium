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


#include "core/cross/precompile.h"
#include "core/cross/error_stream_manager.h"
#include "base/logging.h"

namespace o3d {

#ifdef NDEBUG
ErrorStreamManager::ErrorStreamManager(ServiceLocator* service_locator) {
#else
ErrorStreamManager::ErrorStreamManager(ServiceLocator* service_locator,
                                       const char* file,
                                       int line)
    : file_(file),
      line_(line) {
#endif
  if (service_locator->IsAvailable<IErrorStatus>())
    error_status_ = service_locator->GetService<o3d::IErrorStatus>();
  else
    error_status_ = NULL;
}

ErrorStreamManager::~ErrorStreamManager() {
  if (error_status_ != NULL)
#ifdef NDEBUG
    error_status_->SetLastError(String(stream_.str()));
#else
    error_status_->SetLastError(String(stream_.str()), file_, line_);
#endif
}
}  // namespace o3d
