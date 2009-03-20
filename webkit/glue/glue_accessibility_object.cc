// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "AccessibilityObject.h"
#include "EventHandler.h"
#include "FrameView.h"
#include "PlatformKeyboardEvent.h"

#include "webkit/glue/glue_accessibility_object.h"

using WebCore::AccessibilityObject;
using WebCore::String;
using webkit_glue::WebAccessibility;

GlueAccessibilityObject::GlueAccessibilityObject(AccessibilityObject* obj)
    : AccessibilityObjectWrapper(obj) {
  m_object->setWrapper(this);
}

GlueAccessibilityObject* GlueAccessibilityObject::CreateInstance(
    AccessibilityObject* obj) {
  if (!obj)
    return NULL;

  return new GlueAccessibilityObject(obj);
}

bool GlueAccessibilityObject::DoDefaultAction(int child_id) {
  AccessibilityObject* child_obj;

  if (!GetAccessibilityObjectForChild(child_id, child_obj) ||
      !child_obj->performDefaultAction()) {
    return false;
  }
  return true;
}

GlueAccessibilityObject* GlueAccessibilityObject::HitTest(long x, long y) {
  if (!m_object)
    return NULL;

  // x, y - coordinates are passed in as window coordinates, to maintain
  // sandbox functionality.
  WebCore::IntPoint point =
    m_object->documentFrameView()->windowToContents(WebCore::IntPoint(x, y));
  AccessibilityObject* child_obj = m_object->doAccessibilityHitTest(point);

  if (!child_obj) {
    // If we did not hit any child objects, test whether the point hit us, and
    // report that.
    if (!m_object->boundingBoxRect().contains(point))
      return NULL;
    child_obj = m_object;
  }
  // TODO(klink): simple object child?
  ToWrapper(child_obj)->ref();
  return ToWrapper(child_obj);
}

bool GlueAccessibilityObject::Location(long* left, long* top, long* width,
                                       long* height, int child_id) {
  if (!left || !top || !width || !height)
    return false;

  *left = *top = *width = *height = 0;

  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  // Returning window coordinates, to be handled and converted appropriately by
  // the client.
  WebCore::IntRect window_rect(child_obj->documentFrameView()->contentsToWindow(
      child_obj->boundingBoxRect()));
  *left = window_rect.x();
  *top = window_rect.y();
  *width = window_rect.width();
  *height = window_rect.height();
  return true;
}

GlueAccessibilityObject* GlueAccessibilityObject::Navigate(
    WebAccessibility::Direction dir, int start_child_id) {
  AccessibilityObject* child_obj = 0;

  switch (dir) {
    case WebAccessibility::DIRECTION_DOWN:
    case WebAccessibility::DIRECTION_UP:
    case WebAccessibility::DIRECTION_LEFT:
    case WebAccessibility::DIRECTION_RIGHT:
      // These directions are not implemented, matching Mozilla and IE.
      return NULL;
    case WebAccessibility::DIRECTION_LASTCHILD:
    case WebAccessibility::DIRECTION_FIRSTCHILD:
      // MSDN states that navigating to first/last child can only be from self.
      if (start_child_id != 0 || !m_object)
        return NULL;

      if (dir == WebAccessibility::DIRECTION_FIRSTCHILD) {
        child_obj = m_object->firstChild();
      } else {
        child_obj = m_object->lastChild();
      }
      break;
    case WebAccessibility::DIRECTION_NEXT:
    case WebAccessibility::DIRECTION_PREVIOUS: {
      // Navigating to next and previous is allowed from self or any of our
      // children.
      if (!GetAccessibilityObjectForChild(start_child_id, child_obj))
        return NULL;

      if (dir == WebAccessibility::DIRECTION_NEXT) {
        child_obj = child_obj->nextSibling();
      } else {
        child_obj = child_obj->previousSibling();
      }
      break;
    }
    default:
      return NULL;
  }

  if (!child_obj)
    return NULL;

  // TODO(klink): simple object child?
  ToWrapper(child_obj)->ref();
  return ToWrapper(child_obj);
}

