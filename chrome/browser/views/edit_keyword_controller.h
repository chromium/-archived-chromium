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

// EditKeywordController provides text fields for editing a keyword: the title,
// url and actual keyword. It is used by the KeywordEditorView of the Options
// dialog, and also on its own to confirm the addition of a keyword added by
// the ExternalJSObject via the RenderView.

#ifndef CHROME_BROWSER_VIEWS_EDIT_KEYWORD_CONTROLLER_H__
#define CHROME_BROWSER_VIEWS_EDIT_KEYWORD_CONTROLLER_H__

#include <Windows.h>

#include "chrome/views/dialog_delegate.h"
#include "chrome/views/text_field.h"

namespace ChromeViews {
  class Label;
  class ImageView;
  class Window;
}

class KeywordEditorView;
class Profile;
class TemplateURL;
class TemplateURLModel;

class EditKeywordController : public ChromeViews::TextField::Controller,
                              public ChromeViews::DialogDelegate {
 public:
  // The template_url and/or keyword_editor_view may be NULL.
  EditKeywordController(HWND parent,
                        const TemplateURL* template_url,
                        KeywordEditorView* keyword_editor_view,
                        Profile* profile);

  virtual ~EditKeywordController() {}

  // Shows the dialog to the user. EditKeywordController takes care of
  // deleting itself after show has been invoked.
  void Show();

  // DialogDelegate overrides.
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual int GetDialogButtons() const;
  virtual bool IsDialogButtonEnabled(DialogButton button) const;
  virtual void WindowClosing();
  virtual bool Cancel();
  virtual bool Accept();
  virtual ChromeViews::View* GetContentsView();

  // ChromeViews::TextField::Controller overrides. Updates whether the user can
  // accept the dialog as well as updating image views showing whether value is
  // valid.
  virtual void ContentsChanged(ChromeViews::TextField* sender,
                               const std::wstring& new_contents);
  virtual void HandleKeystroke(ChromeViews::TextField* sender,
                               UINT message,
                               TCHAR key,
                               UINT repeat_count,
                               UINT flags);

 private:
  void Init();

  // Create a Label containing the text with the specified message id.
  ChromeViews::Label* CreateLabel(int message_id);

  // Creates a text field with the specified text.
  ChromeViews::TextField* CreateTextField(const std::wstring& text);

  // Returns true if the currently input URL is valid. The URL is valid if it
  // contains no search terms and is a valid url, or if it contains a search
  // term and replacing that search term with a character results in a valid
  // url.
  bool IsURLValid() const;

  // Returns the URL the user has input. The returned URL is suitable for use
  // by TemplateURL.
  std::wstring GetURL() const;

  // Returns whether the currently entered keyword is valid. The keyword is
  // valid if it is non-empty and does not conflict with an existing entry.
  // NOTE: this is just the keyword, not the title and url.
  bool IsKeywordValid() const;

  // Invokes UpdateImageView for each of the images views.
  void UpdateImageViews();

  // Updates the tooltip and image of the image view based on is_valid. If
  // is_valid is false the tooltip of the image view is set to the message with
  // id invalid_message_id, otherwise the tooltip is set to the empty text.
  void UpdateImageView(ChromeViews::ImageView* image_view,
                       bool is_valid,
                       int invalid_message_id);

  // Deletes an unused TemplateURL, if its add was cancelled and it's not
  // already owned by the TemplateURLModel.
  void CleanUpCancelledAdd();

  // The TemplateURL we're displaying information for. It may be NULL. If we
  // have a keyword_editor_view, we assume that this TemplateURL is already in
  // the TemplateURLModel; if not, we assume it isn't.
  const TemplateURL* template_url_;

  // Used to parent window to. May be NULL or an invalid window.
  HWND parent_;

  // View containing the buttons, text fields ...
  ChromeViews::View* view_;

  // We may have been created by this, in which case we will call back to it on
  // success to add/modify the entry.  May be NULL.
  KeywordEditorView* keyword_editor_view_;

  // Profile whose TemplateURLModel we're modifying.
  Profile* profile_;

  // Text fields.
  ChromeViews::TextField* title_tf_;
  ChromeViews::TextField* keyword_tf_;
  ChromeViews::TextField* url_tf_;

  // Shows error images.
  ChromeViews::ImageView* title_iv_;
  ChromeViews::ImageView* keyword_iv_;
  ChromeViews::ImageView* url_iv_;

  DISALLOW_EVIL_CONSTRUCTORS(EditKeywordController);
};

#endif  // CHROME_BROWSER_VIEWS_EDIT_KEYWORD_CONTROLLER_H__