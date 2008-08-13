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

#include "base/at_exit.h"
#include "base/logging.h"

namespace base {

// Keep a stack of registered AtExitManagers.  We always operate on the most
// recent, and we should never have more than one outside of testing, when we
// use the shadow version of the constructor.  We don't protect this for
// thread-safe access, since it will only be modified in testing.
static AtExitManager* g_top_manager = NULL;

AtExitManager::AtExitManager() : next_manager_(NULL) {
  DCHECK(!g_top_manager);
  g_top_manager = this;
}

AtExitManager::AtExitManager(bool shadow) : next_manager_(g_top_manager) {
  DCHECK(shadow || !g_top_manager);
  g_top_manager = this;
}

AtExitManager::~AtExitManager() {
  if (!g_top_manager) {
    NOTREACHED() << "Tried to ~AtExitManager without an AtExitManager";
    return;
  }
  DCHECK(g_top_manager == this);

  ProcessCallbacksNow();
  g_top_manager = next_manager_;
}

// static
void AtExitManager::RegisterCallback(AtExitCallbackType func) {
  if (!g_top_manager) {
    NOTREACHED() << "Tried to RegisterCallback without an AtExitManager";
    return;
  }

  AutoLock lock(g_top_manager->lock_);
  g_top_manager->stack_.push(func);
}

// static
void AtExitManager::ProcessCallbacksNow() {
  if (!g_top_manager) {
    NOTREACHED() << "Tried to ProcessCallbacksNow without an AtExitManager";
    return;
  }

  AutoLock lock(g_top_manager->lock_);

  while (!g_top_manager->stack_.empty()) {
    AtExitCallbackType func = g_top_manager->stack_.top();
    g_top_manager->stack_.pop();
    if (func)
      func();
  }
}

}  // namespace base
