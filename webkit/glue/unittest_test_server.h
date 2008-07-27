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

#ifndef WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__
#define WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__

#include "webkit/glue/resource_loader_bridge.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_unittest.h"

using webkit_glue::ResourceLoaderBridge;

// We need to use ResourceLoaderBridge to communicate with the testserver
// instead of using URLRequest directly because URLRequests need to be run on
// the test_shell's IO thread.
class UnittestTestServer : public TestServer {
 public:
  UnittestTestServer() : TestServer(TestServer::ManualInit()) {
    Init("localhost", 1337, L"webkit/data", std::wstring());
  }

  ~UnittestTestServer() {
    Shutdown();
  }

  virtual bool MakeGETRequest(const std::string& page_name) {
    GURL url(TestServerPage(page_name));
    scoped_ptr<ResourceLoaderBridge> loader(
      ResourceLoaderBridge::Create(NULL, "GET",
                                   url,
                                   url,            // policy_url
                                   GURL(),         // no referrer
                                   std::string(),  // no extra headers
                                   net::LOAD_NORMAL,
                                   0,
                                   ResourceType::SUB_RESOURCE,
                                   false));
    EXPECT_TRUE(loader.get());

    ResourceLoaderBridge::SyncLoadResponse resp;
    loader->SyncLoad(&resp);
    return resp.status.is_success();
  }
};

#endif  // WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__
