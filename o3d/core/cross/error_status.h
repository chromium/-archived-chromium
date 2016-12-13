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


#ifndef O3D_CORE_CROSS_ERROR_STATUS_H_
#define O3D_CORE_CROSS_ERROR_STATUS_H_

#include "core/cross/service_implementation.h"
#include "core/cross/service_dependency.h"
#include "core/cross/ierror_status.h"

namespace o3d {

// Records the last reported error. Allows a callback to be invoked when an
// error is reported.
class ErrorStatus : public IErrorStatus {
 public:
  explicit ErrorStatus(ServiceLocator* service_locator_);

  // Sets the error callback. NOTE: The client takes ownership of the
  // ErrorCallback you pass in. It will be deleted if you call SetErrorCallback
  // a second time or if you call ClearErrorCallback.
  //
  // Parameters:
  //   error_callback: ErrorCallback to call each time the client gets
  //       an error.
  void SetErrorCallback(ErrorCallback* error_callback);

  // Clears the Error callback NOTE: The client takes ownership of the
  // ErrorCallback you pass in to SetErrorCallback. It will be deleted if you
  // call SetErrorCallback a second time or if you call ClearErrorCallback.
  virtual void ClearErrorCallback();

  // Sets the last error. This is pretty much only called by ErrorStreamManager.
  virtual void SetLastError(const String& error);

#ifndef NDEBUG
  // For debug builds, we display where in the code the error came from.
  virtual void SetLastError(const String& error, const char *file, int line);
#endif

  // Gets the last reported error.
  virtual const String& GetLastError() const;

  // Clears the stored last error.
  virtual void ClearLastError();

  // File logging is only ever done in a debug build.
  void SetFileLoggingActive(bool should_log) {
    log_to_file_ = should_log;
  }
  bool IsFileLoggingActive() const {
    return log_to_file_;
  }

 protected:
  // Exchanges a new callback with the current callback, returing the old
  // one.
  // Parameters:
  //   callback: ErrorCallback to exchange.
  virtual ErrorCallback* Exchange(ErrorCallback* callback);

 private:
  typedef NonRecursiveCallback1Manager<const o3d::String&>
      ErrorCallbackManager;

  ServiceImplementation<IErrorStatus> service_;
  ErrorCallbackManager error_callback_manager_;
  String error_string_;
  bool log_to_file_;
};

// This class temporarily replaces the error callback on the ErrorStatus
// service. It restores it when destroyed.
// It should be used like this.
//
// { // some scope
//   ErrorCollector error_collector(service_locator);
//
//   .. call some stuff that might generate an error.
//
//   String errors = error_collector.errors();
//
// }  // end of scope, old callback has been restored.
class ErrorCollector : public IErrorStatus::ErrorCallback {
 public:
  explicit ErrorCollector(ServiceLocator* service_locator);
  ~ErrorCollector();

  // Appends the error to the errors being collected.
  virtual void Run(const String& error);

  // Gets the collectd errors.
  const String& errors() const {
    return errors_;
  }

 private:
  IErrorStatus* error_status_;
  IErrorStatus::ErrorCallback* old_callback_;
  String errors_;

  DISALLOW_COPY_AND_ASSIGN(ErrorCollector);
};

// This class temporarily replaces the error callback on the ErrorStatus
// service. It restores it when destroyed.  It's similar to the ErrorCollector,
// but it throws away all errors instead of collecting them.  It also suppresses
// debugging log output temporarily.
//
// It should be used like this.
//
// { // some scope
//   ErrorSuppressor error_suppressor(service_locator);
//
//   .. call some stuff that might generate an error.
//
// }  // end of scope, old callback has been restored.
class ErrorSuppressor : public IErrorStatus::ErrorCallback {
 public:
  explicit ErrorSuppressor(ServiceLocator* service_locator);
  ~ErrorSuppressor();

  virtual void Run(const String& error) {}

 private:
  IErrorStatus* error_status_;
  IErrorStatus::ErrorCallback* old_callback_;
  bool old_file_logging_;

  DISALLOW_COPY_AND_ASSIGN(ErrorSuppressor);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_ERROR_STATUS_H_
