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
#include "base/ref_counted.h"
#include "base/test_suite.h"
#include "net/base/host_resolver_unittest.h"

class NetTestSuite : public TestSuite {
 public:
  NetTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
  }

  virtual void Initialize() {
    TestSuite::Initialize();

    host_mapper_ = new net::RuleBasedHostMapper();
    scoped_host_mapper_.Init(host_mapper_.get());
    // In case any attempts are made to resolve host names, force them all to
    // be mapped to localhost.  This prevents DNS queries from being sent in
    // the process of running these unit tests.
    host_mapper_->AddRule("*", "127.0.0.1");

    message_loop_.reset(new MessageLoopForIO());
  }

  virtual void Shutdown() {
    // We want to destroy this here before the TestSuite continues to tear down
    // the environment.
    message_loop_.reset();

    TestSuite::Shutdown();
  }

 private:
  scoped_ptr<MessageLoop> message_loop_;
  scoped_refptr<net::RuleBasedHostMapper> host_mapper_;
  net::ScopedHostMapper scoped_host_mapper_;
};

int main(int argc, char** argv) {
  return NetTestSuite(argc, argv).Run();
}
