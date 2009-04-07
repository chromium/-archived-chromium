// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var gotDOMContentLoadedEvent = false;

// Test that at parse time, we have the document element.
// This basically tests that we don't get injected too early (before there is
// a document element).
var hasDocumentElement = (document.documentElement.tagName == "HTML");

// TODO(aa): We would like to add more tests here verifying that we aren't
// injected too late. For example, we could test that there are zero child
// nodes to the documentElement, but unfortunately run_at:document_start is
// currently buggy and doesn't guarantee that.

window.addEventListener("DOMContentLoaded", function() {
  gotDOMContentLoadedEvent = true;
}, false);

// Don't run tests until onload so that we can test that DOMContentLoaded and
// onload happen after this script runs.  
window.addEventListener("load", runAllTests, false);

function testRunAtDocumentStart() {
  assert(hasDocumentElement);
}

function testGotLoadEvents() {
  assert(gotDOMContentLoadedEvent);
}
