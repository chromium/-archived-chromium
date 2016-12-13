// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_GLUE_ACCESSIBILITY_OBJECT_H_
#define WEBKIT_GLUE_GLUE_ACCESSIBILITY_OBJECT_H_

#include "AccessibilityObjectWrapper.h"

#include "base/basictypes.h"
#include "webkit/glue/webaccessibility.h"

namespace WebCore {
class AccessibilityObject;
enum AccessibilityRole;
}

////////////////////////////////////////////////////////////////////////////////
//
// GlueAccessibilityObject
//
// Operations that access the underlying WebKit DOM directly, exposing
// accessibility information to the GlueAccessibilityManager. Also provides a
// platform-independent wrapper to WebKit's AccessibilityObject.
////////////////////////////////////////////////////////////////////////////////
class GlueAccessibilityObject : public WebCore::AccessibilityObjectWrapper {
 public:
  static GlueAccessibilityObject* CreateInstance(WebCore::AccessibilityObject*);

  virtual ~GlueAccessibilityObject() {}

  // Performs the default action on a given object.
  bool DoDefaultAction(int child_id);

  // Retrieves the child element or child object at a given point on the screen.
  GlueAccessibilityObject* HitTest(long x, long y);

  // Retrieves the specified object's current screen location.
  bool Location(long* left,
                long* top,
                long* width,
                long* height,
                int child_id);

  // Traverses to another UI element and retrieves the object.
  GlueAccessibilityObject* Navigate(
      webkit_glue::WebAccessibility::Direction dir,
      int start_child_id);

  // Retrieves an GlueAccessibilityObject pointer for the specified [child_id].
  GlueAccessibilityObject* GetChild(int child_id);

  // Retrieves the number of accessible children.
  bool ChildCount(long* count);

  // Retrieves a string that describes the object's default action.
  bool DefaultAction(int child_id, WebCore::String* action);

  // Retrieves the object's description.
  bool Description(int child_id, WebCore::String* description);

  // Retrieves the object that has the keyboard focus.
  GlueAccessibilityObject* GetFocusedChild();

  // Retrieves the help information associated with the object.
  bool HelpText(int child_id, WebCore::String* help);

  // Retrieves the specified object's shortcut.
  bool KeyboardShortcut(int child_id, WebCore::String* shortcut);

  // Retrieves the name of the specified object.
  bool Name(int child_id, WebCore::String* name);

  // Retrieves the GlueAccessibilityObject of the object's parent. In the case
  // of the root object (where the parent is the containing window), it is up
  // to the browser side to handle this.
  GlueAccessibilityObject* GetParent();

  // Retrieves information describing the role of the specified object.
  bool Role(int child_id, long* role);

  // Retrieves the current state of the specified object.
  bool State(int child_id, long* state);

  // Returns the value associated with the object.
  bool Value(int child_id, WebCore::String* value);

  // WebCore::AccessiblityObjectWrapper.
  virtual void detach() {
    if (m_object)
      m_object = 0;
  }

 protected:
  explicit GlueAccessibilityObject(WebCore::AccessibilityObject*);

  // Helper functions.
  WebCore::String name() const;
  WebCore::String value() const;
  WebCore::String description() const;
  webkit_glue::WebAccessibility::Role role() const;

  // Retrieves the AccessibilityObject for a given [child_id]. Returns false if
  // [child_id] is less than 0, or if no valid AccessibilityObject is found.
  // A [child_id] of 0 is treated as referring to the current object itself.
  bool GetAccessibilityObjectForChild(int child_id,
                                      WebCore::AccessibilityObject*&) const;

  // Wraps the given AccessibilityObject in a GlueAccessibilityObject and
  // returns it. If the AccessibilityObject already has a wrapper assigned, that
  // one is returned. Otherwise, a new instance of GlueAccessibilityObject is
  // created and assigned as the wrapper.
  static GlueAccessibilityObject* ToWrapper(WebCore::AccessibilityObject*);

 private:
  DISALLOW_COPY_AND_ASSIGN(GlueAccessibilityObject);
};

#endif  // WEBKIT_GLUE_GLUE_ACCESSIBILITY_OBJECT_H_
