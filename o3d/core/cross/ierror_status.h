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


#ifndef O3D_CORE_CROSS_IERROR_STATUS_H_
#define O3D_CORE_CROSS_IERROR_STATUS_H_

#include "core/cross/service_locator.h"
#include "core/cross/callback.h"
#include "core/cross/types.h"

namespace o3d {

// Records the last reported error. Allows a callback to be invoked when an
// error is reported.
class IErrorStatus {
 public:
  static const InterfaceId kInterfaceId;

  typedef Callback1<const String&> ErrorCallback;

  virtual ~IErrorStatus() {}

  // Sets the error callback. NOTE: The client takes ownership of the
  // ErrorCallback you pass in. It will be deleted if you call SetErrorCallback
  // a second time or if you call ClearErrorCallback.
  //
  // Parameters:
  //   error_callback: ErrorCallback to call each time the client gets
  //       an error.
  virtual void SetErrorCallback(ErrorCallback* error_callback) = 0;

  // Clears the Error callback NOTE: The client takes own ership of the
  // ErrorCallback you pass in to SetErrorCallback. It will be deleted if you
  // call SetErrorCallback a second time or if you call ClearErrorCallback.
  virtual void ClearErrorCallback() = 0;

  // Sets the last error. This is pretty much only called by ErrorStreamManager.
  virtual void SetLastError(const String& error) = 0;

#ifndef NDEBUG
  // For debug builds, we display where in the code the error came from.
  virtual void SetLastError(const String& error, const char *file,
      int line) = 0;
#endif

  // Gets the last reported error.
  virtual const String& GetLastError() const = 0;

  // Clears the stored last error.
  virtual void ClearLastError() = 0;

  // File logging is only ever done in a debug build, but can be turned off
  // there at will.
  virtual void SetFileLoggingActive(bool should_log) = 0;
  virtual bool IsFileLoggingActive() const = 0;

 protected:
  // Exchanges a new callback with the current callback, returing the old
  // one.
  // Parameters:
  //   callback: ErrorCallback to exchange.
  virtual ErrorCallback* Exchange(ErrorCallback* callback) = 0;

 private:
  friend class ErrorCollector;
  friend class ErrorSuppressor;
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_IERROR_STATUS_H_
