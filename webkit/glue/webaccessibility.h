// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBACCESSIBILITY_H_
#define WEBKIT_GLUE_WEBACCESSIBILITY_H_

#include "base/string16.h"

namespace webkit_glue {

class WebAccessibility {
 public:
  // This defines an enumeration of IDs that can uniquely identify a call to a
  // specific accessibility information function. Should match the support
  // implemented in WebKit and GlueAccessibilityObject (functions marked with
  // return value E_NOTIMPL in WebKit are also excluded).
  enum Function {
    FUNCTION_NONE = 0,

    // Supported accessibility information retrieval functions.
    FUNCTION_DODEFAULTACTION,
    FUNCTION_HITTEST,
    FUNCTION_LOCATION,
    FUNCTION_NAVIGATE,
    FUNCTION_GETCHILD,
    FUNCTION_CHILDCOUNT,
    FUNCTION_DEFAULTACTION,
    FUNCTION_DESCRIPTION,
    FUNCTION_GETFOCUSEDCHILD,
    FUNCTION_HELPTEXT,
    FUNCTION_KEYBOARDSHORTCUT,
    FUNCTION_NAME,
    FUNCTION_GETPARENT,
    FUNCTION_ROLE,
    FUNCTION_STATE,
    FUNCTION_VALUE

    // The deprecated put_accName and put_accValue (IAccessible) are not
    // supported here, nor is accSelect, get_accHelpTopic and get_accSelection
    // (matching WebKit's support for IAccessible).
  };

  // This defines an enumeration of navigation directions based on (but
  // independent of) the MSAA Navigation Constants. However, to avoid the use of
  // COM in our Glue layer, we use this as a substitute with a one-to-one
  // conversion between Browser side (has COM) and Glue.
  enum Direction {
    DIRECTION_NONE = 0,

    // Valid directions.
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
    DIRECTION_NEXT,
    DIRECTION_PREVIOUS,
    DIRECTION_FIRSTCHILD,
    DIRECTION_LASTCHILD
  };

  // This defines an enumeration of the supported accessibility roles in our
  // Glue layer (used in GlueAccessibilityObject::Role). Any interface using
  // roles must provide a conversion to its own roles (see e.g.
  // BrowserAccessibility::get_accRole and BrowserAccessibility::MSAARole).
  enum Role {
    ROLE_PUSHBUTTON,
    ROLE_RADIOBUTTON,
    ROLE_CHECKBUTTON,
    ROLE_SLIDER,
    ROLE_PAGETABLIST,
    ROLE_TEXT,
    ROLE_STATICTEXT,
    ROLE_OUTLINE,
    ROLE_COLUMN,
    ROLE_ROW,
    ROLE_GROUPING,
    ROLE_LIST,
    ROLE_TABLE,
    ROLE_LINK,
    ROLE_GRAPHIC,
    ROLE_CLIENT
  };

  // This defines an enumeration of the supported accessibility states in our
  // Glue layer (used in GlueAccessibilityObject::State). Any interface using
  // states must provide a conversion to its own states (see e.g.
  // BrowserAccessibility::get_accState and BrowserAccessibility::MSAAState).
  enum State {
    STATE_LINKED,
    STATE_HOTTRACKED,
    STATE_UNAVAILABLE,
    STATE_READONLY,
    STATE_OFFSCREEN,
    STATE_MULTISELECTABLE,
    STATE_PROTECTED,
    STATE_INDETERMINATE,
    STATE_CHECKED,
    STATE_PRESSED,
    STATE_FOCUSED,
    STATE_TRAVERSED,
    STATE_FOCUSABLE
  };

  // Parameters structure to hold a union of the possible accessibility function
  // INPUT variables, with the unused fields always set to default value. Used
  // in ViewMsg_GetAccessibilityInfo, as only parameter.
  struct InParams {
    // Identifier to uniquely distinguish which instance of accessibility
    // information is being called upon on the renderer side.
    int object_id;

    // Identifier to resolve which accessibility information retrieval function
    // is being called.
    int function_id;

    // Id of accessible child, whose information is being requested.
    int child_id;

    // LONG input parameters, used differently depending on the function called.
    long input_long1;
    long input_long2;
  };

  // Parameters structure to hold a union of the possible accessibility function
  // OUTPUT variables, with the unused fields always set to default value. Used
  // in ViewHostMsg_GetAccessibilityInfoResponse, as only parameter.
  struct OutParams {
    // Identifier to uniquely distinguish which instance of accessibility
    // information is being called upon on the renderer side.
    int object_id;

    // LONG output parameters, used differently depending on the function
    // called. [output_long1] can in some cases be set to -1 to indicate that
    // the child object found by the called IAccessible function is not a simple
    // object.
    long output_long1;
    long output_long2;
    long output_long3;
    long output_long4;

    // String output parameter.
    string16 output_string;

    // Return code, either true (MSAA S_OK) or false (MSAA S_FALSE).
    // Interface-specific error return codes (e.g. MSAA's E_POINTER,
    // E_INVALIDARG, E_FAIL, E_NOTIMPL) must be handled on the browser side by
    // input validation.
    bool return_code;
  };
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBACCESSIBILITY_H_
