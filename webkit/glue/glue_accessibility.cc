// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <comdef.h>

#include "config.h"

#pragma warning(push, 0)
#include "AccessibleDocument.h"
#include "AXObjectCache.h"
#include "Document.h"
#include "Frame.h"
#pragma warning(pop)
#undef LOG

#include "webkit/glue/glue_accessibility.h"

#include "base/ref_counted.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_impl.h"

// struct GlueAccessibility::GlueAccessibilityRoot
struct GlueAccessibility::GlueAccessibilityRoot {
  GlueAccessibilityRoot() {}

  // Root of the WebKit IAccessible tree.
  scoped_refptr<AccessibleDocument> accessibility_root_;
};

// class GlueAccessibility
GlueAccessibility::GlueAccessibility()
    : root_(new GlueAccessibilityRoot) {
}

GlueAccessibility::~GlueAccessibility() {
  delete root_;
}

bool GlueAccessibility::GetAccessibilityInfo(WebView* view,
    const AccessibilityInParams& in_params,
    AccessibilityOutParams* out_params) {
  WebFrame* main_frame = view->GetMainFrame();
  if (!main_frame || !static_cast<WebFrameImpl*>(main_frame)->frameview())
    return false;

  if (!root_->accessibility_root_ && !InitAccessibilityRoot(view)) {
    // Failure in retrieving the root.
    return false;
  }

  // Temporary storing for the currently active IAccessible.
  scoped_refptr<IAccessible> active_iaccessible;
  IntToIAccessibleMap::iterator it =
      int_to_iaccessible_map_.find(in_params.iaccessible_id);

  if (it == int_to_iaccessible_map_.end()) {
    // Map did not contain the data requested.
    return false;
  }

  active_iaccessible = it->second;

  if (!active_iaccessible) {
    // Requested IAccessible not found. Paranoia check.
    NOTREACHED();
    return false;
  }

  // Input VARIANT, determined by the browser side to be of type VT_I4.
  VARIANT input_variant;
  input_variant.vt = VT_I4;
  input_variant.lVal = in_params.input_variant_lval;

  // Output variables, used locally to retrieve data.
  VARIANT output_variant;
  ::VariantInit(&output_variant);
  BSTR output_bstr;
  bool string_output = false;
  HRESULT hr = S_FALSE;

  switch (in_params.iaccessible_function_id) {
    case IACCESSIBLE_FUNC_ACCDODEFAULTACTION :
      hr = active_iaccessible->accDoDefaultAction(input_variant);
      break;
    case IACCESSIBLE_FUNC_ACCHITTEST :
      hr = active_iaccessible->accHitTest(in_params.input_long1,
                                          in_params.input_long2,
                                          &output_variant);
      break;
    case IACCESSIBLE_FUNC_ACCLOCATION :
      hr = active_iaccessible->accLocation(&out_params->output_long1,
                                           &out_params->output_long2,
                                           &out_params->output_long3,
                                           &out_params->output_long4,
                                           input_variant);
      break;
    case IACCESSIBLE_FUNC_ACCNAVIGATE :
      hr = active_iaccessible->accNavigate(in_params.input_long1, input_variant,
                                           &output_variant);
      break;
    case IACCESSIBLE_FUNC_GET_ACCCHILD :
      if (input_variant.lVal == CHILDID_SELF) {
        // If child requested is CHILDID_SELF, stay with the same IAccessible.
        out_params->iaccessible_id = in_params.iaccessible_id;
        hr = S_OK;
        break;
      }
      hr = active_iaccessible->get_accChild(input_variant,
          reinterpret_cast<IDispatch **>(&output_variant.pdispVal));
      output_variant.vt = VT_DISPATCH;
      break;
    case IACCESSIBLE_FUNC_GET_ACCCHILDCOUNT :
      hr = active_iaccessible->get_accChildCount(&out_params->output_long1);
      break;
    case IACCESSIBLE_FUNC_GET_ACCDEFAULTACTION :
      hr = active_iaccessible->get_accDefaultAction(input_variant,
                                                    &output_bstr);
      string_output = true;
      break;
    case IACCESSIBLE_FUNC_GET_ACCDESCRIPTION :
      hr = active_iaccessible->get_accDescription(input_variant, &output_bstr);
      string_output = true;
      break;
    case IACCESSIBLE_FUNC_GET_ACCFOCUS :
      hr = active_iaccessible->get_accFocus(&output_variant);
      break;
    case IACCESSIBLE_FUNC_GET_ACCHELP :
      hr = active_iaccessible->get_accHelp(input_variant, &output_bstr);
      string_output = true;
      break;
    case IACCESSIBLE_FUNC_GET_ACCKEYBOARDSHORTCUT :
      hr = active_iaccessible->get_accKeyboardShortcut(input_variant,
                                                       &output_bstr);
      string_output = true;
      break;
    case IACCESSIBLE_FUNC_GET_ACCNAME :
      hr = active_iaccessible->get_accName(input_variant, &output_bstr);
      string_output = true;
      break;
    case IACCESSIBLE_FUNC_GET_ACCPARENT :
      hr = active_iaccessible->get_accParent(
          reinterpret_cast<IDispatch **>(&output_variant.pdispVal));
      output_variant.vt = VT_DISPATCH;
      break;
    case IACCESSIBLE_FUNC_GET_ACCROLE :
      hr = active_iaccessible->get_accRole(input_variant, &output_variant);
      break;
    case IACCESSIBLE_FUNC_GET_ACCSTATE :
      hr = active_iaccessible->get_accState(input_variant, &output_variant);
      break;
    case IACCESSIBLE_FUNC_GET_ACCVALUE :
      hr = active_iaccessible->get_accValue(input_variant, &output_bstr);
      string_output = true;
      break;
    default:
      // Memory cleanup.
      ::VariantClear(&input_variant);
      ::VariantClear(&output_variant);

      // Non-supported function id.
      return false;
  }

  // Return code handling.
  if (hr == S_OK) {
    out_params->return_code = true;

    // All is ok, assign output string if needed.
    if (string_output) {
      out_params->output_string = _bstr_t(output_bstr);
      ::SysFreeString(output_bstr);
    }

  } else if (hr == S_FALSE) {
    out_params->return_code = false;
  } else {
    // Memory cleanup.
    ::VariantClear(&input_variant);
    ::VariantClear(&output_variant);

    // Generate a generic failure on the browser side. Input validation is the
    // responsibility of the browser side, as is correctly handling calls to
    // non-supported functions appropriately.
    return false;
  }

  // Output and hashmap assignments, as appropriate.
  if (output_variant.vt == VT_DISPATCH && output_variant.pdispVal) {
    IAccessibleToIntMap::iterator it =
        iaccessible_to_int_map_.find(
        reinterpret_cast<IAccessible *>(output_variant.pdispVal));

    if (it != iaccessible_to_int_map_.end()) {
      // Data already present in map, return previously assigned id.
      out_params->iaccessible_id = it->second;
      out_params->output_long1 = -1;
    } else {
      // Insert new IAccessible in hashmaps.
      int_to_iaccessible_map_[iaccessible_id_] =
          reinterpret_cast<IAccessible *>(output_variant.pdispVal);
      iaccessible_to_int_map_[
          reinterpret_cast<IAccessible *>(output_variant.pdispVal)] =
              iaccessible_id_;
      out_params->iaccessible_id = iaccessible_id_++;
      out_params->output_long1 = -1;
    }
  } else if (output_variant.vt == VT_I4) {
    out_params->output_long1 = output_variant.lVal;
  }

  // Memory cleanup.
  ::VariantClear(&input_variant);
  ::VariantClear(&output_variant);

  return true;
}

