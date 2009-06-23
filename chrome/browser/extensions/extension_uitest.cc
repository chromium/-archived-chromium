// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/gfx/rect.h"
#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/values.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/automation/automation_proxy_uitest.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"

namespace {

static const char kTestDirectorySimpleApiCall[] =
    "extensions/uitest/simple_api_call";
static const char kTestDirectoryRoundtripApiCall[] =
    "extensions/uitest/roundtrip_api_call";
static const char kTestDirectoryBrowserEvent[] =
    "extensions/uitest/event_sink";

// Base class to test extensions almost end-to-end by including browser
// startup, manifest parsing, and the actual process model in the
// equation.  This would also let you write UITests that test individual
// Chrome Extensions as running in Chrome.  Takes over implementation of
// extension API calls so that behavior can be tested deterministically
// through code, instead of having to contort the browser into a state
// suitable for testing.
template <class ParentTestType>
class ExtensionUITest : public ParentTestType {
 public:
  explicit ExtensionUITest(const std::string& extension_path) {
    launch_arguments_.AppendSwitch(switches::kEnableExtensions);

    FilePath filename(test_data_directory_);
    filename = filename.AppendASCII(extension_path);
    launch_arguments_.AppendSwitchWithValue(switches::kLoadExtension,
                                            filename.value());
  }

  void SetUp() {
    ParentTestType::SetUp();
    automation()->SetEnableExtensionAutomation(true);
  }

  void TearDown() {
    automation()->SetEnableExtensionAutomation(false);
    ParentTestType::TearDown();
  }

  void TestWithURL(const GURL& url) {
    HWND external_tab_container = NULL;
    HWND tab_wnd = NULL;
    scoped_refptr<TabProxy> tab(automation()->CreateExternalTab(NULL,
        gfx::Rect(), WS_POPUP, false, &external_tab_container, &tab_wnd));
    ASSERT_TRUE(tab != NULL);
    ASSERT_NE(FALSE, ::IsWindow(external_tab_container));
    DoAdditionalPreNavigateSetup(tab.get());

    // We explicitly do not make this a toolstrip in the extension manifest,
    // so that the test can control when it gets loaded, and so that we test
    // the intended behavior that tabs should be able to show extension pages
    // (useful for development etc.)
    tab->NavigateInExternalTab(url);
    EXPECT_EQ(true, ExternalTabMessageLoop(external_tab_container, 5000));
    // Since the tab goes away lazily, wait a bit.
    PlatformThread::Sleep(1000);
    EXPECT_FALSE(tab->is_valid());
  }

  // Override if you need additional stuff before we navigate the page.
  virtual void DoAdditionalPreNavigateSetup(TabProxy* tab) {
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ExtensionUITest);
};

// For tests that only need to check for a single postMessage
// being received from the tab in Chrome.  These tests can send a message
// to the tab before receiving the new message, but there will not be
// a chance to respond by sending a message from the test to the tab after
// the postMessage is received.
typedef ExtensionUITest<ExternalTabTestType> SingleMessageExtensionUITest;

// A test that loads a basic extension that makes an API call that does
// not require a response.
class SimpleApiCallExtensionTest : public SingleMessageExtensionUITest {
 public:
  SimpleApiCallExtensionTest()
      : SingleMessageExtensionUITest(kTestDirectorySimpleApiCall) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleApiCallExtensionTest);
};

