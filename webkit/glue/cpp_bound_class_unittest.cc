// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for CppBoundClass, in conjunction with CppBindingExample.  Binds
// a CppBindingExample class into JavaScript in a custom test shell and tests
// the binding from the outside by loading JS into the shell.

#include <vector>

#include "base/message_loop.h"
#include "webkit/glue/cpp_binding_example.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

class CppBindingExampleSubObject : public CppBindingExample {
 public:
  CppBindingExampleSubObject() {
    sub_value_.Set("sub!");
    BindProperty("sub_value", &sub_value_);
  }
 private:
  CppVariant sub_value_;
};


class CppBindingExampleWithOptionalFallback : public CppBindingExample {
 public:
  CppBindingExampleWithOptionalFallback() {
    BindProperty("sub_object", sub_object_.GetAsCppVariant());
  }

  void set_fallback_method_enabled(bool state) {
    BindFallbackMethod(state ?
        &CppBindingExampleWithOptionalFallback::fallbackMethod
        : NULL);
  }

  // The fallback method does nothing, but because of it the JavaScript keeps
  // running when a nonexistent method is called on an object.
  void fallbackMethod(const CppArgumentList& args, CppVariant* result) {
  }

 private:
  CppBindingExampleSubObject sub_object_;
};

class ExampleTestShell : public TestShell {
 public:

  ExampleTestShell(bool use_fallback_method) {
    example_bound_class_.set_fallback_method_enabled(use_fallback_method);
  }

  // When called by WebViewDelegate::WindowObjectCleared method, this binds a
  // CppExampleObject to window.example.
  virtual void BindJSObjectsToWindow(WebFrame* frame) {
    example_bound_class_.BindToJavascript(frame, L"example");
    // We use the layoutTestController binding for notifyDone.
    TestShell::BindJSObjectsToWindow(frame);
  }
  
  // This is a public interface to TestShell's protected method, so it 
  // can be called by our CreateEmptyWindow.
  bool PublicInitialize(const std::wstring& startingURL) {
    return Initialize(startingURL);
  }

  CppBindingExampleWithOptionalFallback example_bound_class_;
};

class CppBoundClassTest : public TestShellTest {
 protected:
   // Adapted from TestShell::CreateNewWindow, this creates an
   // ExampleTestShellWindow rather than a regular TestShell.
   virtual void CreateEmptyWindow() {
     ExampleTestShell* host = new ExampleTestShell(useFallback());
     ASSERT_TRUE(host != NULL);
     bool rv = host->PublicInitialize(L"about:blank");
     if (rv) {
       test_shell_ = host;
       TestShell::windowList()->push_back(host->mainWnd());
       webframe_ = test_shell_->webView()->GetMainFrame();
       ASSERT_TRUE(webframe_ != NULL);
     } else {
       delete host;
     }
   }

   // Wraps the given JavaScript snippet in <html><body><script> tags, then
   // loads it into a webframe so it is executed.
   void ExecuteJavaScript(const std::string& javascript) {
     std::string html = "<html><body>";
     html.append(TestShellTest::kJavascriptDelayExitScript);
     html.append("<script>");
     html.append(javascript);
     html.append("</script></body></html>");
     // The base URL doesn't matter.
     webframe_->LoadHTMLString(html, GURL("about:blank"));

     test_shell_->WaitTestFinished();
    }

   // Executes the specified JavaScript and checks to be sure that the resulting
   // document text is exactly "SUCCESS".
   void CheckJavaScriptSuccess(const std::string& javascript) {
     ExecuteJavaScript(javascript);
     EXPECT_EQ(L"SUCCESS", webkit_glue::DumpDocumentText(webframe_));
   }
   
   // Executes the specified JavaScript and checks that the resulting document
   // text is empty.
   void CheckJavaScriptFailure(const std::string& javascript) {
     ExecuteJavaScript(javascript);
     EXPECT_EQ(L"", webkit_glue::DumpDocumentText(webframe_));
   }

   // Constructs a JavaScript snippet that evaluates and compares the left and
   // right expressions, printing "SUCCESS" to the page if they are equal and
   // printing their actual values if they are not.  Any strings in the
   // expressions should be enclosed in single quotes, and no double quotes
   // should appear in either expression (even if escaped). (If a test case
   // is added that needs fancier quoting, Json::valueToQuotedString could be
   // used here.  For now, it's not worth adding the dependency.)
   std::string BuildJSCondition(std::string left, std::string right) {
     return "var leftval = " + left + ";" +
            "var rightval = " + right + ";" +
            "if (leftval == rightval) {" +
            "  document.writeln('SUCCESS');" +
            "} else {" +
            "  document.writeln(\"" + 
                 left + " [\" + leftval + \"] != " + 
                 right + " [\" + rightval + \"]\");" +
            "}";
   }

protected:
  virtual bool useFallback() {
    return false;
  }

private:
  WebFrame* webframe_;
};

