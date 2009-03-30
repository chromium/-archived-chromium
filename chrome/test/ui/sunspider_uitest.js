/*
  Copyright (c) 2009 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

// Automation utilities for running SunSpider as a UI test.

function Automation() {
}

Automation.prototype.SetDone = function() {
  document.cookie = '__done=1; path=/';
}

Automation.prototype.GetTotal = function() {
  // Calculated in sunspider-analyze-results.js.
  return mean + ',' + stdDev;
}

Automation.prototype.GetResults = function() {
  var results = {};

  var input = eval("(" + decodeURI(location.search.substring(1)) + ")");
  for (test in input)
    results[test] = input[test].toString();

  return results;
}

automation = new Automation();
