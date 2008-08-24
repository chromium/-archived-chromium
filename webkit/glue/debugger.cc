// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"  // webkit config for V8_BINDING
#include "base/string_util.h"
#include "webkit/glue/debugger.h"

#if USE(V8_BINDING)
#define USING_V8
#include "v8/public/debug.h"
#endif

void V8DebugMessageHandler(const uint16_t* message, int length, void* data) {
  std::wstring out(reinterpret_cast<const wchar_t*>(message), length);
  reinterpret_cast<Debugger*>(data)->OutputLater(out);
}

Debugger::Debugger(Delegate* del) : delegate_(del), attached_(false) {
  delegate_loop_ = MessageLoop::current();
}

Debugger::~Debugger() {
  DCHECK(!attached_);
  Detach();
}

void Debugger::Attach() {
#ifdef USING_V8
  if (!attached_) {
    attached_ = true;
    v8::Debug::SetMessageHandler(V8DebugMessageHandler, this);
  }
#endif
}

void Debugger::Detach() {
#ifdef USING_V8
  if (attached_) {
    attached_ = false;
    v8::Debug::SetMessageHandler(NULL);
  }
#endif
}

void Debugger::OutputLater(const std::wstring& out) {
  delegate_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &Debugger::Output, out));
}

void Debugger::Output(const std::wstring& out) {
  delegate_->DebuggerOutput(out);
}

void Debugger::Command(const std::wstring& cmd) {
#ifdef USING_V8
  DCHECK(attached_);
  v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd.data()),
                         cmd.length());
#endif
}