class CppBoundClassWithFallbackMethodTest : public CppBoundClassTest {
protected:
  virtual bool useFallback() {
    return true;
  }
};

// Ensures that the example object has been bound to JS.
TEST_F(CppBoundClassTest, ObjectExists) {
  std::string js = BuildJSCondition("typeof window.example", "'object'");
  CheckJavaScriptSuccess(js);

  // An additional check to test our test.
  js = BuildJSCondition("typeof window.invalid_object", "'undefined'");
  CheckJavaScriptSuccess(js);
}

TEST_F(CppBoundClassTest, PropertiesAreInitialized) {
  std::string js = BuildJSCondition("example.my_value", "10");
  CheckJavaScriptSuccess(js);

  js = BuildJSCondition("example.my_other_value", "'Reinitialized!'");
  CheckJavaScriptSuccess(js);
}

TEST_F(CppBoundClassTest, SubOject) {
  std::string js = BuildJSCondition("typeof window.example.sub_object",
                                    "'object'");
  CheckJavaScriptSuccess(js);

  js = BuildJSCondition("example.sub_object.sub_value", "'sub!'");
  CheckJavaScriptSuccess(js);
}

TEST_F(CppBoundClassTest, SetAndGetProperties) {
  // The property on the left will be set to the value on the right, then
  // checked to make sure it holds that same value.
  static const std::string tests[] = {
    "example.my_value", "7",
    "example.my_value", "'test'",
    "example.my_other_value", "3.14",
    "example.my_other_value", "false",
    "" // Array end marker: insert additional test pairs before this.
  };

  for (int i = 0; tests[i] != ""; i += 2) {
    std::string left = tests[i];
    std::string right = tests[i + 1];
    // left = right;
    std::string js = left;
    js.append(" = ");
    js.append(right);
    js.append(";");
    js.append(BuildJSCondition(left, right));
    CheckJavaScriptSuccess(js);
  }
}

TEST_F(CppBoundClassTest, InvokeMethods) {
  // The expression on the left is expected to return the value on the right.
  static const std::string tests[] = {
    "example.echoValue(true)", "true",
    "example.echoValue(13)", "13",
    "example.echoValue(2.718)", "2.718",
    "example.echoValue('yes')", "'yes'",
    "example.echoValue()", "null",     // Too few arguments

    "example.echoType(false)", "true",
    "example.echoType(19)", "7",
    "example.echoType(9.876)", "3.14159",
    "example.echoType('test string')", "'Success!'",
    "example.echoType()", "null",      // Too few arguments

    // Comparing floats that aren't integer-valued is usually problematic due
    // to rounding, but exact powers of 2 should also be safe.
    "example.plus(2.5, 18.0)", "20.5",
    "example.plus(2, 3.25)", "5.25",
    "example.plus(2, 3)", "5",
    "example.plus()", "null",             // Too few arguments
    "example.plus(1)", "null",            // Too few arguments
    "example.plus(1, 'test')", "null",    // Wrong argument type
    "example.plus('test', 2)", "null",    // Wrong argument type
    "example.plus('one', 'two')", "null", // Wrong argument type
    "" // Array end marker: insert additional test pairs before this.
  };

  for (int i = 0; tests[i] != ""; i+= 2) {
    std::string left = tests[i];
    std::string right = tests[i + 1];
    std::string js = BuildJSCondition(left, right);
    CheckJavaScriptSuccess(js);
  }

  std::string js = "example.my_value = 3.25; example.my_other_value = 1.25;";
  js.append(BuildJSCondition(
      "example.plus(example.my_value, example.my_other_value)", "4.5"));
  CheckJavaScriptSuccess(js);
}

// Tests that invoking a nonexistent method with no fallback method stops the 
// script's execution
TEST_F(CppBoundClassTest, 
       InvokeNonexistentMethodNoFallback) {
  std::string js = "example.nonExistentMethod();document.writeln('SUCCESS');";
  CheckJavaScriptFailure(js);
}

// Ensures existent methods can be invoked successfully when the fallback method
// is used
TEST_F(CppBoundClassWithFallbackMethodTest, 
       InvokeExistentMethodsWithFallback) {
  std::string js = BuildJSCondition("example.echoValue(34)", "34");
  CheckJavaScriptSuccess(js);
}

}  // namespace
