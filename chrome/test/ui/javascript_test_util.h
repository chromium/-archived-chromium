// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_JAVASCRIPT_TEST_UTIL_H_
#define CHROME_TEST_UI_JAVASCRIPT_TEST_UTIL_H_

#include <string>
#include <map>

// This file provides a common set of utilities that are useful to UI tests
// that interact with JavaScript.

// Given a JSON encoded representation of a dictionary, parses the string and
// fills in a map with the results. No attempt is made to clear the map.
// Returns a bool indicating success or failure of the operation.
bool JsonDictionaryToMap(const std::string& json,
                         std::map<std::string, std::string>* results);

#endif  // CHROME_TEST_UI_JAVASCRIPT_TEST_UTIL_H_
