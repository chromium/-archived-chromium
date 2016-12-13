// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBACCESSIBILITYMANAGER_IMPL_H_
#define WEBKIT_GLUE_WEBACCESSIBILITYMANAGER_IMPL_H_

#include "base/hash_tables.h"
#include "webkit/glue/webaccessibilitymanager.h"

class GlueAccessibilityObject;

////////////////////////////////////////////////////////////////////////////////
//
// WebAccessibilityManagerImpl
//
//
// Implements WebAccessibilityManager.
// Responds to incoming accessibility requests from the browser side. Retrieves
// the requested information from the active AccessibilityObject, through the
// GlueAccessibilityObject.
////////////////////////////////////////////////////////////////////////////////

namespace webkit_glue {

class WebAccessibilityManagerImpl : public WebAccessibilityManager {
 public:
  // From WebAccessibilityManager.
  bool GetAccObjInfo(WebView* view, const WebAccessibility::InParams& in_params,
                     WebAccessibility::OutParams* out_params);
  bool ClearAccObjMap(int acc_obj_id, bool clear_all);
  int FocusAccObj(WebCore::AccessibilityObject* acc_obj);

 protected:
  // Needed so WebAccessibilityManager::Create can call our constructor.
  friend class WebAccessibilityManager;

  // Constructor creates a new GlueAccessibilityObjectRoot, and initializes
  // the root |acc_obj_id_| to 1000, to avoid conflicts with platform-specific
  // child ids.
  WebAccessibilityManagerImpl();
  ~WebAccessibilityManagerImpl();

 private:
  // From WebAccessibilityManager.
  bool InitAccObjRoot(WebView* view);

  // Wrapper around the pointer that holds the root of the AccessibilityObject
  // tree, to allow the use of a scoped_refptr.
  struct GlueAccessibilityObjectRoot;
  GlueAccessibilityObjectRoot* root_;

  typedef base::hash_map<int, GlueAccessibilityObject*> IntToGlueAccObjMap;
  typedef base::hash_map<WebCore::AccessibilityObject*, int> AccObjToIntMap;

  // Hashmap for cashing of elements in use by the AT, mapping id (int) to a
  // GlueAccessibilityObject pointer.
  IntToGlueAccObjMap int_to_glue_acc_obj_map_;
  // Hashmap for cashing of elements in use by the AT, mapping a
  // AccessibilityObject pointer to its id (int). Needed for reverse lookup,
  // to ensure unnecessary duplicate entries are not created in the
  // IntToGlueAccObjMap (above) and for focus changes in WebKit.
  AccObjToIntMap acc_obj_to_int_map_;

  // Unique identifier for retrieving an accessibility object from the page's
  // hashmaps. Id is always 0 for the root of the accessibility object
  // hierarchy (on a per-renderer process basis).
  int acc_obj_id_;

  DISALLOW_COPY_AND_ASSIGN(WebAccessibilityManagerImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBACCESSIBILITYMANAGER_IMPL_H_
