// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_

#include "app/table_model.h"
#include "chrome/browser/search_engines/template_url_model.h"

class ModelEntry;
class SkBitmap;
class TemplateURL;
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

class TemplateURLTableModel : public TableModel,
                                     TemplateURLModelObserver {
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
  virtual void SetObserver(TableModelObserver* observer);
  virtual bool HasGroups();
  virtual Groups GetGroups();
  virtual int GetGroupID(int row);

  // Removes the entry at the specified index.
  void Remove(int index);

  // Adds a new entry at the specified index.
  void Add(int index, TemplateURL* template_url);

  // Update the entry at the specified index.
  void ModifyTemplateURL(int index, const std::wstring& title,
                         const std::wstring& keyword, const std::wstring& url);

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
  int MoveToMainGroup(int index);

  // Make the TemplateURL at |index| the default.  Returns the new index, or -1
  // if the index is invalid or it is already the default.
  int MakeDefaultTemplateURL(int index);

  // If there is an observer, it's notified the selected row has changed.
  void NotifyChanged(int index);

  TemplateURLModel* template_url_model() const { return template_url_model_; }

  // Returns the index of the last entry shown in the search engines group.
  int last_search_engine_index() const { return last_search_engine_index_; }

 private:
  friend class ModelEntry;

  // Notification that a model entry has fetched its icon.
  void FavIconAvailable(ModelEntry* entry);

  // TemplateURLModelObserver notification.
  virtual void OnTemplateURLModelChanged();

  TableModelObserver* observer_;

  // The entries.
  std::vector<ModelEntry*> entries_;

  // The model we're displaying entries from.
  TemplateURLModel* template_url_model_;

  // Index of the last search engine in entries_. This is used to determine the
  // group boundaries.
  int last_search_engine_index_;

  DISALLOW_EVIL_CONSTRUCTORS(TemplateURLTableModel);
};


#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_