// TODO(port) Should become portable once ExternalTabMessageLoop is ported.
#if defined(OS_WIN)
TEST_F(SimpleApiCallExtensionTest, RunTest) {
  namespace keys = extension_automation_constants;

  TestWithURL(GURL(
      "chrome-extension://pmgpglkggjdpkpghhdmbdhababjpcohk/test.html"));
  AutomationProxyForExternalTab* proxy =
      static_cast<AutomationProxyForExternalTab*>(automation());
  ASSERT_GT(proxy->messages_received(), 0);

  // Using EXPECT_TRUE rather than EXPECT_EQ as the compiler (VC++) isn't
  // finding the right match for EqHelper.
  EXPECT_TRUE(proxy->origin() == keys::kAutomationOrigin);
  EXPECT_TRUE(proxy->target() == keys::kAutomationRequestTarget);

  scoped_ptr<Value> message_value(JSONReader::Read(proxy->message(), false));
  ASSERT_TRUE(message_value->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* message_dict =
      reinterpret_cast<DictionaryValue*>(message_value.get());
  std::string result;
  message_dict->GetString(keys::kAutomationNameKey, &result);
  EXPECT_EQ(result, "RemoveTab");

  result = "";
  message_dict->GetString(keys::kAutomationArgsKey, &result);
  EXPECT_NE(result, "");

  int callback_id = 0xBAADF00D;
  message_dict->GetInteger(keys::kAutomationRequestIdKey, &callback_id);
  EXPECT_NE(callback_id, 0xBAADF00D);

  bool has_callback = true;
  EXPECT_TRUE(message_dict->GetBoolean(keys::kAutomationHasCallbackKey,
                                       &has_callback));
  EXPECT_FALSE(has_callback);
}
#endif  // defined(OS_WIN)

// A base class for an automation proxy that checks several messages in
// a row.
class MultiMessageAutomationProxy : public AutomationProxyForExternalTab {
 public:
  explicit MultiMessageAutomationProxy(int execution_timeout)
      : AutomationProxyForExternalTab(execution_timeout) {
  }

  // Call when testing with the current tab is finished.
  void Quit() {
    PostQuitMessage(0);
  }

 protected:
  virtual void OnMessageReceived(const IPC::Message& msg) {
    IPC_BEGIN_MESSAGE_MAP(MultiMessageAutomationProxy, msg)
      IPC_MESSAGE_HANDLER(AutomationMsg_DidNavigate,
                          AutomationProxyForExternalTab::OnDidNavigate)
      IPC_MESSAGE_HANDLER(AutomationMsg_ForwardMessageToExternalHost,
                          OnForwardMessageToExternalHost)
    IPC_END_MESSAGE_MAP()
  }

  void OnForwardMessageToExternalHost(int handle,
                                      const std::string& message,
                                      const std::string& origin,
                                      const std::string& target) {
    messages_received_++;
    message_ = message;
    origin_ = origin;
    target_ = target;
    HandleMessageFromChrome();
  }

  // Override to do your custom checking and initiate any custom actions
  // needed in your particular unit test.
  virtual void HandleMessageFromChrome() = 0;
};

// This proxy is specific to RoundtripApiCallExtensionTest.
class RoundtripAutomationProxy : public MultiMessageAutomationProxy {
 public:
  explicit RoundtripAutomationProxy(int execution_timeout)
      : MultiMessageAutomationProxy(execution_timeout),
        tab_(NULL) {
  }

  // Must set before initiating test.
  TabProxy* tab_;

 protected:
  virtual void HandleMessageFromChrome() {
    namespace keys = extension_automation_constants;

    ASSERT_TRUE(tab_ != NULL);
    ASSERT_TRUE(messages_received_ == 1 || messages_received_ == 2);

    // Using EXPECT_TRUE rather than EXPECT_EQ as the compiler (VC++) isn't
    // finding the right match for EqHelper.
    EXPECT_TRUE(origin_ == keys::kAutomationOrigin);
    EXPECT_TRUE(target_ == keys::kAutomationRequestTarget);

    scoped_ptr<Value> message_value(JSONReader::Read(message_, false));
    ASSERT_TRUE(message_value->IsType(Value::TYPE_DICTIONARY));
    DictionaryValue* request_dict =
        static_cast<DictionaryValue*>(message_value.get());
    std::string function_name;
    ASSERT_TRUE(request_dict->GetString(keys::kAutomationNameKey,
                                        &function_name));
    int request_id = -2;
    EXPECT_TRUE(request_dict->GetInteger(keys::kAutomationRequestIdKey,
                                         &request_id));
    bool has_callback = false;
    EXPECT_TRUE(request_dict->GetBoolean(keys::kAutomationHasCallbackKey,
                                         &has_callback));

    if (messages_received_ == 1) {
      EXPECT_EQ(function_name, "GetLastFocusedWindow");
      EXPECT_GE(request_id, 0);
      EXPECT_TRUE(has_callback);

      DictionaryValue response_dict;
      EXPECT_TRUE(response_dict.SetInteger(keys::kAutomationRequestIdKey,
                                           request_id));
      EXPECT_TRUE(response_dict.SetString(keys::kAutomationResponseKey, "42"));

      std::string response_json;
      JSONWriter::Write(&response_dict, false, &response_json);

      tab_->HandleMessageFromExternalHost(
          response_json,
          keys::kAutomationOrigin,
          keys::kAutomationResponseTarget);
    } else if (messages_received_ == 2) {
      EXPECT_EQ(function_name, "RemoveTab");
      EXPECT_FALSE(has_callback);

      std::string args;
      EXPECT_TRUE(request_dict->GetString(keys::kAutomationArgsKey, &args));
      EXPECT_NE(args.find("42"), -1);

      Quit();
    } else {
      Quit();
      FAIL();
    }
  }
};

class RoundtripApiCallExtensionTest
    : public ExtensionUITest<
                 CustomAutomationProxyTest<RoundtripAutomationProxy>> {
 public:
  RoundtripApiCallExtensionTest()
      : ExtensionUITest<
          CustomAutomationProxyTest<
              RoundtripAutomationProxy> >(kTestDirectoryRoundtripApiCall) {
  }

  void DoAdditionalPreNavigateSetup(TabProxy* tab) {
    RoundtripAutomationProxy* proxy =
      static_cast<RoundtripAutomationProxy*>(automation());
    proxy->tab_ = tab;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RoundtripApiCallExtensionTest);
};

// TODO(port) Should become portable once
// ExternalTabMessageLoop is ported.
#if defined(OS_WIN)
TEST_F(RoundtripApiCallExtensionTest, RunTest) {
  TestWithURL(GURL(
      "chrome-extension://ofoknjclcmghjfmbncljcnpjmfmldhno/test.html"));
  RoundtripAutomationProxy* proxy =
      static_cast<RoundtripAutomationProxy*>(automation());

  // Validation is done in the RoundtripAutomationProxy, so we just check
  // something basic here.
  EXPECT_EQ(proxy->messages_received(), 2);
}
#endif  // defined(OS_WIN)

// This proxy is specific to BrowserEventExtensionTest.
class BrowserEventAutomationProxy : public MultiMessageAutomationProxy {
 public:
  explicit BrowserEventAutomationProxy(int execution_timeout)
      : MultiMessageAutomationProxy(execution_timeout),
        tab_(NULL) {
  }

  // Must set before initiating test.
  TabProxy* tab_;

  // Counts the number of times we got a given event.
  std::map<std::string, int> event_count_;

  // Array containing the names of the events to fire to the extension.
  static const char* event_names_[];

 protected:
  // Process a message received from the test extension.
  virtual void HandleMessageFromChrome();

  // Fire an event of the given name to the test extension.
  void FireEvent(const char* event_name);
};

const char* BrowserEventAutomationProxy::event_names_[] = {
  // Window events.
  "window-created",
  "window-removed",
  "window-focus-changed",

  // Tab events.
  "tab-created",
  "tab-updated",
  "tab-moved",
  "tab-selection-changed",
  "tab-attached",
  "tab-detached",
  "tab-removed",

  // Page action events.
  "page-action-executed",

  // Bookmark events.
  "bookmark-added",
  "bookmark-removed",
  "bookmark-changed",
  "bookmark-moved",
  "bookmark-children-reordered",
};

void BrowserEventAutomationProxy::HandleMessageFromChrome() {
  namespace keys = extension_automation_constants;
  ASSERT_TRUE(tab_ != NULL);

  std::string message(message());
  std::string origin(origin());
  std::string target(target());

  ASSERT_TRUE(message.length() > 0);
  ASSERT_STREQ(keys::kAutomationOrigin, origin.c_str());

  if (target == keys::kAutomationRequestTarget) {
    // This should be a request for the current window.  We don't need to
    // respond, as this is used only as an indication that the extension
    // page is now loaded.
    scoped_ptr<Value> message_value(JSONReader::Read(message, false));
    ASSERT_TRUE(message_value->IsType(Value::TYPE_DICTIONARY));
    DictionaryValue* message_dict =
        reinterpret_cast<DictionaryValue*>(message_value.get());

    std::string name;
    message_dict->GetString(keys::kAutomationNameKey, &name);
    ASSERT_STREQ(name.c_str(), "GetCurrentWindow");

    // Send an OpenChannelToExtension message to chrome. Note: the JSON
    // reader expects quoted property keys.  See the comment in
    // TEST_F(BrowserEventExtensionTest, RunTest) to understand where the
    // extension Id comes from.
    tab_->HandleMessageFromExternalHost(
        "{\"rqid\":0, \"extid\": \"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
        " \"connid\": 1}",
        keys::kAutomationOrigin,
        keys::kAutomationPortRequestTarget);
  } else if (target == keys::kAutomationPortResponseTarget) {
    // This is a response to the open channel request.  This means we know
    // that the port is ready to send us messages.  Fire all the events now.
    for (int i = 0; i < arraysize(event_names_); ++i) {
      FireEvent(event_names_[i]);
    }
  } else if (target == keys::kAutomationPortRequestTarget) {
    // This is the test extension calling us back.  Make sure its telling
    // us that it received an event.  We do this by checking to see if the
    // message is a simple string of one of the event names that is fired.
    //
    // There is a special message "ACK" which means that the extension
    // received the port connection.  This is not an event response and
    // should happen before all events.
    scoped_ptr<Value> message_value(JSONReader::Read(message, false));
    ASSERT_TRUE(message_value->IsType(Value::TYPE_DICTIONARY));
    DictionaryValue* message_dict =
        reinterpret_cast<DictionaryValue*>(message_value.get());

    std::string event_name;
    message_dict->GetString(L"data", &event_name);
    if (event_name == "\"ACK\"") {
      ASSERT_EQ(0, event_count_.size());
    } else {
      ++event_count_[event_name];
    }
  }
}

void BrowserEventAutomationProxy::FireEvent(const char* event_name) {
  namespace keys = extension_automation_constants;

  // Build the event message to send to the extension.  The only important
  // part is the name, as the payload is not used by the test extension.
  std::string message;
  message += "[\"";
  message += event_name;
  message += "\", \"[]\"]";

  tab_->HandleMessageFromExternalHost(
      message,
      keys::kAutomationOrigin,
      keys::kAutomationBrowserEventRequestTarget);
}

class BrowserEventExtensionTest
    : public ExtensionUITest<
                 CustomAutomationProxyTest<BrowserEventAutomationProxy>> {
 public:
  BrowserEventExtensionTest()
      : ExtensionUITest<
          CustomAutomationProxyTest<
              BrowserEventAutomationProxy> >(kTestDirectoryBrowserEvent) {
  }

  void DoAdditionalPreNavigateSetup(TabProxy* tab) {
    BrowserEventAutomationProxy* proxy =
      static_cast<BrowserEventAutomationProxy*>(automation());
    proxy->tab_ = tab;
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(BrowserEventExtensionTest);
};

// TODO(port) Should become portable once
// ExternalTabMessageLoop is ported.
#if defined(OS_WIN)
TEST_F(BrowserEventExtensionTest, RunTest) {
  // The extension for this test does not specify a "key" property in its
  // manifest file.  Therefore, the extension system will automatically assign
  // it an Id.  To make this test consistent and non-flaky, the genetated Id
  // counter is reset before the test so that we can hardcode the first Id
  // that will be generated.
  Extension::ResetGeneratedIdCounter();
  TestWithURL(GURL(
      "chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/test.html"));
  BrowserEventAutomationProxy* proxy =
      static_cast<BrowserEventAutomationProxy*>(automation());

  EXPECT_EQ(arraysize(BrowserEventAutomationProxy::event_names_),
            proxy->event_count_.size());
  for (std::map<std::string, int>::iterator i = proxy->event_count_.begin();
      i != proxy->event_count_.end(); ++i) {
    const std::pair<std::string, int>& value = *i;
    ASSERT_EQ(1, value.second);
  }
}
#endif  // defined(OS_WIN)

}  // namespace
