// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (window == top) {
  findFeeds();
  window.addEventListener("focus", findFeeds);
}

function findFeeds() {
  // Find all the RSS link elements.
  var result = document.evaluate(
      '//link[@rel="alternate"][contains(@type, "rss") or ' +
      'contains(@type, "atom") or contains(@type, "rdf")]',
      document, null, 0, null);

  var feeds = [];
  var item;
  while (item = result.iterateNext())
    feeds.push(item.href);

  // Notify the extension of the feed URLs we found.
  chrome.extension.connect().postMessage(feeds);
}
