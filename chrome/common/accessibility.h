// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACCESSIBILITY_H_
#define CHROME_COMMON_ACCESSIBILITY_H_

// This defines an enumeration of IDs that can uniquely identify a call to a
// specific IAccessible function. Should match the support implemented in WebKit
// (functions marked with return value E_NOTIMPL are also excluded).
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

// Parameters structure to hold a union of the possible IAccessible function
// INPUT variables, with the unused fields always set to default value. Used in
// ViewMsg_GetAccessibilityInfo, as only parameter.
struct AccessibilityInParams {
  // Identifier to uniquely distinguish which instance of IAccessible is being
  // called upon on the renderer side.
  int iaccessible_id;

  // Identifier to resolve which IAccessible interface function is being called.
  int iaccessible_function_id;

  // Function input parameters.
  // Input VARIANT structure's LONG field to specify requested object.
  long input_variant_lval;

  // LONG input parameters, used differently depending on the function called.
  long input_long1;
  long input_long2;
};

// Parameters structure to hold a union of the possible IAccessible function
// OUTPUT variables, with the unused fields always set to default value. Used in
// ViewHostMsg_GetAccessibilityInfoResponse, as only parameter.
struct AccessibilityOutParams {
  // Identifier to uniquely distinguish which instance of IAccessible is being
  // called upon on the renderer side.
  int iaccessible_id;

  // Function output parameters.
  // Output VARIANT structure's LONG field to specify requested object.
  long output_variant_lval;

  // LONG output parameters, used differently depending on the function called.
  // output_long1 can in some cases be set to -1 to indicate that the child
  // object found by the called IAccessible function is not a simple object.
  long output_long1;
  long output_long2;
  long output_long3;
  long output_long4;

  // String output parameter.
  std::wstring output_string;

  // Return code, either S_OK (true) or S_FALSE (false). WebKit MSAA error
  // return codes (E_POINTER, E_INVALIDARG, E_FAIL, E_NOTIMPL) must be handled
  // on the browser side by input validation.
  bool return_code;
};

#endif  // CHROME_COMMON_ACCESSIBILITY_H_