GlueAccessibilityObject* GlueAccessibilityObject::GetChild(int child_id) {
  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  // TODO(klink): simple object child?
  ToWrapper(child_obj)->ref();
  return ToWrapper(child_obj);
}

bool GlueAccessibilityObject::ChildCount(long* count) {
  if (!m_object || !count)
    return false;

  *count = static_cast<long>(m_object->children().size());
  return true;
}

bool GlueAccessibilityObject::DefaultAction(int child_id, String* action) {
  if (!action)
    return false;

  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  *action = child_obj->actionVerb();
  return !action->isEmpty();
}

bool GlueAccessibilityObject::Description(int child_id, String* description) {
  if (!description)
    return false;

  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  // TODO(klink): Description, for SELECT subitems, should be a string
  // describing the position of the item in its group and of the group in the
  // list (see Firefox).
  *description = ToWrapper(child_obj)->description();
  return !description->isEmpty();
}

GlueAccessibilityObject* GlueAccessibilityObject::GetFocusedChild() {
  if (!m_object)
    return NULL;

  AccessibilityObject* focused_obj = m_object->focusedUIElement();
  if (!focused_obj)
    return NULL;

  // Only return the focused child if it's us or a child of us.
  if (focused_obj == m_object || focused_obj->parentObject() == m_object) {
    ToWrapper(focused_obj)->ref();
    return ToWrapper(focused_obj);
  }
  return NULL;
}

bool GlueAccessibilityObject::HelpText(int child_id, String* help) {
  if (!help)
    return false;

  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  *help = child_obj->helpText();
  return !help->isEmpty();
}

bool GlueAccessibilityObject::KeyboardShortcut(int child_id, String* shortcut) {
  if (!shortcut)
    return false;

  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  String access_key = child_obj->accessKey();
  if (access_key.isNull())
    return false;

  static String access_key_modifiers;
  if (access_key_modifiers.isNull()) {
    unsigned modifiers = WebCore::EventHandler::accessKeyModifiers();
    // Follow the same order as Mozilla MSAA implementation:
    // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
    // should not be localized and defines the separator as "+".
    if (modifiers & WebCore::PlatformKeyboardEvent::CtrlKey)
        access_key_modifiers += "Ctrl+";
    if (modifiers & WebCore::PlatformKeyboardEvent::AltKey)
        access_key_modifiers += "Alt+";
    if (modifiers & WebCore::PlatformKeyboardEvent::ShiftKey)
        access_key_modifiers += "Shift+";
    if (modifiers & WebCore::PlatformKeyboardEvent::MetaKey)
        access_key_modifiers += "Win+";
  }
  *shortcut = access_key_modifiers + access_key;
  return !shortcut->isEmpty();
}

bool GlueAccessibilityObject::Name(int child_id, String* name) {
  if (!name)
    return false;

  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  *name = ToWrapper(child_obj)->name();
  return !name->isEmpty();
}

GlueAccessibilityObject* GlueAccessibilityObject::GetParent() {
  if (!m_object)
    return NULL;

  AccessibilityObject* parent_obj = m_object->parentObject();

  if (parent_obj) {
    ToWrapper(parent_obj)->ref();
    return ToWrapper(parent_obj);
  }
  // No valid parent, or parent is the containing window.
  return NULL;
}

bool GlueAccessibilityObject::Role(int child_id, long* role) {
  if (!role)
    return false;

  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  *role = ToWrapper(child_obj)->role();
  return true;
}

bool GlueAccessibilityObject::Value(int child_id, String* value) {
  if (!value)
    return false;

  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  *value = ToWrapper(child_obj)->value();
  return !value->isEmpty();
}

