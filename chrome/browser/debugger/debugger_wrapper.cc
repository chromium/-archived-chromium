// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_wrapper.h"

#include "chrome/browser/debugger/debugger_remote_service.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/debugger/devtools_remote_service.h"

DebuggerWrapper::DebuggerWrapper(int port) {
  if (port > 0) {
    proto_handler_ = new DevToolsProtocolHandler(port);
    proto_handler_->RegisterDestination(
        new DevToolsRemoteService(proto_handler_),
        DevToolsRemoteService::kToolName);
    proto_handler_->RegisterDestination(
        new DebuggerRemoteService(proto_handler_),
        DebuggerRemoteService::kToolName);
    proto_handler_->Start();
  }
}

DebuggerWrapper::~DebuggerWrapper() {
  if (proto_handler_.get() != NULL) {
    proto_handler_->Stop();
  }
}
