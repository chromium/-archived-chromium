// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "AXObjectCache.h"
#include "Document.h"
#include "Frame.h"
#include "RefPtr.h"
#undef LOG

#include "webkit/glue/webaccessibilitymanager_impl.h"

#include "webkit/glue/glue_accessibility_object.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_impl.h"

namespace webkit_glue {

// struct WebAccessibilityManagerImpl::GlueAccessibilityObjectRoot
struct WebAccessibilityManagerImpl::GlueAccessibilityObjectRoot {
  GlueAccessibilityObjectRoot() {}

  // Root of the WebKit AccessibilityObject tree.
  RefPtr<GlueAccessibilityObject> acc_obj_root_;
};

/*static*/
WebAccessibilityManager* WebAccessibilityManager::Create() {
  return new WebAccessibilityManagerImpl();
}

// class WebAccessibilityManagerImpl
WebAccessibilityManagerImpl::WebAccessibilityManagerImpl()
    : root_(new GlueAccessibilityObjectRoot) {
}

bool WebAccessibilityManagerImpl::GetAccObjInfo(WebView* view,
  const WebAccessibility::InParams& in_params,
  WebAccessibility::OutParams* out_params) {
  if (!root_->acc_obj_root_ && !InitAccObjRoot(view)) {
    // Failure in retrieving or initializing the root.
    return false;
  }

  // Find GlueAccessibilityObject requested by [in_params.object_id].
  IntToAccObjMap::iterator it =
      int_to_acc_obj_map_.find(in_params.object_id);
  if (it == int_to_acc_obj_map_.end() || !it->second) {
    // Map did not contain a valid instance of the data requested.
    return false;
  }
  RefPtr<GlueAccessibilityObject> active_acc_obj = it->second;

  // Function input parameters.
  int child_id = in_params.child_id;

  // Temp paramters for holding output information.
  RefPtr<GlueAccessibilityObject> out_acc_obj = NULL;
  WebCore::String out_string;

  switch (in_params.function_id) {
    case WebAccessibility::FUNCTION_DODEFAULTACTION :
      if (!active_acc_obj->DoDefaultAction(child_id))
        return false;
      break;
    case WebAccessibility::FUNCTION_HITTEST :
      out_acc_obj = active_acc_obj->HitTest(in_params.input_long1,
                                            in_params.input_long2);
      if (!out_acc_obj.get())
        return false;
      break;
    case WebAccessibility::FUNCTION_LOCATION :
      if (!active_acc_obj->Location(&out_params->output_long1,
                                    &out_params->output_long2,
                                    &out_params->output_long3,
                                    &out_params->output_long4,
                                    child_id)) {
        return false;
      }
      break;
    case WebAccessibility::FUNCTION_NAVIGATE :
      out_acc_obj = active_acc_obj->Navigate(
          static_cast<WebAccessibility::Direction>(in_params.input_long1),
          child_id);
      if (!out_acc_obj.get())
        return false;
      break;
    case WebAccessibility::FUNCTION_GETCHILD :
      if (child_id == 0) {
        // If child requested is self, stay with the same accessibility object.
        out_params->object_id = in_params.object_id;
        out_acc_obj = active_acc_obj.get();
      } else {
        out_acc_obj = active_acc_obj->GetChild(child_id);
      }

      if (!out_acc_obj.get())
        return false;
      break;
    case WebAccessibility::FUNCTION_CHILDCOUNT :
      if (!active_acc_obj->ChildCount(&out_params->output_long1))
        return false;
      break;
    case WebAccessibility::FUNCTION_DEFAULTACTION :
      if (!active_acc_obj->DefaultAction(child_id, &out_string))
        return false;
      break;
    case WebAccessibility::FUNCTION_DESCRIPTION :
      if (!active_acc_obj->Description(child_id, &out_string))
        return false;
      break;
    case WebAccessibility::FUNCTION_GETFOCUSEDCHILD :
      out_acc_obj = active_acc_obj->GetFocusedChild();
      if (!out_acc_obj.get())
        return false;
      break;
    case WebAccessibility::FUNCTION_HELPTEXT :
      if (!active_acc_obj->HelpText(child_id, &out_string))
        return false;
      break;
    case WebAccessibility::FUNCTION_KEYBOARDSHORTCUT :
      if (!active_acc_obj->KeyboardShortcut(child_id, &out_string))
        return false;
      break;
    case WebAccessibility::FUNCTION_NAME :
      if (!active_acc_obj->Name(child_id, &out_string))
        return false;
      break;
    case WebAccessibility::FUNCTION_GETPARENT :
      out_acc_obj = active_acc_obj->GetParent();
      if (!out_acc_obj.get())
        return false;
      break;
    case WebAccessibility::FUNCTION_ROLE :
      if (!active_acc_obj->Role(child_id, &out_params->output_long1))
        return false;
      break;
    case WebAccessibility::FUNCTION_STATE :
      if (!active_acc_obj->State(child_id, &out_params->output_long1))
        return false;
      break;
    case WebAccessibility::FUNCTION_VALUE :
      if (!active_acc_obj->Value(child_id, &out_string))
        return false;
      break;
    default:
      // Non-supported function id.
      return false;
  }

  // Output and hashmap assignments, as appropriate.
  if (!out_string.isEmpty())
    out_params->output_string = StringToString16(out_string);

  if (out_acc_obj) {
    AccObjToIntMap::iterator it = acc_obj_to_int_map_.find(out_acc_obj.get());

    if (it != acc_obj_to_int_map_.end()) {
      // Data already present in map, return previously assigned id.
      out_params->object_id = it->second;
      out_params->output_long1 = -1;
    } else {
      // Insert new GlueAccessibilityObject in hashmaps.
      int_to_acc_obj_map_[acc_obj_id_] = out_acc_obj.get();
      acc_obj_to_int_map_[out_acc_obj.get()] = acc_obj_id_;
      out_params->object_id = acc_obj_id_++;
      out_params->output_long1 = -1;
    }
  }
  // TODO(klink): Handle simple objects returned.
  return true;
}

bool WebAccessibilityManagerImpl::InitAccObjRoot(WebView* view) {
  // Root id is always 0.
  acc_obj_id_ = 0;

  // Enable accessibility and retrieve Document.
  WebCore::AXObjectCache::enableAccessibility();
  WebFrameImpl* main_frame_impl =
      static_cast<WebFrameImpl*>(view->GetMainFrame());
  if (!main_frame_impl || !main_frame_impl->frame())
    return false;

  WebCore::Document* doc = main_frame_impl->frame()->document();

  if (!doc || !doc->renderer())
    return false;

  if (!root_->acc_obj_root_) {
    // Either we've never had a wrapper for this frame's top-level Document,
    // the Document renderer was destroyed and its wrapper was detached, or
    // the previous Document is in the page cache, and the current document
    // needs to be wrapped.
    root_->acc_obj_root_ = GlueAccessibilityObject::CreateInstance(doc->
        axObjectCache()->getOrCreate(doc->renderer()));
  }
  // Insert root in hashmaps.
  int_to_acc_obj_map_[acc_obj_id_] = root_->acc_obj_root_.get();
  acc_obj_to_int_map_[root_->acc_obj_root_.get()] = acc_obj_id_++;

  return true;
}

bool WebAccessibilityManagerImpl::ClearAccObjMap(int acc_obj_id,
                                                 bool clear_all) {
  if (clear_all) {
    // Clear maps and invalidate root.
    int_to_acc_obj_map_.clear();
    acc_obj_to_int_map_.clear();
    root_->acc_obj_root_ = 0;
    return true;
  }

  IntToAccObjMap::iterator it = int_to_acc_obj_map_.find(acc_obj_id);

  if (it == int_to_acc_obj_map_.end()) {
    // Element not found.
    return false;
  }

  if (it->second) {
    // Erase element from reverse hashmap.
    AccObjToIntMap::iterator it2 = acc_obj_to_int_map_.find(it->second);

    if (it2 != acc_obj_to_int_map_.end())
      acc_obj_to_int_map_.erase(it2);
  }
  int_to_acc_obj_map_.erase(it);

  if (acc_obj_id == 0) {
    // Invalidate root.
    root_->acc_obj_root_ = 0;
  }
  return true;
}

}  // namespace webkit_glue
