// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChildProcessSecurityPolicyTest : public testing::Test {
protected:
  // testing::Test
  virtual void SetUp() {
    // In the real world, "chrome:" is a handled scheme.
    URLRequest::RegisterProtocolFactory(chrome::kChromeUIScheme,
                                        &URLRequestTestJob::Factory);
  }
  virtual void TearDown() {
    URLRequest::RegisterProtocolFactory(chrome::kChromeUIScheme, NULL);
  }
};

static int kRendererID = 42;

TEST_F(ChildProcessSecurityPolicyTest, IsWebSafeSchemeTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  EXPECT_TRUE(p->IsWebSafeScheme("http"));
  EXPECT_TRUE(p->IsWebSafeScheme("https"));
  EXPECT_TRUE(p->IsWebSafeScheme("ftp"));
  EXPECT_TRUE(p->IsWebSafeScheme("data"));
  EXPECT_TRUE(p->IsWebSafeScheme("feed"));
  EXPECT_TRUE(p->IsWebSafeScheme("chrome-extension"));

  EXPECT_FALSE(p->IsWebSafeScheme("registered-web-safe-scheme"));
  p->RegisterWebSafeScheme("registered-web-safe-scheme");
  EXPECT_TRUE(p->IsWebSafeScheme("registered-web-safe-scheme"));
}

TEST_F(ChildProcessSecurityPolicyTest, IsPseudoSchemeTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  EXPECT_TRUE(p->IsPseudoScheme("about"));
  EXPECT_TRUE(p->IsPseudoScheme("javascript"));
  EXPECT_TRUE(p->IsPseudoScheme("view-source"));

  EXPECT_FALSE(p->IsPseudoScheme("registered-psuedo-scheme"));
  p->RegisterPseudoScheme("registered-psuedo-scheme");
  EXPECT_TRUE(p->IsPseudoScheme("registered-psuedo-scheme"));
}

TEST_F(ChildProcessSecurityPolicyTest, StandardSchemesTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  // Safe
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("http://www.google.com/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("https://www.paypal.com/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("ftp://ftp.gnu.org/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("data:text/html,<b>Hi</b>")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID,
                               GURL("view-source:http://www.google.com/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("chrome-extension://xy/z")));

  // Dangerous
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("view-cache:http://www.google.com/")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("chrome://foo/bar")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, AboutTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("about:blank")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("about:BlAnK")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("aBouT:blank")));

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:memory")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:cache")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:hang")));

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("aBoUt:memory")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:CrASh")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("abOuT:cAChe")));

  p->GrantRequestURL(kRendererID, GURL("about:memory"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:memory")));

  p->GrantRequestURL(kRendererID, GURL("about:crash"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:crash")));

  p->GrantRequestURL(kRendererID, GURL("about:cache"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:cache")));

  p->GrantRequestURL(kRendererID, GURL("about:hang"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:hang")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, JavaScriptTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("javascript:alert('xss')")));
  p->GrantRequestURL(kRendererID, GURL("javascript:alert('xss')"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("javascript:alert('xss')")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, RegisterWebSafeSchemeTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  // Currently, "asdf" is destined for ShellExecute, so it is allowed.
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));

  // Once we register a ProtocolFactory for "asdf", we default to deny.
  URLRequest::RegisterProtocolFactory("asdf", &URLRequestTestJob::Factory);
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));

  // We can allow new schemes by adding them to the whitelist.
  p->RegisterWebSafeScheme("asdf");
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));

  // Cleanup.
  URLRequest::RegisterProtocolFactory("asdf", NULL);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanServiceCommandsTest) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  p->GrantRequestURL(kRendererID, GURL("file:///etc/passwd"));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));

  // We should forget our state if we repeat a renderer id.
  p->Remove(kRendererID);
  p->Add(kRendererID);
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, ViewSource) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  // View source is determined by the embedded scheme.
  EXPECT_TRUE(p->CanRequestURL(kRendererID,
                               GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));

  p->GrantRequestURL(kRendererID, GURL("view-source:file:///etc/passwd"));
  // View source needs to be able to request the embedded scheme.
  EXPECT_TRUE(p->CanRequestURL(kRendererID,
                               GURL("view-source:file:///etc/passwd")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanUploadFiles) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanUploadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/passwd"))));
  p->GrantUploadFile(kRendererID, FilePath(FILE_PATH_LITERAL("/etc/passwd")));
  EXPECT_TRUE(p->CanUploadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/passwd"))));
  EXPECT_FALSE(p->CanUploadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/shadow"))));

  p->Remove(kRendererID);
  p->Add(kRendererID);

  EXPECT_FALSE(p->CanUploadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/passwd"))));
  EXPECT_FALSE(p->CanUploadFile(kRendererID,
      FilePath(FILE_PATH_LITERAL("/etc/shadow"))));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanServiceInspectElement) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  GURL url("chrome://inspector/inspector.html");

  p->Add(kRendererID);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
  p->GrantInspectElement(kRendererID);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanServiceDOMUIBindings) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  GURL url("chrome://thumb/http://www.google.com/");

  p->Add(kRendererID);

  EXPECT_FALSE(p->HasDOMUIBindings(kRendererID));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
  p->GrantDOMUIBindings(kRendererID);
  EXPECT_TRUE(p->HasDOMUIBindings(kRendererID));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, RemoveRace) {
  ChildProcessSecurityPolicy* p = ChildProcessSecurityPolicy::GetInstance();

  GURL url("file:///etc/passwd");
  FilePath file(FILE_PATH_LITERAL("/etc/passwd"));

  p->Add(kRendererID);

  p->GrantRequestURL(kRendererID, url);
  p->GrantUploadFile(kRendererID, file);
  p->GrantDOMUIBindings(kRendererID);

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url));
  EXPECT_TRUE(p->CanUploadFile(kRendererID, file));
  EXPECT_TRUE(p->HasDOMUIBindings(kRendererID));

  p->Remove(kRendererID);

  // Renderers are added and removed on the UI thread, but the policy can be
  // queried on the IO thread.  The ChildProcessSecurityPolicy needs to be prepared
  // to answer policy questions about renderers who no longer exist.

  // In this case, we default to secure behavior.
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
  EXPECT_FALSE(p->CanUploadFile(kRendererID, file));
  EXPECT_FALSE(p->HasDOMUIBindings(kRendererID));
}
