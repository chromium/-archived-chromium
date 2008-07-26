// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/views/link.h"

#include "base/scoped_ptr.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/views/event.h"

namespace ChromeViews {

static HCURSOR g_hand_cursor = NULL;

// Default colors used for links.
static const SkColor kHighlightedColor = SkColorSetRGB(255, 0x00, 0x00);
static const SkColor kNormalColor = SkColorSetRGB(0, 51, 153);
static const SkColor kDisabledColor = SkColorSetRGB(0, 0, 0);

const char Link::kViewClassName[] = "chrome/views/Link";

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
                 HitTest(e.GetLocation()));
  return true;
}

void Link::OnMouseReleased(const MouseEvent& e, bool canceled) {
  // Change the highlight first just in case this instance is deleted
  // while calling the controller
  SetHighlighted(false);
  if (enabled_ && !canceled &&
      (e.IsLeftMouseButton() || e.IsMiddleMouseButton()) &&
      HitTest(e.GetLocation())) {
    // Focus the link on click.
    RequestFocus();

    if (controller_)
      controller_->LinkActivated(this, e.GetFlags());
  }
}

bool Link::OnKeyPressed(const KeyEvent& e) {
  if ((e.GetCharacter() == L' ') || (e.GetCharacter() == L'\n')) {
    SetHighlighted(true);
    return true;
  }
  return false;
}

bool Link::OnKeyReleased(const KeyEvent& e) {
  if ((e.GetCharacter() == L' ') || (e.GetCharacter() == L'\n')) {
    SetHighlighted(false);

    // Focus the link on key pressed.
    RequestFocus();

    if (controller_)
      controller_->LinkActivated(this, e.GetFlags());

    return true;
  }
  return false;
}

void Link::SetHighlighted(bool f) {
  if (f != highlighted_) {
    highlighted_ = f;
    ValidateStyle();
    SchedulePaint();
  }
}

void Link::ValidateStyle() {
  ChromeFont font = GetFont();

  if (enabled_) {
    if ((font.style() & ChromeFont::UNDERLINED) == 0) {
      Label::SetFont(font.DeriveFont(0, font.style() |
                                     ChromeFont::UNDERLINED));
    }
  } else {
    if ((font.style() & ChromeFont::UNDERLINED) != 0) {
      Label::SetFont(font.DeriveFont(0, font.style() &
                                     ~ChromeFont::UNDERLINED));
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

void Link::SetFont(const ChromeFont& font) {
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

HCURSOR Link::GetCursorForPoint(Event::EventType event_type, int x, int y) {
  if (enabled_) {
    if (!g_hand_cursor) {
      g_hand_cursor = LoadCursor(NULL, IDC_HAND);
    }
    return g_hand_cursor;
  } else {
    return NULL;
  }
}

}
