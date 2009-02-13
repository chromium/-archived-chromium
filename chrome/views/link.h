// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_LINK_H__
#define CHROME_VIEWS_LINK_H__

#include "chrome/views/label.h"

namespace views {

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
  virtual bool OverrideAccelerator(const Accelerator& accelerator);

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

  DISALLOW_COPY_AND_ASSIGN(Link);
};

}  // namespace views

#endif  // CHROME_VIEWS_LINK_H__

