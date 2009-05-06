// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST_F(RenderViewTest, OnLoadAlternateHTMLText) {
  // Test a new navigation.
  GURL test_url("http://www.google.com/some_test_url");
  view_->OnLoadAlternateHTMLText("<html></html>", true, test_url,
                                 std::string());

  // We should have gotten two different types of start messages in the
  // following order.
  ASSERT_EQ((size_t)2, render_thread_.sink().message_count());
  const IPC::Message* msg = render_thread_.sink().GetMessageAt(0);
  EXPECT_EQ(ViewHostMsg_DidStartLoading::ID, msg->type());

  msg = render_thread_.sink().GetMessageAt(1);
  EXPECT_EQ(ViewHostMsg_DidStartProvisionalLoadForFrame::ID, msg->type());
  ViewHostMsg_DidStartProvisionalLoadForFrame::Param start_params;
  ViewHostMsg_DidStartProvisionalLoadForFrame::Read(msg, &start_params);
  EXPECT_EQ(GURL("chrome://chromewebdata/"), start_params.b);
}

// Test that we get form state change notifications when input fields change.
TEST_F(RenderViewTest, OnNavStateChanged) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  view_->set_delay_seconds_for_form_state_sync(0);

  LoadHTML("<input type=\"text\" id=\"elt_text\"></input>");

  // We should NOT have gotten a form state change notification yet.
  EXPECT_FALSE(render_thread_.sink().GetFirstMessageMatching(
      ViewHostMsg_UpdateState::ID));
  render_thread_.sink().ClearMessages();

  // Change the value of the input. We should have gotten an update state
  // notification. We need to spin the message loop to catch this update.
  ExecuteJavaScript("document.getElementById('elt_text').value = 'foo';");
  ProcessPendingMessages();
  EXPECT_TRUE(render_thread_.sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID));
}

// Test that our IME backend sends a notification message when the input focus
// changes.
TEST_F(RenderViewTest, OnImeStateChanged) {
  // Enable our IME backend code.
  view_->OnImeSetInputMode(true);

  // Load an HTML page consisting of two input fields.
  view_->set_delay_seconds_for_form_state_sync(0);
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<input id=\"test1\" type=\"text\"></input>"
           "<input id=\"test2\" type=\"password\"></input>"
           "</body>"
           "</html>");
  render_thread_.sink().ClearMessages();

  const int kRepeatCount = 10;
  for (int i = 0; i < kRepeatCount; i++) {
    // Move the input focus to the first <input> element, where we should
    // activate IMEs.
    ExecuteJavaScript("document.getElementById('test1').focus();");
    ProcessPendingMessages();
    render_thread_.sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to activate IMEs.
    view_->UpdateIME();
    const IPC::Message* msg = render_thread_.sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_ImeUpdateStatus::ID, msg->type());
    ViewHostMsg_ImeUpdateStatus::Param params;
    ViewHostMsg_ImeUpdateStatus::Read(msg, &params);
    EXPECT_EQ(params.a, IME_COMPLETE_COMPOSITION);
    EXPECT_TRUE(params.b.x() > 0 && params.b.y() > 0);

    // Move the input focus to the second <input> element, where we should
    // de-activate IMEs.
    ExecuteJavaScript("document.getElementById('test2').focus();");
    ProcessPendingMessages();
    render_thread_.sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to de-activate IMEs.
    view_->UpdateIME();
    msg = render_thread_.sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_ImeUpdateStatus::ID, msg->type());
    ViewHostMsg_ImeUpdateStatus::Read(msg, &params);
    EXPECT_EQ(params.a, IME_DISABLE);
  }
}

