// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;

const wchar_t kSimplePage[] = L"files/devtools/simple_page.html";

class DevToolsSanityTest : public InProcessBrowserTest {
 public:
  DevToolsSanityTest() {
    set_show_window(true);
    EnableDOMAutomation();
  }

  void OpenWebInspector(const std::wstring& page_url) {
    HTTPTestServer* server = StartHTTPServer();
    GURL url = server->TestServerPageW(page_url);
    ui_test_utils::NavigateToURL(browser(), url);

    TabContents* tab = browser()->GetSelectedTabContents();
    RenderViewHost* inspected_rvh = tab->render_view_host();
    DevToolsManager* devtools_manager = g_browser_process->devtools_manager();
    devtools_manager->OpenDevToolsWindow(inspected_rvh);

    DevToolsClientHost* client_host =
        devtools_manager->GetDevToolsClientHostFor(inspected_rvh);
    DevToolsWindow* window = client_host->AsDevToolsWindow();
    RenderViewHost* client_rvh = window->GetRenderViewHost();
    client_contents_ = client_rvh->delegate()->GetAsTabContents();
    ui_test_utils::WaitForNavigation(&client_contents_->controller());
  }

  void AssertTrue(const std::string& expr) {
    AssertEquals("true", expr);
  }

  void AssertEquals(const std::string& expected, const std::string& expr) {
    std::string call = StringPrintf(
        "try {"
        "  var domAgent = devtools.tools.getDomAgent();"
        "  var netAgent = devtools.tools.getNetAgent();"
        "  var doc = domAgent.getDocument();"
        "  window.domAutomationController.send((%s) + '');"
        "} catch(e) {"
        "  window.domAutomationController.send(e.toString());"
        "}", expr.c_str());
    std::string result;
    ASSERT_TRUE(
        ui_test_utils::ExecuteJavaScriptAndExtractString(
            client_contents_,
            L"",
            UTF8ToWide(call),
            &result));
    ASSERT_EQ(expected, result);
  }

 protected:
  TabContents* client_contents_;
};

// WebInspector opens.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, OpenWebInspector) {
  OpenWebInspector(kSimplePage);
  AssertTrue("typeof DevToolsHost == 'object' && !DevToolsHost.isStub");
  AssertTrue("!!doc.documentElement");
}

// Tests elements panel basics.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, ElementsPanel) {
  OpenWebInspector(kSimplePage);
  AssertEquals("HTML", "doc.documentElement.nodeName");
  AssertTrue("doc.documentElement.hasChildNodes()");
}

// Tests resources panel basics.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, ResourcesPanel) {
  OpenWebInspector(kSimplePage);
  std::string func =
      "function() {"
      "  var tokens = [];"
      "  var resources = WebInspector.resources;"
      "  for (var id in resources) {"
      "    tokens.push(resources[id].lastPathComponent);"
      "  }"
      "  return tokens.join(',');"
      "}()";
  AssertEquals("simple_page.html", func);
}

}  // namespace
