// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__
#define WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__

#include "webkit/glue/resource_loader_bridge.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_unittest.h"

using webkit_glue::ResourceLoaderBridge;

// We need to use ResourceLoaderBridge to communicate with the testserver
// instead of using URLRequest directly because URLRequests need to be run on
// the test_shell's IO thread.
class UnittestTestServer : public HTTPTestServer {
 protected:
  UnittestTestServer() {
  }

 public:
  static UnittestTestServer* CreateServer() {
    UnittestTestServer* test_server = new UnittestTestServer();
    FilePath no_cert;
    FilePath docroot = FilePath::FromWStringHack(L"webkit/data");
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        "localhost", 1337, docroot, no_cert)) {
      delete test_server;
      return NULL;
    }
    return test_server;
  }

  virtual ~UnittestTestServer() {
  }

  virtual bool MakeGETRequest(const std::string& page_name) {
    GURL url(TestServerPage(page_name));
    scoped_ptr<ResourceLoaderBridge> loader(
      ResourceLoaderBridge::Create("GET",
                                   url,
                                   url,            // policy_url
                                   GURL(),         // no referrer
                                   std::string(),  // no extra headers
                                   net::LOAD_NORMAL,
                                   0,
                                   ResourceType::SUB_RESOURCE,
                                   false,
                                   0));
    EXPECT_TRUE(loader.get());

    ResourceLoaderBridge::SyncLoadResponse resp;
    loader->SyncLoad(&resp);
    return resp.status.is_success();
  }
};

#endif  // WEBKIT_GLUE_UNITTEST_TEST_SERVER_H__