// Test that our IME backend can compose CJK words.
// Our IME front-end sends many platform-independent messages to the IME backend
// while it composes CJK words. This test sends the minimal messages captured
// on my local environment directly to the IME backend to verify if the backend
// can compose CJK words without any problems.
// This test uses an array of command sets because an IME composotion does not
// only depends on IME events, but also depends on window events, e.g. moving
// the window focus while composing a CJK text. To handle such complicated
// cases, this test should not only call IME-related functions in the
// RenderWidget class, but also call some RenderWidget members, e.g.
// ExecuteJavaScript(), RenderWidget::OnSetFocus(), etc.
TEST_F(RenderViewTest, ImeComposition) {
  enum ImeCommand {
    IME_INITIALIZE,
    IME_SETINPUTMODE,
    IME_SETFOCUS,
    IME_SETCOMPOSITION,
  };
  struct ImeMessage {
    ImeCommand command;
    bool enable;
    int string_type;
    int cursor_position;
    int target_start;
    int target_end;
    const wchar_t* ime_string;
    const wchar_t* result;
  };
  static const ImeMessage kImeMessages[] = {
    // Scenario 1: input a Chinese word with Microsoft IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 0, 1, -1, -1, L"n", L"n"},
    {IME_SETCOMPOSITION, false, 0, 2, -1, -1, L"ni", L"ni"},
    {IME_SETCOMPOSITION, false, 0, 3, -1, -1, L"nih", L"nih"},
    {IME_SETCOMPOSITION, false, 0, 4, -1, -1, L"niha", L"niha"},
    {IME_SETCOMPOSITION, false, 0, 5, -1, -1, L"nihao", L"nihao"},
    {IME_SETCOMPOSITION, false, 0, 2, -1, -1, L"\x4F60\x597D", L"\x4F60\x597D"},
    {IME_SETCOMPOSITION, false, 1, -1, -1, -1, L"\x4F60\x597D",
     L"\x4F60\x597D"},
    {IME_SETCOMPOSITION, false, -1, -1, -1, -1, L"", L"\x4F60\x597D"},
    // Scenario 2: input a Japanese word with Microsoft IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 0, 1, 0, 1, L"\xFF4B", L"\xFF4B"},
    {IME_SETCOMPOSITION, false, 0, 1, 0, 1, L"\x304B", L"\x304B"},
    {IME_SETCOMPOSITION, false, 0, 2, 0, 2, L"\x304B\xFF4E", L"\x304B\xFF4E"},
    {IME_SETCOMPOSITION, false, 0, 3, 0, 3, L"\x304B\x3093\xFF4A",
     L"\x304B\x3093\xFF4A"},
    {IME_SETCOMPOSITION, false, 0, 3, 0, 3, L"\x304B\x3093\x3058",
     L"\x304B\x3093\x3058"},
    {IME_SETCOMPOSITION, false, 0, 0, 0, 2, L"\x611F\x3058", L"\x611F\x3058"},
    {IME_SETCOMPOSITION, false, 0, 0, 0, 2, L"\x6F22\x5B57", L"\x6F22\x5B57"},
    {IME_SETCOMPOSITION, false, 1, -1, -1, -1, L"\x6F22\x5B57",
     L"\x6F22\x5B57"},
    {IME_SETCOMPOSITION, false, -1, -1, -1, -1, L"", L"\x6F22\x5B57"},
    // Scenario 3: input a Korean word with Microsot IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 0, 0, 0, 1, L"\x3147", L"\x3147"},
    {IME_SETCOMPOSITION, false, 0, 0, 0, 1, L"\xC544", L"\xC544"},
    {IME_SETCOMPOSITION, false, 0, 0, 0, 1, L"\xC548", L"\xC548"},
    {IME_SETCOMPOSITION, false, 1, -1, -1, -1, L"\xC548", L"\xC548"},
    {IME_SETCOMPOSITION, false, 0, 0, 0, 1, L"\x3134", L"\xC548\x3134"},
    {IME_SETCOMPOSITION, false, 0, 0, 0, 1, L"\xB140", L"\xC548\xB140"},
    {IME_SETCOMPOSITION, false, 0, 0, 0, 1, L"\xB155", L"\xC548\xB155"},
    {IME_SETCOMPOSITION, false, -1, -1, -1, -1, L"", L"\xC548"},
    {IME_SETCOMPOSITION, false, 1, -1, -1, -1, L"\xB155", L"\xC548\xB155"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kImeMessages); i++) {
    const ImeMessage* ime_message = &kImeMessages[i];
    switch (ime_message->command) {
      case IME_INITIALIZE:
        // Load an HTML page consisting of a content-editable <div> element,
        // and move the input focus to the <div> element, where we can use
        // IMEs.
        view_->OnImeSetInputMode(ime_message->enable);
        view_->set_delay_seconds_for_form_state_sync(0);
        LoadHTML("<html>"
                "<head>"
                "</head>"
                "<body>"
                "<div id=\"test1\" contenteditable=\"true\"></div>"
                "</body>"
                "</html>");
        ExecuteJavaScript("document.getElementById('test1').focus();");
        break;

      case IME_SETINPUTMODE:
        // Activate (or deactivate) our IME back-end.
        view_->OnImeSetInputMode(ime_message->enable);
        break;

      case IME_SETFOCUS:
        // Update the window focus.
        view_->OnSetFocus(ime_message->enable);
        break;

      case IME_SETCOMPOSITION:
        view_->OnImeSetComposition(ime_message->string_type,
                                   ime_message->cursor_position,
                                   ime_message->target_start,
                                   ime_message->target_end,
                                   ime_message->ime_string);
        break;
    }

    // Update the status of our IME back-end.
    // TODO(hbono): we should verify messages to be sent from the back-end.
    view_->UpdateIME();
    ProcessPendingMessages();
    render_thread_.sink().ClearMessages();

    if (ime_message->result) {
      // Retrieve the content of this page and compare it with the expected
      // result.
      const int kMaxOutputCharacters = 128;
      std::wstring output;
      GetMainFrame()->GetContentAsPlainText(kMaxOutputCharacters, &output);
      EXPECT_EQ(output, ime_message->result);
    }
  }
}