bool GlueAccessibilityObject::State(int child_id, long* state) {
  if (!state)
    return false;

  *state = 0;
  AccessibilityObject* child_obj;
  if (!GetAccessibilityObjectForChild(child_id, child_obj))
    return false;

  if (child_obj->isAnchor())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_LINKED);

  if (child_obj->isHovered())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_HOTTRACKED);

  if (!child_obj->isEnabled())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_UNAVAILABLE);

  if (child_obj->isReadOnly())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_READONLY);

  if (child_obj->isOffScreen())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_OFFSCREEN);

  if (child_obj->isMultiSelect())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_MULTISELECTABLE);

  if (child_obj->isPasswordField())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_PROTECTED);

  if (child_obj->isIndeterminate())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_INDETERMINATE);

  if (child_obj->isChecked())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_CHECKED);

  if (child_obj->isPressed())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_PRESSED);

  if (child_obj->isFocused())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_FOCUSED);

  if (child_obj->isVisited())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_TRAVERSED);

  if (child_obj->canSetFocusAttribute())
    *state |= static_cast<long>(1 << WebAccessibility::STATE_FOCUSABLE);

  // TODO(klink): Add selected and selectable states.

  return true;
}

// Helper functions
String GlueAccessibilityObject::name() const {
  return m_object->title();
}

String GlueAccessibilityObject::value() const {
  return m_object->stringValue();
}

String GlueAccessibilityObject::description() const {
  String desc = m_object->accessibilityDescription();
  if (desc.isNull())
      return desc;

  // From the Mozilla MSAA implementation:
  // "Signal to screen readers that this description is speakable and is not
  // a formatted positional information description. Don't localize the
  // 'Description: ' part of this string, it will be parsed out by assistive
  // technologies."
  return "Description: " + desc;
}

// Provides a conversion between the WebCore::AccessibilityRole and a
// role supported on the Browser side. Static function.
static WebAccessibility::Role SupportedRole(WebCore::AccessibilityRole role) {
  switch (role) {
    case WebCore::ButtonRole:
      return WebAccessibility::ROLE_PUSHBUTTON;
    case WebCore::RadioButtonRole:
      return WebAccessibility::ROLE_RADIOBUTTON;
    case WebCore::CheckBoxRole:
      return WebAccessibility::ROLE_CHECKBUTTON;
    case WebCore::SliderRole:
      return WebAccessibility::ROLE_SLIDER;
    case WebCore::TabGroupRole:
      return WebAccessibility::ROLE_PAGETABLIST;
    case WebCore::TextFieldRole:
    case WebCore::TextAreaRole:
    case WebCore::ListMarkerRole:
      return WebAccessibility::ROLE_TEXT;
    case WebCore::StaticTextRole:
      return WebAccessibility::ROLE_STATICTEXT;
    case WebCore::OutlineRole:
      return WebAccessibility::ROLE_OUTLINE;
    case WebCore::ColumnRole:
      return WebAccessibility::ROLE_COLUMN;
    case WebCore::RowRole:
      return WebAccessibility::ROLE_ROW;
    case WebCore::GroupRole:
      return WebAccessibility::ROLE_GROUPING;
    case WebCore::ListRole:
      return WebAccessibility::ROLE_LIST;
    case WebCore::TableRole:
      return WebAccessibility::ROLE_TABLE;
    case WebCore::LinkRole:
    case WebCore::WebCoreLinkRole:
      return WebAccessibility::ROLE_LINK;
    case WebCore::ImageMapRole:
    case WebCore::ImageRole:
      return WebAccessibility::ROLE_GRAPHIC;
    default:
        // This is the default role.
      return WebAccessibility::ROLE_CLIENT;
  }
}

WebAccessibility::Role GlueAccessibilityObject::role() const {
  return SupportedRole(m_object->roleValue());
}

bool GlueAccessibilityObject::GetAccessibilityObjectForChild(int child_id,
    AccessibilityObject*& child_obj) const {
  child_obj = 0;

  if (!m_object || child_id < 0)
    return false;

  if (child_id == 0) {
    child_obj = m_object;
  } else {
    size_t child_index = static_cast<size_t>(child_id - 1);

    if (child_index >= m_object->children().size())
      return false;
    child_obj = m_object->children().at(child_index).get();
  }

  if (!child_obj)
    return false;

  return true;
}

GlueAccessibilityObject* GlueAccessibilityObject::ToWrapper(
    AccessibilityObject* obj) {
  if (!obj)
    return NULL;

  GlueAccessibilityObject* result =
      static_cast<GlueAccessibilityObject*>(obj->wrapper());
  if (!result)
    result = CreateInstance(obj);

  return result;
}
