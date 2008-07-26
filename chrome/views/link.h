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

#ifndef CHROME_VIEWS_LINK_H__
#define CHROME_VIEWS_LINK_H__

#include "chrome/views/label.h"

namespace ChromeViews {

class Link;

////////////////////////////////////////////////////////////////////////////////
//
// LinkController defines the method that should be implemented to
// receive a notification when a link is clicked
//
////////////////////////////////////////////////////////////////////////////////
class LinkController {
 public:
  virtual void LinkActivated(Link* source, int event_flags) = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// Link class
//
// A Link is a label subclass that looks like an HTML link. It has a
// controller which is notified when a click occurs.
//
////////////////////////////////////////////////////////////////////////////////
class Link : public Label {
 public:
  static const char Link::kViewClassName[];

  Link();
  Link(const std::wstring& title);
  virtual ~Link();

  void SetController(LinkController* controller);
  const LinkController* GetController();

  // Overridden from View:
  virtual bool OnMousePressed(const MouseEvent& event);
  virtual bool OnMouseDragged(const MouseEvent& event);
  virtual void OnMouseReleased(const MouseEvent& event,
                               bool canceled);
  virtual bool OnKeyPressed(const KeyEvent& e);
  virtual bool OnKeyReleased(const KeyEvent& e);

  virtual void SetFont(const ChromeFont& font);

  // Set whether the link is enabled.
  virtual void SetEnabled(bool f);

  virtual HCURSOR GetCursorForPoint(Event::EventType event_type, int x, int y);

  virtual std::string GetClassName() const;

  void SetHighlightedColor(const SkColor& color);
  void SetDisabledColor(const SkColor& color);
  void SetNormalColor(const SkColor& color);

 private:

  // A highlighted link is clicked.
  void SetHighlighted(bool f);

  // Make sure the label style matched the current state.
  void ValidateStyle();

  void Init();

  LinkController* controller_;

  // Whether the link is currently highlighted.
  bool highlighted_;

  // The color when the link is highlighted.
  SkColor highlighted_color_;

  // The color when the link is disabled.
  SkColor disabled_color_;

  // The color when the link is neither highlighted nor disabled.
  SkColor normal_color_;
};
}
#endif  // CHROME_VIEWS_LINK_H__