// Test that the RenderView::OnSetTextDirection() function can change the text
// direction of the selected input element.
TEST_F(RenderViewTest, OnSetTextDirection) {
  // Load an HTML page consisting of a <textarea> element and a <div> element.
  // This test changes the text direction of the <textarea> element, and
  // writes the values of its 'dir' attribute and its 'direction' property to
  // verify that the text direction is changed.
  view_->set_delay_seconds_for_form_state_sync(0);
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<textarea id=\"test\"></textarea>"
           "<div id=\"result\" contenteditable=\"true\"></div>"
           "</body>"
           "</html>");
  render_thread_.sink().ClearMessages();

  static const struct {
    WebTextDirection direction;
    const wchar_t* expected_result;
  } kTextDirection[] = {
    {WEB_TEXT_DIRECTION_RTL, L"\x000A" L"rtl,rtl"},
    {WEB_TEXT_DIRECTION_LTR, L"\x000A" L"ltr,ltr"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTextDirection); ++i) {
    // Set the text direction of the <textarea> element.
    ExecuteJavaScript("document.getElementById('test').focus();");
    view_->OnSetTextDirection(kTextDirection[i].direction);

    // Write the values of its DOM 'dir' attribute and its CSS 'direction'
    // property to the <div> element.
    ExecuteJavaScript("var result = document.getElementById('result');"
                      "var node = document.getElementById('test');"
                      "var style = getComputedStyle(node, null);"
                      "result.innerText ="
                      "    node.getAttribute('dir') + ',' +"
                      "    style.getPropertyValue('direction');");

    // Copy the document content to std::wstring and compare with the
    // expected result.
    const int kMaxOutputCharacters = 16;
    std::wstring output;
    GetMainFrame()->GetContentAsPlainText(kMaxOutputCharacters, &output);
    EXPECT_EQ(output, kTextDirection[i].expected_result);
  }
}

