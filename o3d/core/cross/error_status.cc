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
#include "core/cross/error_status.h"

namespace o3d {

ErrorStatus::ErrorStatus(ServiceLocator* service_locator)
    : service_(service_locator, this), log_to_file_(true) {}

void ErrorStatus::SetErrorCallback(ErrorCallback* error_callback) {
  error_callback_manager_.Set(error_callback);
}

void ErrorStatus::ClearErrorCallback() {
  error_callback_manager_.Clear();
}

IErrorStatus::ErrorCallback* ErrorStatus::Exchange(ErrorCallback* callback) {
  return error_callback_manager_.Exchange(callback);
}

#ifndef NDEBUG
void ErrorStatus::SetLastError(const String& error, const char *file,
    int line) {
  if (log_to_file_) {
    logging::LogMessage(file, line, ERROR).stream() << error;
  }
  SetLastError(error);
}
#endif

void ErrorStatus::SetLastError(const String& error) {
  error_string_ = error;
  error_callback_manager_.Run(error_string_);
}

const String& ErrorStatus::GetLastError() const {
  return error_string_;
}

void ErrorStatus::ClearLastError() {
  error_string_.clear();
}

ErrorCollector::ErrorCollector(ServiceLocator* service_locator)
    : error_status_(service_locator->GetService<IErrorStatus>()) {
  old_callback_ = error_status_->Exchange(this);
}

ErrorCollector::~ErrorCollector() {
  error_status_->Exchange(old_callback_);
}

void ErrorCollector::Run(const String& error) {
  errors_ += (errors_.empty() ? String("") : String("\n")) + error;
}

ErrorSuppressor::ErrorSuppressor(ServiceLocator* service_locator)
    : error_status_(service_locator->GetService<IErrorStatus>()) {
  old_callback_ = error_status_->Exchange(this);
  old_file_logging_ = error_status_->IsFileLoggingActive();
  error_status_->SetFileLoggingActive(false);
}

ErrorSuppressor::~ErrorSuppressor() {
  error_status_->Exchange(old_callback_);
  error_status_->SetFileLoggingActive(old_file_logging_);
}

}  // namespace o3d
