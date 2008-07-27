// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
