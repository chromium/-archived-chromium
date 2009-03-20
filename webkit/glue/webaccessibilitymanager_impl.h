// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBACCESSIBILITYMANAGER_IMPL_H_
#define WEBKIT_GLUE_WEBACCESSIBILITYMANAGER_IMPL_H_

#include "base/hash_tables.h"
#include "webkit/glue/webaccessibilitymanager.h"

class GlueAccessibilityObject;

namespace webkit_glue {
typedef base::hash_map<int, GlueAccessibilityObject*> IntToAccObjMap;
typedef base::hash_map<GlueAccessibilityObject*, int> AccObjToIntMap;

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
class WebAccessibilityManagerImpl : public WebAccessibilityManager {
 public:
  // From WebAccessibilityManager.
  bool GetAccObjInfo(WebView* view, const WebAccessibility::InParams& in_params,
                     WebAccessibility::OutParams* out_params);

  // From WebAccessibilityManager.
  bool ClearAccObjMap(int acc_obj_id, bool clear_all);

 protected:
  // Needed so WebAccessibilityManager::Create can call our constructor.
  friend class WebAccessibilityManager;

  WebAccessibilityManagerImpl();
  ~WebAccessibilityManagerImpl() {}

 private:
  // From WebAccessibilityManager.
  bool InitAccObjRoot(WebView* view);

  // Wrapper around the pointer that holds the root of the AccessibilityObject
  // tree, to allow the use of a scoped_refptr.
  struct GlueAccessibilityObjectRoot;
  GlueAccessibilityObjectRoot* root_;

  // Hashmap for cashing of elements in use by the AT, mapping id (int) to a
  // GlueAccessibilityObject pointer.
  IntToAccObjMap int_to_acc_obj_map_;
  // Hashmap for cashing of elements in use by the AT, mapping a
  // GlueAccessibilityObject pointer to its id (int). Needed for reverse lookup,
  // to ensure unnecessary duplicate entries are not created in the
  // IntToAccObjMap (above).
  AccObjToIntMap acc_obj_to_int_map_;

  // Unique identifier for retrieving a GlueAccessibilityObject from the page's
  // hashmaps.
  int acc_obj_id_;

  DISALLOW_COPY_AND_ASSIGN(WebAccessibilityManagerImpl);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBACCESSIBILITYMANAGER_IMPL_H_