// Tests that printing pages work and sending and receiving messages through
// that channel all works.
TEST_F(RenderViewTest, OnPrintPages) {
#if defined(OS_WIN)
  // Lets simulate a print pages with Hello world.
  LoadHTML("<body><p>Hello World!</p></body>");
  view_->OnPrintPages();

  // The renderer should be done calculating the number of rendered pages
  // according to the specified settings defined in the mock render thread.
  // Verify the page count is correct.
  const IPC::Message* page_cnt_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_DidGetPrintedPagesCount::ID);
  EXPECT_TRUE(page_cnt_msg);
  ViewHostMsg_DidGetPrintedPagesCount::Param post_page_count_param;
  ViewHostMsg_DidGetPrintedPagesCount::Read(page_cnt_msg,
                                            &post_page_count_param);
  EXPECT_EQ(1, post_page_count_param.b);

  // Verify the rendered "printed page".
  const IPC::Message* did_print_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_DidPrintPage::ID);
  EXPECT_TRUE(did_print_msg);
  ViewHostMsg_DidPrintPage::Param post_did_print_page_param;
  ViewHostMsg_DidPrintPage::Read(did_print_msg, &post_did_print_page_param);
  EXPECT_EQ(0, post_did_print_page_param.page_number);
#else
  NOTIMPLEMENTED();
#endif
}