bool GlueAccessibility::InitAccessibilityRoot(WebView* view) {
  WebCore::AXObjectCache::enableAccessibility();
  iaccessible_id_ = 0;

  WebFrame* main_frame = view->GetMainFrame();
  WebFrameImpl* main_frame_impl = static_cast<WebFrameImpl*>(main_frame);
  WebCore::Frame* frame = main_frame_impl->frame();
  WebCore::Document* currentDocument = frame->document();

  if (!currentDocument || !currentDocument->renderer()) {
    return false;
  } else if (!root_->accessibility_root_ ||
             root_->accessibility_root_->document() != currentDocument) {
    // Either we've never had a wrapper for this frame's top-level Document,
    // the Document renderer was destroyed and its wrapper was detached, or
    // the previous Document is in the page cache, and the current document
    // needs to be wrapped.
    root_->accessibility_root_ = new AccessibleDocument(currentDocument);
  }
  // Insert root in hashmaps.
  int_to_iaccessible_map_[iaccessible_id_] = root_->accessibility_root_.get();
  iaccessible_to_int_map_[root_->accessibility_root_.get()] = iaccessible_id_++;

  return true;
}

bool GlueAccessibility::ClearIAccessibleMap(int iaccessible_id,
                                            bool clear_all) {
  if (clear_all) {
    // Clear maps and invalidate root.
    int_to_iaccessible_map_.clear();
    iaccessible_to_int_map_.clear();
    root_->accessibility_root_ = 0;
    return true;
  }

  IntToIAccessibleMap::iterator it =
      int_to_iaccessible_map_.find(iaccessible_id);

  if (it == int_to_iaccessible_map_.end()) {
    // Element not found.
    return false;
  } else {
    if (it->second) {
      // Erase element from reverse hashmap.
      IAccessibleToIntMap::iterator it2 =
          iaccessible_to_int_map_.find(it->second.get());

      DCHECK(it2 != iaccessible_to_int_map_.end());
      iaccessible_to_int_map_.erase(it2);
    }

    int_to_iaccessible_map_.erase(it);

    if (iaccessible_id == 0) {
      // Invalidate root.
      root_->accessibility_root_ = 0;
    }
  }

  return true;
}
