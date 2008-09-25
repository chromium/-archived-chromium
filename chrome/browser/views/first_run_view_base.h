// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_BASE_H_
#define CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_BASE_H_

#include "chrome/browser/importer/importer.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/view.h"

namespace ChromeViews {
class CheckBox;
class Window;
class ImageView;
class Separator;
class Throbber;
}

class Profile;
class ImporterHost;

// This class abstracts the code that creates the dialog look for the two
// first-run dialogs. This amounts to the bitmap, the two separators, the
// progress throbber and some common resize code.
class FirstRunViewBase : public ChromeViews::View,
                         public ChromeViews::DialogDelegate {
 public:
  explicit FirstRunViewBase(Profile* profile);
  virtual ~FirstRunViewBase();

  // Overridden from ChromeViews::View.
  virtual void Layout();

  // Overridden from ChromeViews::WindowDelegate.
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;

  // Overridden from ChromeViews::DialogDelegate.
  std::wstring GetDialogButtonLabel(DialogButton button) const;

 protected:
  // Returns the items that the first run process is required to import
  // from other browsers.
  int GetDefaultImportItems() const;

  // Creates the desktop and quick launch shortcut. Existing shortcut is lost.
  bool CreateDesktopShortcut();
  bool CreateQuickLaunchShortcut();

  // Set us as default browser if the user checked the box.
  bool SetDefaultBrowser();

  // Modifies the chrome configuration so that the first-run dialogs are not
  // shown again.
  bool FirstRunComplete();

  // Disables the standard buttons of the dialog. Useful when importing.
  void DisableButtons();
  // Computes a tight dialog width given a contained UI element.
  void AdjustDialogWidth(const ChromeViews::View* sub_view);

  // Sets a minimum dialog size.
  void SetMinimumDialogWidth(int width);

  // Returns the background image. It is useful for getting the metrics.
  const ChromeViews::ImageView* background_image() const {
    return background_image_;
  }
  // Returns the computed preferred width of the dialog. This value can change
  // when AdjustDialogWidth() is called during layout.
  int preferred_width() const {
    return preferred_width_;
  }

  scoped_refptr<ImporterHost> importer_host_;
  Profile* profile_;
  ChromeViews::CheckBox* default_browser_;

 private:
  // Initializes the controls on the dialog.
  void SetupControls();
  ChromeViews::ImageView* background_image_;
  ChromeViews::Separator* separator_1_;
  ChromeViews::Separator* separator_2_;
  int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunViewBase);
};

#endif  // CHROME_BROWSER_VIEWS_FIRST_RUN_VIEW_BASE_H_

