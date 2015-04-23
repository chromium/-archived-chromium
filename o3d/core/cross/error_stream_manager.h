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


#ifndef O3D_CORE_CROSS_ERROR_STREAM_MANAGER_H_
#define O3D_CORE_CROSS_ERROR_STREAM_MANAGER_H_

#include <sstream>

#include "core/cross/ierror_status.h"

namespace o3d {

// We create a new ErrorStreamManager with each instantiation of the
// O3D_ERROR macro. That way we can call stuff based on the destruction
// of the manager.
class ErrorStreamManager {
 public:
  // Constructs an ErrorStreamManager which on destruction will copy the
  // contents of the stream to the client's error_string.
  // Parameters:
  //   client: The client whose error message to set.
#ifdef NDEBUG
  explicit ErrorStreamManager(ServiceLocator *service_locator);
#else
  ErrorStreamManager(ServiceLocator* service_locator,
                     const char* file,
                     int line);
#endif
  ~ErrorStreamManager();

  std::ostream& stream() {
    return stream_;
  }
 private:
  std::ostringstream stream_;
  IErrorStatus* error_status_;
#ifndef NDEBUG
  const char* file_;
  int line_;
#endif
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_ERROR_STREAM_MANAGER_H_
