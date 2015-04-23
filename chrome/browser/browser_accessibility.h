// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACCESSIBILITY_H_
#define CHROME_BROWSER_BROWSER_ACCESSIBILITY_H_

#include <atlbase.h>
#include <atlcom.h>

#include <oleacc.h>

#include "webkit/glue/webaccessibility.h"

using webkit_glue::WebAccessibility;

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibility
//
// Class implementing the MSAA IAccessible COM interface for the
// Browser-Renderer communication of MSAA information, providing accessibility
// to be used by screen readers and other assistive technology (AT).
//
////////////////////////////////////////////////////////////////////////////////
class ATL_NO_VTABLE BrowserAccessibility
  : public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchImpl<IAccessible, &IID_IAccessible, &LIBID_Accessibility> {
 public:
  BEGIN_COM_MAP(BrowserAccessibility)
    COM_INTERFACE_ENTRY2(IDispatch, IAccessible)
    COM_INTERFACE_ENTRY(IAccessible)
  END_COM_MAP()

  BrowserAccessibility() {}
  ~BrowserAccessibility() {}

  HRESULT Initialize(int iaccessible_id,
                     int routing_id,
                     int process_id,
                     HWND parent_hwnd);

  // Supported IAccessible methods.

  // Performs the default action on a given object.
  STDMETHODIMP accDoDefaultAction(VARIANT var_id);

  // Retrieves the child element or child object at a given point on the screen.
  STDMETHODIMP accHitTest(LONG x_left, LONG y_top, VARIANT* child);

  // Retrieves the specified object's current screen location.
  STDMETHODIMP accLocation(LONG* x_left,
                           LONG* y_top,
                           LONG* width,
                           LONG* height,
                           VARIANT var_id);

  // Traverses to another UI element and retrieves the object.
  STDMETHODIMP accNavigate(LONG nav_dir, VARIANT start, VARIANT* end);

  // Retrieves an IDispatch interface pointer for the specified child.
  STDMETHODIMP get_accChild(VARIANT var_child, IDispatch** disp_child);

  // Retrieves the number of accessible children.
  STDMETHODIMP get_accChildCount(LONG* child_count);

  // Retrieves a string that describes the object's default action.
  STDMETHODIMP get_accDefaultAction(VARIANT var_id, BSTR* default_action);

  // Retrieves the object's description.
  STDMETHODIMP get_accDescription(VARIANT var_id, BSTR* desc);

  // Retrieves the object that has the keyboard focus.
  STDMETHODIMP get_accFocus(VARIANT* focus_child);

  // Retrieves the help information associated with the object.
  STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* help);

  // Retrieves the specified object's shortcut.
  STDMETHODIMP get_accKeyboardShortcut(VARIANT var_id, BSTR* access_key);

  // Retrieves the name of the specified object.
  STDMETHODIMP get_accName(VARIANT var_id, BSTR* name);

  // Retrieves the IDispatch interface of the object's parent.
  STDMETHODIMP get_accParent(IDispatch** disp_parent);

  // Retrieves information describing the role of the specified object.
  STDMETHODIMP get_accRole(VARIANT var_id, VARIANT* role);

  // Retrieves the current state of the specified object.
  STDMETHODIMP get_accState(VARIANT var_id, VARIANT* state);

  // Returns the value associated with the object.
  STDMETHODIMP get_accValue(VARIANT var_id, BSTR* value);

  // Non-supported (by WebKit) IAccessible methods.
  STDMETHODIMP accSelect(LONG flags_sel, VARIANT var_id) { return E_NOTIMPL; }

  STDMETHODIMP get_accHelpTopic(BSTR* help_file,
                                VARIANT var_id,
                                LONG* topic_id);

  STDMETHODIMP get_accSelection(VARIANT* selected);

  // Deprecated functions, not implemented here.
  STDMETHODIMP put_accName(VARIANT var_id, BSTR put_name) { return E_NOTIMPL; }
  STDMETHODIMP put_accValue(VARIANT var_id, BSTR put_val) { return E_NOTIMPL; }

  // Accessors/mutators.
  HWND parent_hwnd() const { return parent_hwnd_;}

  // Modify/retrieve the state (active/inactive) of this instance.
  void set_instance_active(bool instance_active) {
    instance_active_ = instance_active;
  }
  int instance_active() const { return instance_active_; }

  int routing_id() const { return routing_id_; }

 private:
  // Creates an empty VARIANT. Used as the equivalent of a NULL (unused) input
  // parameter.
  VARIANT EmptyVariant() const {
    VARIANT empty;
    empty.vt = VT_EMPTY;
    return empty;
  }

  // Wrapper functions, calling through to singleton instance of
  // BrowserAccessibilityManager.

  // Creates an instance of BrowserAccessibility, initializes it and sets the
  // [iaccessible_id] and [parent_id].
  STDMETHODIMP CreateInstance(REFIID iid,
                              int iaccessible_id,
                              void** interface_ptr);

  // Composes and sends a message for requesting needed accessibility
  // information. Unused LONG input parameters should be NULL, and the VARIANT
  // an empty, valid instance.
  bool RequestAccessibilityInfo(int iaccessible_func_id,
                                VARIANT var_id,
                                LONG input1, LONG input2);

  // Accessors.
  const WebAccessibility::OutParams& response();

  // Returns a conversion from the BrowserAccessibilityRole (as defined in
  // webkit/glue/webaccessibility.h) to an MSAA role.
  long MSAARole(long browser_accessibility_role);

  // Returns a conversion from the BrowserAccessibilityState (as defined in
  // webkit/glue/webaccessibility.h) to MSAA states set.
  long MSAAState(long browser_accessibility_state);

  // Id to uniquely distinguish this instance in the render-side caching,
  // mapping it to the correct IAccessible on that side. Initialized to -1.
  int iaccessible_id_;

  // The unique ids of this IAccessible instance. Used to help
  // BrowserAccessibilityManager instance retrieve the correct member
  // variables for this process.
  int routing_id_;
  int process_id_;

  HWND parent_hwnd_;

  // The instance should only be active if there is a non-terminated
  // RenderProcessHost associated with it. The BrowserAccessibilityManager keeps
  // track of this state, and sets it to false to disable all calls into the
  // renderer from this instance of BroweserAccessibility, and have all
  // IAccessible functions return E_FAIL.
  bool instance_active_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibility);
};
#endif  // CHROME_BROWSER_BROWSER_ACCESSIBILITY_H_
