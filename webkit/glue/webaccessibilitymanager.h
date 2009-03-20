// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBACCESSIBILITYMANAGER_H_
#define WEBKIT_GLUE_WEBACCESSIBILITYMANAGER_H_

#include "webkit/glue/webaccessibility.h"

class WebView;

////////////////////////////////////////////////////////////////////////////////
//
// WebAccessibilityManager
//
// Responds to incoming accessibility requests from the browser side. Retrieves
// the requested information from the active AccessibilityObject, through the
// GlueAccessibilityObject.
////////////////////////////////////////////////////////////////////////////////
namespace webkit_glue {

class WebAccessibilityManager {
 public:
  WebAccessibilityManager() {}
  virtual ~WebAccessibilityManager() {}

  static WebAccessibilityManager* Create();

  // Retrieves the accessibility information as requested in in_params, by
  // calling into WebKit's AccessibilityObject. Maintains a hashmap of the
  // currently active (browser side ref-count non-zero) instances. Returns true
  // if successful, false otherwise.
  virtual bool GetAccObjInfo(WebView* view,
      const WebAccessibility::InParams& in_params,
      WebAccessibility::OutParams* out_params) = 0;

  // Erases the entry identified by the [acc_obj_id] from the hash maps. If
  // [clear_all] is true, all entries are erased. Returns true if successful,
  // false otherwise.
  virtual bool ClearAccObjMap(int acc_obj_id, bool clear_all) = 0;

  // Retrieves the RenderObject associated with this WebView, and uses it to
  // initialize the root of the GlueAccessibilityObject tree with the
  // associated accessibility information. Returns true if successful, false
  // otherwise.
  virtual bool InitAccObjRoot(WebView* view) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAccessibilityManager);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBACCESSIBILITYMANAGER_H_
