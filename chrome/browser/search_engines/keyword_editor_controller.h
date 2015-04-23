// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_KEYWORD_EDITOR_CONTROLLER_H_
#define CHROME_BROWSER_SEARCH_ENGINES_KEYWORD_EDITOR_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

class Profile;
class TemplateURL;
class TemplateURLModel;
class TemplateURLTableModel;

class KeywordEditorController {
 public:
  explicit KeywordEditorController(Profile* profile);
  ~KeywordEditorController();

  // Invoked when the user succesfully fills out the add keyword dialog.
  // Propagates the change to the TemplateURLModel and updates the table model.
  // Returns the index of the added URL.
  int AddTemplateURL(const std::wstring& title,
                     const std::wstring& keyword,
                     const std::wstring& url);

  // Invoked when the user modifies a TemplateURL. Updates the TemplateURLModel
  // and table model appropriately.
  void ModifyTemplateURL(const TemplateURL* template_url,
                         const std::wstring& title,
                         const std::wstring& keyword,
                         const std::wstring& url);

  // Return true if the given |url| can be made the default.
  bool CanMakeDefault(const TemplateURL* url) const;

  // Return true if the given |url| can be removed.
  bool CanRemove(const TemplateURL* url) const;

  // Remove the TemplateURL at the specified index in the TableModel.
  void RemoveTemplateURL(int index);

  // Make the TemplateURL at the specified index (into the TableModel) the
  // default search provider.  Return the new index, or -1 if nothing was done.
  int MakeDefaultTemplateURL(int index);

  // Return true if the |url_model_| data is loaded.
  bool loaded() const;

  // Return the TemplateURL corresponding to the |index| in the model.
  const TemplateURL* GetTemplateURL(int index) const;

  TemplateURLTableModel* table_model() {
    return table_model_.get();
  }

  TemplateURLModel* url_model() const;

 private:
  // The profile.
  Profile* profile_;

  // Model for the TableView.
  scoped_ptr<TemplateURLTableModel> table_model_;

  DISALLOW_COPY_AND_ASSIGN(KeywordEditorController);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_KEYWORD_EDITOR_CONTROLLER_H_
