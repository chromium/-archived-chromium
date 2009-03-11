// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_FORM_DATA_H__
#define WEBKIT_GLUE_FORM_DATA_H__

#include <vector>

#include "googleurl/src/gurl.h"

// Holds information about a form to be filled and/or submitted.
struct FormData {
  // The URL (minus query parameters) containing the form
  GURL origin;
  // The action target of the form
  GURL action;
  // A list of element names to be filled
  std::vector<std::wstring> elements;
  // A list of element values to be filled
  std::vector<std::wstring> values;
  // The name of the submit button to be used to submit (optional)
  std::wstring submit;
};

#endif  // WEBKIT_GLUE_FORM_DATA_H__
