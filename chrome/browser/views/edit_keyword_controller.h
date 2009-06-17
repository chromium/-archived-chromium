// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// EditKeywordController provides text fields for editing a keyword: the title,
// url and actual keyword. It is used by the KeywordEditorView of the Options
// dialog, and also on its own to confirm the addition of a keyword added by
// the ExternalJSObject via the RenderView.

#ifndef CHROME_BROWSER_VIEWS_EDIT_KEYWORD_CONTROLLER_H_
#define CHROME_BROWSER_VIEWS_EDIT_KEYWORD_CONTROLLER_H_

#include <windows.h>

#include "chrome/browser/search_engines/edit_keyword_controller_base.h"
#include "views/controls/textfield/textfield.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Label;
class ImageView;
class Window;
}

class Profile;
class TemplateURL;
class TemplateURLModel;

class EditKeywordController : public views::Textfield::Controller,
                              public views::DialogDelegate,
                              public EditKeywordControllerBase {
 public:
  // The |template_url| and/or |delegate| may be NULL.
  EditKeywordController(HWND parent,
                        const TemplateURL* template_url,
                        Delegate* delegate,
                        Profile* profile);

  virtual ~EditKeywordController() {}

  // Shows the dialog to the user. EditKeywordController takes care of
  // deleting itself after show has been invoked.
  void Show();

  // DialogDelegate overrides.
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual void DeleteDelegate();
  virtual bool Cancel();
  virtual bool Accept();
  virtual views::View* GetContentsView();

  // views::Textfield::Controller overrides. Updates whether the user can
  // accept the dialog as well as updating image views showing whether value is
  // valid.
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& key);

 private:
  void Init();

  // Create a Label containing the text with the specified message id.
  views::Label* CreateLabel(int message_id);

  // Creates a text field with the specified text. If |lowercase| is true, the
  // Textfield is configured to map all input to lower case.
  views::Textfield* CreateTextfield(const std::wstring& text, bool lowercase);

  // EditKeywordControllerBase overrides
  virtual std::wstring GetURLInput() const;
  virtual std::wstring GetKeywordInput() const;
  virtual std::wstring GetTitleInput() const;

  // Invokes UpdateImageView for each of the images views.
  void UpdateImageViews();

  // Updates the tooltip and image of the image view based on is_valid. If
  // is_valid is false the tooltip of the image view is set to the message with
  // id invalid_message_id, otherwise the tooltip is set to the empty text.
  void UpdateImageView(views::ImageView* image_view,
                       bool is_valid,
                       int invalid_message_id);

  // Used to parent window to. May be NULL or an invalid window.
  HWND parent_;

  // View containing the buttons, text fields ...
  views::View* view_;

  // Text fields.
  views::Textfield* title_tf_;
  views::Textfield* keyword_tf_;
  views::Textfield* url_tf_;

  // Shows error images.
  views::ImageView* title_iv_;
  views::ImageView* keyword_iv_;
  views::ImageView* url_iv_;

  DISALLOW_COPY_AND_ASSIGN(EditKeywordController);
};

#endif  // CHROME_BROWSER_VIEWS_EDIT_KEYWORD_CONTROLLER_H_
