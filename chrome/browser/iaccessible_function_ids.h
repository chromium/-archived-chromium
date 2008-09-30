// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines an enumeration of IDs that can uniquely identify a call to a
// specific IAccessible function. Should match the support implemented in WebKit
// (functions marked with return value E_NOTIMPL are also excluded).

#ifndef CHROME_BROWSER_IACCESSIBLE_FUNCTION_IDS_H_
#define CHROME_BROWSER_IACCESSIBLE_FUNCTION_IDS_H_

enum IAccessibleID {
  IACCESSIBLE_FUNC_NONE = 0,

  // Supported IAccessible interface functions.
  IACCESSIBLE_FUNC_ACCDODEFAULTACTION,
  IACCESSIBLE_FUNC_ACCHITTEST,
  IACCESSIBLE_FUNC_ACCLOCATION,
  IACCESSIBLE_FUNC_ACCNAVIGATE,
  IACCESSIBLE_FUNC_GET_ACCCHILD,
  IACCESSIBLE_FUNC_GET_ACCCHILDCOUNT,
  IACCESSIBLE_FUNC_GET_ACCDEFAULTACTION,
  IACCESSIBLE_FUNC_GET_ACCDESCRIPTION,
  IACCESSIBLE_FUNC_GET_ACCFOCUS,
  IACCESSIBLE_FUNC_GET_ACCHELP,
  IACCESSIBLE_FUNC_GET_ACCKEYBOARDSHORTCUT,
  IACCESSIBLE_FUNC_GET_ACCNAME,
  IACCESSIBLE_FUNC_GET_ACCPARENT,
  IACCESSIBLE_FUNC_GET_ACCROLE,
  IACCESSIBLE_FUNC_GET_ACCSTATE,
  IACCESSIBLE_FUNC_GET_ACCVALUE

  // The deprecated put_accName and put_accValue are not supported here, nor is
  // accSelect, get_accHelpTopic and get_accSelection (matching WebKit's
  // support).
};

#endif  // CHROME_BROWSER_IACCESSIBLE_FUNCTION_IDS_H_
