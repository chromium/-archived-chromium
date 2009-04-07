// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that we cannot see global variables defined in other content scripts.
// This var was defined in script1a.js. We run each content script in a separate
// context, so we shouldn't see globals across them.
function testCannotSeeOtherContentScriptGlobals() {
  assert(typeof script1_var == "undefined");
}

runAllTests();
