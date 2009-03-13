// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOOLBAR_MODEL_H__
#define CHROME_BROWSER_TOOLBAR_MODEL_H__

#include <string>

#include "base/basictypes.h"
#include "skia/include/SkColor.h"

class NavigationController;
class NavigationEntry;

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
// Sub-classes have only need to implement GetNavigationController().
class ToolbarModel {
 public:
  enum SecurityLevel {
    SECURE = 0,
    NORMAL,
    INSECURE
  };

  enum Icon {
    NO_ICON = 0,
    LOCK_ICON,
    WARNING_ICON
  };

  ToolbarModel();
  ~ToolbarModel();

  // Returns the text that should be displayed in the location bar.
  // Default value: empty string.
  virtual std::wstring GetText();

  // Returns the security level that the toolbar should display.
  // Default value: NORMAL.
  virtual SecurityLevel GetSecurityLevel();

  // Returns the security level that should be used in the scheme part of the
  // displayed URL.  If SECURE, then the scheme is painted in green.  If
  // INSECURE, it is painted in red and stricken-out.
  // Default value: NORMAL.
  virtual SecurityLevel GetSchemeSecurityLevel();

  // Returns the icon that should be displayed on the right of the location bar.
  // Default value: NO_ICON.
  virtual Icon GetIcon();

  // Sets the text and color of the text displayed in the info bubble that
  // appears when the user hovers the mouse over the icon.
  // Default value: empty string.
  virtual void GetIconHoverText(std::wstring* text, SkColor* text_color);

  // Sets |text| to contain the text that should be displayed on the right of
  // the location bar, and |tooltip| to the tooltip text that should be shown
  // when the mouse hover over that info label.
  // Default value: empty string.
  virtual void GetInfoText(std::wstring* text,
                           SkColor* text_color,
                           std::wstring* tooltip);

  // Getter/setter of whether the text in location bar is currently being
  // edited.
  virtual void set_input_in_progress(bool value) { input_in_progress_ = value; }
  virtual bool input_in_progress() const { return input_in_progress_; }

 protected:
  // Returns the navigation controller used to retrieve the navigation entry
  // from which the states are retrieved.
  // If this returns NULL, default values are used.
  virtual NavigationController* GetNavigationController() = 0;

 private:
  // Builds a short error message from the SSL status code found in |entry|.
  // The message is set in |text|.
  void CreateErrorText(NavigationEntry* entry, std::wstring* text);

  // Whether the text in the location bar is currently being edited.
  bool input_in_progress_;

  DISALLOW_EVIL_CONSTRUCTORS(ToolbarModel);
};

#endif  // CHROME_BROWSER_TOOLBAR_MODEL_H__
