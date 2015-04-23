// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_KEYWORD_EDITOR_VIEW_H_
#define CHROME_BROWSER_VIEWS_KEYWORD_EDITOR_VIEW_H_

#include <Windows.h>
#include <map>

#include "app/table_model.h"
#include "chrome/browser/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/search_engines/keyword_editor_controller.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "views/controls/button/button.h"
#include "views/controls/table/table_view_observer.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Label;
class NativeButton;
}

namespace {
class BorderView;
}

class SkBitmap;
class TemplateURLModel;
class TemplateURLTableModel;

// KeywordEditorView allows the user to edit keywords.

class KeywordEditorView : public views::View,
                          public views::TableViewObserver,
                          public views::ButtonListener,
                          public TemplateURLModelObserver,
                          public views::DialogDelegate,
                          public EditSearchEngineControllerDelegate {
 public:
  // Shows the KeywordEditorView for the specified profile. If there is a
  // KeywordEditorView already open, it is closed and a new one is shown.
  static void Show(Profile* profile);

  explicit KeywordEditorView(Profile* profile);
  virtual ~KeywordEditorView();

  // Overridden from EditSearchEngineControllerDelegate.
  // Calls AddTemplateURL or ModifyTemplateURL as appropriate.
  virtual void OnEditedKeyword(const TemplateURL* template_url,
                               const std::wstring& title,
                               const std::wstring& keyword,
                               const std::wstring& url);

  // Overriden to invoke Layout.
  virtual gfx::Size GetPreferredSize();

  // DialogDelegate methods:
  virtual bool CanResize() const;
  virtual std::wstring GetWindowTitle() const;
  virtual int GetDialogButtons() const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual views::View* GetContentsView();

 private:
  void Init();

  // Creates the layout and adds the views to it.
  void InitLayoutManager();

  // TableViewObserver method. Updates buttons contingent on the selection.
  virtual void OnSelectionChanged();
  // Edits the selected item.
  virtual void OnDoubleClick();

  // Button::ButtonListener method.
  virtual void ButtonPressed(views::Button* sender);

  // TemplateURLModelObserver notification.
  virtual void OnTemplateURLModelChanged();

  // Toggles whether the selected keyword is the default search provider.
  void MakeDefaultTemplateURL();

  // The profile.
  Profile* profile_;

  scoped_ptr<KeywordEditorController> controller_;

  // All the views are added as children, so that we don't need to delete
  // them directly.
  views::TableView* table_view_;
  views::NativeButton* add_button_;
  views::NativeButton* edit_button_;
  views::NativeButton* remove_button_;
  views::NativeButton* make_default_button_;

  DISALLOW_COPY_AND_ASSIGN(KeywordEditorView);
};

#endif  // CHROME_BROWSER_VIEWS_KEYWORD_EDITOR_VIEW_H_
