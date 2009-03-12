/*
  Copyright (c) 2009 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

// Write a cookie with a given name and value.
function writeCookie(name, value) {
  document.cookie = name + '=' + value + '; path=/';
}

// Convert an array to a string to be stored in a cookie.
//
// Since cookie values are limited in length, this function takes a parameter
// 'max_chars' to indicate the maximum length of the returned string.  The
// returned string will be less than or equal to 'max_chars' in length.  Values
// from the array are added to the string, in left to right order, until either
// there are no more elements in the array or the next element will cause the
// string to overflow the maximum number of characters allowed.
function convertArrayToCookieValue(arr, max_chars) {
  var string_builder = [];
  var current_length = 0;

  for (var i = 0; i < arr.length && current_length <= max_chars; ++i) {
    var value = arr[i].toString();

    // Get rid of newline characters.
    value = value.replace(/[\r\n]/g, '');
    // Replace reserved characters with spaces.
    value = value.replace(/[,;=]/g, ' ');

    // Make sure that the string length doesn't exceed the maximum allowed
    // number of characters.
    if (current_length + value.length > max_chars)
      break;

    string_builder.push(value);

    // Add an extra 1 to factor in the length of the separator (a comma).
    current_length += value.length + 1;
  }

  return string_builder.toString();
}

// Override functions that can spawn dialog boxes.

window.alert = function() {}
window.confirm = function() {}
window.prompt = function() {}
