// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_ABOUT_CHROME_VIEW_H_
#define CHROME_BROWSER_VIEWS_ABOUT_CHROME_VIEW_H_

#include "chrome/browser/google_update.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/image_view.h"
#include "chrome/views/label.h"
#include "chrome/views/link.h"
#include "chrome/views/view.h"

namespace ChromeViews {
  class TextField;
  class Throbber;
  class Window;
}

class Profile;

////////////////////////////////////////////////////////////////////////////////
//
// The AboutChromeView class is responsible for drawing the UI controls of the
// About Chrome dialog that allows the user to see what version is installed
// and check for updates.
//
////////////////////////////////////////////////////////////////////////////////
class AboutChromeView : public ChromeViews::View,
                        public ChromeViews::DialogDelegate,
                        public ChromeViews::LinkController,
                        public GoogleUpdateStatusListener {
 public:
  explicit AboutChromeView(Profile* profile);
  virtual ~AboutChromeView();

  // Initialize the controls on the dialog.
  void Init();

  // Overridden from ChromeViews::View:
  virtual void GetPreferredSize(CSize *out);
  virtual void Layout();
  virtual void Paint(ChromeCanvas* canvas);
  virtual void ViewHierarchyChanged(bool is_add,
                                    ChromeViews::View* parent,
                                    ChromeViews::View* child);

  // Overridden from ChromeViews::DialogDelegate:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;
  virtual bool IsDialogButtonEnabled(DialogButton button) const;
  virtual bool IsDialogButtonVisible(DialogButton button) const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual ChromeViews::View* GetContentsView();

  // Overridden from ChromeViews::LinkController:
  virtual void LinkActivated(ChromeViews::Link* source, int event_flags);

  // Overridden from GoogleUpdateStatusListener:
  virtual void OnReportResults(GoogleUpdateUpgradeResult result,
                               GoogleUpdateErrorCode error_code,
                               const std::wstring& version);

 private:
  // The visible state of the Check For Updates button.
  enum CheckButtonStatus {
    CHECKBUTTON_HIDDEN = 0,
    CHECKBUTTON_DISABLED,
    CHECKBUTTON_ENABLED,
  };

  // Update the UI to show the status of the upgrade.
  void UpdateStatus(GoogleUpdateUpgradeResult result,
                    GoogleUpdateErrorCode error_code);

  // Draws a string onto the canvas (wrapping if needed) while also keeping
  // track of where it ends so we can position a URL after the text. The
  // parameter |bounds| represents the boundary we have to work with, |position|
  // specifies where to draw the string (relative to the top left corner of the
  // |bounds| rectangle and |font| specifies the font to use when drawing. When
  // the function returns, the parameter |rect| contains where to draw the URL
  // (to the right of where we just drew the text) and |position| is updated to
  // reflect where to draw the next string after the URL.
  // NOTE: The reason why we need this function is because while Skia knows how
  // to wrap text appropriately, it doesn't tell us where it drew the last
  // character, which we need to position the URLs within the text.
  void DrawTextAndPositionUrl(ChromeCanvas* canvas,
                              const std::wstring& text,
                              ChromeViews::Link* link,
                              gfx::Rect* rect,
                              CSize* position,
                              const CRect& bounds,
                              const ChromeFont& font);

  // A helper function for DrawTextAndPositionUrl, which simply draws the text
  // from a certain starting point |position| and wraps within bounds. For
  // details on the parameters, see DrawTextAndPositionUrl.
  void DrawTextStartingFrom(ChromeCanvas* canvas,
                            const std::wstring& text,
                            CSize* position,
                            const CRect& bounds,
                            const ChromeFont& font);

  // A simply utility function that calculates whether a word of width
  // |word_width| fits at position |position| within the |bounds| rectangle. If
  // not, |position| is updated to wrap to the beginning of the next line.
  void WrapIfWordDoesntFit(int word_width,
                           int font_height,
                           CSize* position,
                           const CRect& bounds);

  Profile* profile_;

  // UI elements on the dialog.
  ChromeViews::ImageView* about_dlg_background_;
  ChromeViews::Label* about_title_label_;
  ChromeViews::TextField* version_label_;
  ChromeViews::Label* copyright_label_;
  ChromeViews::Label* main_text_label_;
  int main_text_label_height_;
  ChromeViews::Link* chromium_url_;
  gfx::Rect chromium_url_rect_;
  ChromeViews::Link* open_source_url_;
  gfx::Rect open_source_url_rect_;
  ChromeViews::Link* terms_of_service_url_;
  gfx::Rect terms_of_service_url_rect_;
  // UI elements we add to the parent view.
  scoped_ptr<ChromeViews::Throbber> throbber_;
  ChromeViews::ImageView success_indicator_;
  ChromeViews::ImageView update_available_indicator_;
  ChromeViews::ImageView timeout_indicator_;
  ChromeViews::Label update_label_;

  // Keeps track of the visible state of the Check For Updates button.
  CheckButtonStatus check_button_status_;

  // The text to display as the main label of the About box. We draw this text
  // word for word with the help of the WordIterator, and make room for URLs
  // which are drawn using ChromeViews::Link. See also |url_offsets_|.
  std::wstring main_label_chunk1_;
  std::wstring main_label_chunk2_;
  std::wstring main_label_chunk3_;
  std::wstring main_label_chunk4_;
  std::wstring main_label_chunk5_;
  // Determines the order of the two links we draw in the main label.
  bool chromium_url_appears_first_;

  // The class that communicates with Google Update to find out if an update is
  // available and asks it to start an upgrade.
  scoped_refptr<GoogleUpdate> google_updater_;

  // Our current version.
  std::wstring current_version_;

  // The version Google Update reports is available to us.
  std::wstring new_version_available_;

  DISALLOW_COPY_AND_ASSIGN(AboutChromeView);
};

#endif  // CHROME_BROWSER_VIEWS_ABOUT_CHROME_VIEW_H_