// Test that we can receive correct DOM events when we send input events
// through the RenderWidget::OnHandleInputEvent() function.
TEST_F(RenderViewTest, OnHandleKeyboardEvent) {
#if defined(OS_WIN)
  // Save the keyboard layout and the status.
  // This test changes the keyboard layout and status. This may break
  // succeeding tests. To prevent this possible break, we should save the
  // layout and status here to restore when this test is finished.
  HKL original_layout = GetKeyboardLayout(0);
  BYTE original_key_states[256];
  GetKeyboardState(&original_key_states[0]);

  // Load an HTML page consisting of one <input> element and three
  // contentediable <div> elements.
  // The <input> element is used for sending keyboard events, and the <div>
  // elements are used for writing DOM events in the following format:
  //   "<keyCode>,<shiftKey>,<controlKey>,<altKey>,<metaKey>".
  view_->set_delay_seconds_for_form_state_sync(0);
  LoadHTML("<html>"
           "<head>"
           "<title></title>"
           "<script type='text/javascript' language='javascript'>"
           "function OnKeyEvent(ev) {"
           "  var result = document.getElementById(ev.type);"
           "  result.innerText ="
           "      (ev.which || ev.keyCode) + ',' +"
           "      ev.shiftKey + ',' +"
           "      ev.ctrlKey + ',' +"
           "      ev.altKey + ',' +"
           "      ev.metaKey;"
           "  return true;"
           "}"
           "</script>"
           "</head>"
           "<body>"
           "<input id='test' type='text'"
           "    onkeydown='return OnKeyEvent(event);'"
           "    onkeypress='return OnKeyEvent(event);'"
           "    onkeyup='return OnKeyEvent(event);'>"
           "</input>"
           "<div id='keydown' contenteditable='true'>"
           "</div>"
           "<div id='keypress' contenteditable='true'>"
           "</div>"
           "<div id='keyup' contenteditable='true'>"
           "</div>"
           "</body>"
           "</html>");
  ExecuteJavaScript("document.getElementById('test').focus();");
  render_thread_.sink().ClearMessages();

  // Language IDs used in this test.
  // This test directly loads keyboard-layout drivers and use them for
  // emulating non-US keyboard layouts.
  static const wchar_t* kLanguageIDs[] = {
    L"00000401",  // Arabic
    L"00000409",  // United States
    L"0000040c",  // French
    L"0000040d",  // Hebrew
    L"00001009",  // Canadian French
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kLanguageIDs); ++i) {
    // Load a keyboard-layout driver.
    HKL handle = LoadKeyboardLayout(kLanguageIDs[i], KLF_ACTIVATE);
    EXPECT_TRUE(handle != NULL);

    // For each key code, we send two keyboard events: one when we only press
    // the key, and one when we press the key and a shift key.
    for (int modifiers = 0; modifiers < 2; ++modifiers) {
      // Virtual key codes used for this test.
      static const int kKeyCodes[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
        'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
        'W', 'X', 'Y', 'Z',
        VK_OEM_1,
        VK_OEM_PLUS,
        VK_OEM_COMMA,
        VK_OEM_MINUS,
        VK_OEM_PERIOD,
        VK_OEM_2,
        VK_OEM_3,
        VK_OEM_4,
        VK_OEM_5,
        VK_OEM_6,
        VK_OEM_7,
        VK_OEM_8,
      };

      // Over-write the keyboard status with our modifier-key status.
      // WebInputEventFactory::keyboardEvent() uses GetKeyState() to retrive
      // modifier-key status. So, we update the modifier-key status with this
      // SetKeyboardState() call before creating NativeWebKeyboardEvent
      // instances.
      BYTE key_states[256];
      memset(&key_states[0], 0, sizeof(key_states));
      key_states[VK_SHIFT] = (modifiers & 0x01) ? 0x80 : 0;
      SetKeyboardState(&key_states[0]);

      for (size_t j = 0; j <= ARRAYSIZE_UNSAFE(kKeyCodes); ++j) {
        // Retrieve the Unicode character composed from the virtual-key code
        // and our modifier-key status from the keyboard-layout driver.
        // This character is used for creating a WM_CHAR message and an
        // expected result.
        int key_code = kKeyCodes[j];
        wchar_t codes[4];
        int length = ToUnicodeEx(key_code, MapVirtualKey(key_code, 0),
                                 &key_states[0], &codes[0],
                                 ARRAYSIZE_UNSAFE(codes), 0, handle);
        if (length != 1)
          continue;

        // Create IPC messages from Windows messages and send them to our
        // back-end.
        // A keyboard event of Windows consists of three Windows messages:
        // WM_KEYDOWN, WM_CHAR, and WM_KEYUP.
        // WM_KEYDOWN and WM_KEYUP sends virtual-key codes. On the other hand,
        // WM_CHAR sends a composed Unicode character.
        NativeWebKeyboardEvent keydown_event(NULL, WM_KEYDOWN, key_code, 0);
        scoped_ptr<IPC::Message> keydown_message(
            new ViewMsg_HandleInputEvent(0));
        keydown_message->WriteData(
            reinterpret_cast<const char*>(&keydown_event),
            sizeof(WebKit::WebKeyboardEvent));
        view_->OnHandleInputEvent(*keydown_message);

        NativeWebKeyboardEvent char_event(NULL, WM_CHAR, codes[0], 0);
        scoped_ptr<IPC::Message> char_message(new ViewMsg_HandleInputEvent(0));
        char_message->WriteData(reinterpret_cast<const char*>(&char_event),
                                sizeof(WebKit::WebKeyboardEvent));
        view_->OnHandleInputEvent(*char_message);

        NativeWebKeyboardEvent keyup_event(NULL, WM_KEYUP, key_code, 0);
        scoped_ptr<IPC::Message> keyup_message(new ViewMsg_HandleInputEvent(0));
        keyup_message->WriteData(reinterpret_cast<const char*>(&keyup_event),
                                 sizeof(WebKit::WebKeyboardEvent));
        view_->OnHandleInputEvent(*keyup_message);

        // Create an expected result from the virtual-key code, the character
        // code, and the modifier-key status.
        // We format a string that emulates a DOM-event string produced hy
        // our JavaScript function. (See the above comment for the format.)
        static const wchar_t* kModifiers[] = {
          L"false,false,false,false",
          L"true,false,false,false",
        };
        static wchar_t expected_result[1024];
        wsprintf(&expected_result[0],
                 L"\x000A"       // texts in the <input> element
                 L"%d,%s\x000A"  // texts in the first <div> element
                 L"%d,%s\x000A"  // texts in the second <div> element
                 L"%d,%s",       // texts in the third <div> element
                 key_code, kModifiers[modifiers],
                 codes[0], kModifiers[modifiers],
                 key_code, kModifiers[modifiers]);

        // Retrieve the text in the test page and compare it with the expected
        // text created from a virtual-key code, a character code, and the
        // modifier-key status.
        const int kMaxOutputCharacters = 1024;
        std::wstring output;
        GetMainFrame()->GetContentAsPlainText(kMaxOutputCharacters, &output);

        EXPECT_EQ(expected_result, output);
      }
    }

    UnloadKeyboardLayout(handle);
  }

  // Restore the keyboard layout and status.
  SetKeyboardState(&original_key_states[0]);
  ActivateKeyboardLayout(original_layout, KLF_RESET);
#else
  NOTIMPLEMENTED();
#endif
}
