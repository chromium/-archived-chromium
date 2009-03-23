// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_KEYWORD_EDITOR_VIEW_H_
#define CHROME_BROWSER_VIEWS_KEYWORD_EDITOR_VIEW_H_

#include <Windows.h>
#include <map>

#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/views/controls/button/button.h"
#include "chrome/views/controls/table/table_view.h"
#include "chrome/views/view.h"
#include "chrome/views/window/dialog_delegate.h"

namespace views {
class Label;
class NativeButton;
}

namespace {
class BorderView;
}

class ModelEntry;
class SkBitmap;
class TemplateURLModel;
class TemplateURLTableModel;

// TemplateURLTableModel is the TableModel implementation used by
// KeywordEditorView to show the keywords in a TableView.
//
// TemplateURLTableModel has two columns, the first showing the description,
// the second the keyword.
//
// TemplateURLTableModel maintains a vector of ModelEntrys that correspond to
// each row in the tableview. Each ModelEntry wraps a TemplateURL, providing
// the favicon. The entries in the model are sorted such that non-generated
// appear first (grouped together) and are followed by generated keywords.

class TemplateURLTableModel : public views::TableModel {
 public:
  explicit TemplateURLTableModel(TemplateURLModel* template_url_model);

  virtual ~TemplateURLTableModel();

  // Reloads the entries from the TemplateURLModel. This should ONLY be invoked
  // if the TemplateURLModel wasn't initially loaded and has been loaded.
  void Reload();

  // TableModel overrides.
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column);
  virtual SkBitmap GetIcon(int row);
  virtual void SetObserver(views::TableModelObserver* observer);
  virtual bool HasGroups();
  virtual Groups GetGroups();
  virtual int GetGroupID(int row);

  // Removes the entry at the specified index. This does NOT propagate the
  // change to the backend.
  void Remove(int index);

  // Adds a new entry at the specified index. This does not propagate the
  // change to the backend.
  void Add(int index, const TemplateURL* template_url);

  // Reloads the icon at the specified index.
  void ReloadIcon(int index);

  // Returns The TemplateURL at the specified index.
  const TemplateURL& GetTemplateURL(int index);

  // Returns the index of the TemplateURL, or -1 if it the TemplateURL is not
  // found.
  int IndexOfTemplateURL(const TemplateURL* template_url);

  // Moves the keyword at the specified index to be at the end of the main
  // group. Returns the new index. This does nothing if the entry is already
  // in the main group.
  void MoveToMainGroup(int index);

  // If there is an observer, it's notified the selected row has changed.
  void NotifyChanged(int index);

  TemplateURLModel* template_url_model() const { return template_url_model_; }

  // Returns the index of the last entry shown in the search engines group.
  int last_search_engine_index() const { return last_search_engine_index_; }

 private:
  friend class ModelEntry;

  // Notification that a model entry has fetched its icon.
  void FavIconAvailable(ModelEntry* entry);

  views::TableModelObserver* observer_;

  // The entries.
  std::vector<ModelEntry*> entries_;

  // The model we're displaying entries from.
  TemplateURLModel* template_url_model_;

  // Index of the last search engine in entries_. This is used to determine the
  // group boundaries.
  int last_search_engine_index_;

  DISALLOW_EVIL_CONSTRUCTORS(TemplateURLTableModel);
};

// KeywordEditorView ----------------------------------------------------------

// KeywordEditorView allows the user to edit keywords.

class KeywordEditorView : public views::View,
                          public views::TableViewObserver,
                          public views::ButtonListener,
                          public TemplateURLModelObserver,
                          public views::DialogDelegate {
  friend class KeywordEditorViewTest;
  FRIEND_TEST(KeywordEditorViewTest, MakeDefault);
 public:
  // Shows the KeywordEditorView for the specified profile. If there is a
  // KeywordEditorView already open, it is closed and a new one is shown.
  static void Show(Profile* profile);

  explicit KeywordEditorView(Profile* profile);
  virtual ~KeywordEditorView();

  // Invoked when the user succesfully fills out the add keyword dialog.
  // Propagates the change to the TemplateURLModel and updates the table model.
  void AddTemplateURL(const std::wstring& title,
                      const std::wstring& keyword,
                      const std::wstring& url);

  // Invoked when the user modifies a TemplateURL. Updates the TemplateURLModel
  // and table model appropriately.
  void ModifyTemplateURL(const TemplateURL* template_url,
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

  // Returns the TemplateURLModel we're using.
  TemplateURLModel* template_url_model() const { return url_model_; }

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
  void MakeDefaultSearchProvider();

  // Make the TemplateURL at the specified index (into the TableModel) the
  // default search provider.
  void MakeDefaultSearchProvider(int index);

  // The profile.
  Profile* profile_;

  // Model containing TemplateURLs. We listen for changes on this and propagate
  // them to the table model.
  TemplateURLModel* url_model_;

  // Model for the TableView.
  scoped_ptr<TemplateURLTableModel> table_model_;

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
