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

#ifndef CHROME_BROWSER_VIEWS_KEYWORD_EDITOR_VIEW_H__
#define CHROME_BROWSER_VIEWS_KEYWORD_EDITOR_VIEW_H__

#include <Windows.h>
#include <map>

#include "base/logging.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/views/dialog_delegate.h"
#include "chrome/views/native_button.h"
#include "chrome/views/table_view.h"
#include "chrome/views/view.h"

namespace ChromeViews {
class CheckBox;
class Label;
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

class TemplateURLTableModel : public ChromeViews::TableModel {
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
  virtual void SetObserver(ChromeViews::TableModelObserver* observer);
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

  ChromeViews::TableModelObserver* observer_;

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

class KeywordEditorView : public ChromeViews::View,
                          public ChromeViews::TableViewObserver,
                          public ChromeViews::NativeButton::Listener,
                          public TemplateURLModelObserver,
                          public ChromeViews::DialogDelegate {
 public:
  static void RegisterUserPrefs(PrefService* prefs);

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

  // Invoked when the user modifies a TemplateURL. Update the TemplateURLModel
  // and table model appropriatley.
  void ModifyTemplateURL(const TemplateURL* template_url,
                         const std::wstring& title,
                         const std::wstring& keyword,
                         const std::wstring& url);

  // Overriden to invoke Layout.
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual void GetPreferredSize(CSize* out);

  // DialogDelegate methods:
  virtual bool CanResize() const;
  virtual std::wstring GetWindowTitle() const;
  virtual int GetDialogButtons() const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual ChromeViews::View* GetContentsView();

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
  virtual void ButtonPressed(ChromeViews::NativeButton* sender);

  // TemplateURLModelObserver notification.
  virtual void OnTemplateURLModelChanged();

  // Toggles whether the selected keyword is the default search provider.
  void MakeDefaultSearchProvider();

  // The profile.
  Profile* profile_;

  // Model containing TemplateURLs. We listen for changes on this and propagate
  // them to the table model.
  TemplateURLModel* url_model_;

  // Model for the TableView.
  scoped_ptr<TemplateURLTableModel> table_model_;

  // All the views are added as children, so that we don't need to delete
  // them directly.
  ChromeViews::TableView* table_view_;
  ChromeViews::NativeButton* add_button_;
  ChromeViews::NativeButton* edit_button_;
  ChromeViews::NativeButton* remove_button_;
  ChromeViews::NativeButton* make_default_button_;
  ChromeViews::CheckBox* enable_suggest_checkbox_;

  DISALLOW_EVIL_CONSTRUCTORS(KeywordEditorView);
};

#endif  // CHROME_BROWSER_VIEWS_KEYWORD_EDITOR_VIEW_H__
