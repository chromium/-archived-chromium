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

#include "chrome/common/chrome_counters.h"

#include "base/stats_counters.h"

namespace chrome {

// Note: We use the construct-on-first-use pattern here, because we don't
//       want to fight with any static initializer ordering problems later.
//       The downside of this is that the objects don't ever get cleaned up.
//       But they are small and this is okay.

// Note: Because these are constructed on-first-use, there is a slight
//       race condition - two threads could initialize the same counter.
//       If this happened, the stats table would still work just fine;
//       we'd leak the extraneous StatsCounter object once, and that
//       would be it.  But these are small objects, so this is ok.

StatsCounter& Counters::ipc_send_counter() {
  static StatsCounter* ctr = new StatsCounter(L"IPC.SendMsgCount");
  return *ctr;
}

StatsCounterTimer& Counters::chrome_main() {
  static StatsCounterTimer* ctr = new StatsCounterTimer(L"Chrome.Init");
  return *ctr;
}

StatsCounterTimer& Counters::renderer_main() {
  static StatsCounterTimer* ctr = new StatsCounterTimer(L"Chrome.RendererInit");
  return *ctr;
}

StatsCounterTimer& Counters::spellcheck_init() {
  static StatsCounterTimer* ctr = new StatsCounterTimer(L"SpellCheck.Init");
  return *ctr;
}

StatsRate& Counters::spellcheck_lookup() {
  static StatsRate* ctr = new StatsRate(L"SpellCheck.Lookup");
  return *ctr;
}

StatsCounterTimer& Counters::plugin_load() {
  static StatsCounterTimer* ctr = new StatsCounterTimer(L"ChromePlugin.Load");
  return *ctr;
}

StatsRate& Counters::plugin_intercept() {
  static StatsRate* ctr = new StatsRate(L"ChromePlugin.Intercept");
  return *ctr;
}

}  // namespace chrome
