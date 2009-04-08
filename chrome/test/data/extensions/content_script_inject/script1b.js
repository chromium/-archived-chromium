// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tests that we are running in the same global context as script1a.js.
function testScriptFilesRunInSameContext() {
  assert(script1_var === 1);
}

// This tests that our relationship to content is working correctly.
// a) We should not see content globals in our global scope.
// b) We should have a contentWindow object that looks like a DOM Window.
// c) We should be able to access content globals via contentWindow.
function testContentInteraction() {
  assert(typeof content_var == "undefined");
  assert(typeof contentWindow != "undefined");
  assert(contentWindow.location.href.match(/content_script_inject_page.html$/));
  assert(contentWindow.content_var == "hello");
}

// Test that our css in script1.css was injected successfully.
function testCSSWasInjected() {
  assert(document.defaultView.getComputedStyle(
      document.body, null)["background-color"] == "rgb(255, 0, 0)");
}

runAllTests();
