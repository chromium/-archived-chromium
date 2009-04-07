// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A quick and dirty JavaScript test runner.

function assert(truth) {
  if (!truth) {
    throw new Error('Assertion failed');
  }
}

function runAllTests() {
  // If there was already an error, do nothing. We don't want to muddy
  // up the results.
  if (document.title.indexOf("Error: ") == 0) {
    return;
  }

  for (var propName in window) {
    if (typeof window[propName] == "function" &&
        propName.indexOf("test") == 0) {
      try {
        window[propName]();
        document.title += propName + ",";
      } catch (e) {
        // We use document.title to communicate results back to the browser.
        document.title = "Error: " + propName + ': ' + e.message;
        return;
      }
    }
  }
}
