/*
  Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

/*
  Common methods for performance-plotting JS.
*/

function Fetch(url, callback) {
  var r = new XMLHttpRequest();
  r.open("GET", url);
  r.setRequestHeader("pragma", "no-cache");
  r.setRequestHeader("cache-control", "no-cache");
  r.send(null);

  r.onload = function() {
    callback(r.responseText);
  }
}

// Returns an Object with properties given by the parameters specified in the
// URL's query string.
function ParseParams() {
  var result = new Object();
  var s = window.location.search.substring(1).split('&');
  for (i = 0; i < s.length; ++i) {
    var v = s[i].split('=');
    result[v[0]] = unescape(v[1]);
  }
  return result;
}

// Creates the URL constructed from the current pathname and the given params.
function MakeURL(params) {
  var url = window.location.pathname;
  var sep = '?';
  for (p in params) {
    if (!p)
      continue;
    url = url + sep + p + '=' + params[p];
    sep = '&';
  }
  return url;
}

// Returns a string describing an object, recursively.  On the initial call,
// |name| is optionally the name of the object and |indent| is not needed.
function DebugDump(obj, opt_name, opt_indent) {
  var name = opt_name || '';
  var indent = opt_indent || '';
  if (typeof obj == "object") {
    var child = null;
    var output = indent + name + "\n";

    for (var item in obj) {
      try {
        child = obj[item];
      } catch (e) {
        child = "<Unable to Evaluate>";
      }
      output += DebugDump(child, item, indent + "  ");
    }

    return output;
  } else {
    return indent + name + ": " + obj + "\n";
  }
}
