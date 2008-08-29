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

#include "base/message_loop.h"
#include "base/test_suite.h"
#include "base/timer.h"

class NetTestSuite : public TestSuite {
 public:
  NetTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
  }

  virtual void Initialize() {
    TestSuite::Initialize();

    message_loop_.reset(new MessageLoopForIO());

    // TODO(darin): Remove this god awful, son of a wart toad hack.  This timer
    // keeps the MessageLoop pumping, which avoids a hang on Vista.  The real
    // fix lies elsewhere, but this is a stop-gap to keep the tests running on
    // Vista in the meantime.
    keep_looping_.Start(
        TimeDelta::FromMilliseconds(100), this, &NetTestSuite::DoNothing);
  }

  virtual void Shutdown() {
    // We want to destroy this here before the TestSuite continues to tear down
    // the environment.
    message_loop_.reset();

    TestSuite::Shutdown();
  }

 private:
  void DoNothing() {}

  scoped_ptr<MessageLoop> message_loop_;
  base::RepeatingTimer<NetTestSuite> keep_looping_;
};

int main(int argc, char** argv) {
  return NetTestSuite(argc, argv).Run();
}
