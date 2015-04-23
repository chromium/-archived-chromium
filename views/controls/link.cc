// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/link.h"

#if defined(OS_LINUX)
#include <gdk/gdk.h>
#endif

#include "app/gfx/font.h"
#include "base/logging.h"
#include "views/event.h"

namespace views {

#if defined(OS_WIN)
static HCURSOR g_hand_cursor = NULL;
#endif

// Default colors used for links.
static const SkColor kHighlightedColor = SkColorSetRGB(255, 0x00, 0x00);
static const SkColor kNormalColor = SkColorSetRGB(0, 51, 153);
static const SkColor kDisabledColor = SkColorSetRGB(0, 0, 0);

const char Link::kViewClassName[] = "views/Link";

Link::Link() : Label(L""),
               controller_(NULL),
               highlighted_(false),
               highlighted_color_(kHighlightedColor),
               disabled_color_(kDisabledColor),
               normal_color_(kNormalColor) {
  Init();
  SetFocusable(true);
}

Link::Link(const std::wstring& title) : Label(title),
                                        controller_(NULL),
                                        highlighted_(false),
                                        highlighted_color_(kHighlightedColor),
                                        disabled_color_(kDisabledColor),
                                        normal_color_(kNormalColor) {
  Init();
  SetFocusable(true);
}

void Link::Init() {
  SetColor(normal_color_);
  ValidateStyle();
}

Link::~Link() {
}

void Link::SetController(LinkController* controller) {
  controller_ = controller;
}

const LinkController* Link::GetController() {
  return controller_;
}

std::string Link::GetClassName() const {
  return kViewClassName;
}

void Link::SetHighlightedColor(const SkColor& color) {
  normal_color_ = color;
  ValidateStyle();
}

void Link::SetDisabledColor(const SkColor& color) {
  disabled_color_ = color;
  ValidateStyle();
}

void Link::SetNormalColor(const SkColor& color) {
  normal_color_ = color;
  ValidateStyle();
}

bool Link::OnMousePressed(const MouseEvent& e) {
  if (!enabled_ || (!e.IsLeftMouseButton() && !e.IsMiddleMouseButton()))
    return false;
  SetHighlighted(true);
  return true;
}

bool Link::OnMouseDragged(const MouseEvent& e) {
  SetHighlighted(enabled_ &&
                 (e.IsLeftMouseButton() || e.IsMiddleMouseButton()) &&
                 HitTest(e.location()));
  return true;
}

void Link::OnMouseReleased(const MouseEvent& e, bool canceled) {
  // Change the highlight first just in case this instance is deleted
  // while calling the controller
  SetHighlighted(false);
  if (enabled_ && !canceled &&
      (e.IsLeftMouseButton() || e.IsMiddleMouseButton()) &&
      HitTest(e.location())) {
    // Focus the link on click.
    RequestFocus();

    if (controller_)
      controller_->LinkActivated(this, e.GetFlags());
  }
}

bool Link::OnKeyPressed(const KeyEvent& e) {
#if defined(OS_WIN)
  bool activate = ((e.GetCharacter() == VK_SPACE) ||
                   (e.GetCharacter() == VK_RETURN));
#else
  bool activate = false;
  NOTIMPLEMENTED();
#endif
  if (activate) {
    SetHighlighted(false);

    // Focus the link on key pressed.
    RequestFocus();

    if (controller_)
      controller_->LinkActivated(this, e.GetFlags());

    return true;
  }
  return false;
}

bool Link::SkipDefaultKeyEventProcessing(const KeyEvent& e) {
#if defined(OS_WIN)
  // Make sure we don't process space or enter as accelerators.
  return (e.GetCharacter() == VK_SPACE) || (e.GetCharacter() == VK_RETURN);
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

void Link::SetHighlighted(bool f) {
  if (f != highlighted_) {
    highlighted_ = f;
    ValidateStyle();
    SchedulePaint();
  }
}

void Link::ValidateStyle() {
  gfx::Font font = GetFont();

  if (enabled_) {
    if ((font.style() & gfx::Font::UNDERLINED) == 0) {
      Label::SetFont(font.DeriveFont(0, font.style() |
                                     gfx::Font::UNDERLINED));
    }
  } else {
    if ((font.style() & gfx::Font::UNDERLINED) != 0) {
      Label::SetFont(font.DeriveFont(0, font.style() &
                                     ~gfx::Font::UNDERLINED));
    }
  }

  if (enabled_) {
    if (highlighted_) {
      Label::SetColor(highlighted_color_);
    } else {
      Label::SetColor(normal_color_);
    }
  } else {
    Label::SetColor(disabled_color_);
  }
}

void Link::SetFont(const gfx::Font& font) {
  Label::SetFont(font);
  ValidateStyle();
}

void Link::SetEnabled(bool f) {
  if (f != enabled_) {
    enabled_ = f;
    ValidateStyle();
    SchedulePaint();
  }
}

gfx::NativeCursor Link::GetCursorForPoint(Event::EventType event_type, int x,
                                          int y) {
  if (enabled_) {
#if defined(OS_WIN)
    if (!g_hand_cursor)
      g_hand_cursor = LoadCursor(NULL, IDC_HAND);
    return g_hand_cursor;
#elif defined(OS_LINUX)
    return gdk_cursor_new(GDK_HAND2);
#endif
  } else {
    return NULL;
  }
}

}  // namespace views
