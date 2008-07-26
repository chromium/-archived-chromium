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

#ifndef CHROME_BROWSER_HISTORY_TAB_UI_H__
#define CHROME_BROWSER_HISTORY_TAB_UI_H__

#include "chrome/browser/native_ui_contents.h"

class BaseHistoryModel;
class HistoryView;

// HistoryTabUI -------------------------------------------------------------

// HistoryTabUI provides the glue to make HistoryView available in
// NativeUIContents.

class HistoryTabUI : public NativeUI,
                     public SearchableUIContainer::Delegate {
 public:
  // Creates the HistoryTabUI. You must invoke Init to finish initialization.
  HistoryTabUI(NativeUIContents* contents);
  virtual ~HistoryTabUI() {}

  // Creates the model and view.
  void Init();

  virtual const std::wstring GetTitle() const;
  virtual const int GetFavIconID() const;
  virtual const int GetSectionIconID() const;
  virtual const std::wstring GetSearchButtonText() const;
  virtual ChromeViews::View* GetView();
  virtual void WillBecomeVisible(NativeUIContents* parent);
  virtual void WillBecomeInvisible(NativeUIContents* parent);
  virtual void Navigate(const PageState& state);
  virtual bool SetInitialFocus();

  // Return the URL that can be used to show this view in a NativeUIContents.
  static GURL GetURL();

  // Return the NativeUIFactory object for application views. This object is
  // owned by the caller.
  static NativeUIFactory* GetNativeUIFactory();

  // Returns a URL that shows history tab ui with the search text set to
  // text.
  static const GURL GetHistoryURLWithSearchText(const std::wstring& text);

  // Returns the model.
  BaseHistoryModel* model() const { return model_; }

  // Returns the NativeUIContents we're contained in.
  NativeUIContents* contents() const { return contents_; }

 protected:
  // Returns the SearchableUIContainer that contains the HistoryView.
  SearchableUIContainer* searchable_container() {
    return &searchable_container_;
  }

  // Creates the HistoryModel.
  virtual BaseHistoryModel* CreateModel();

  // Creates the HistoryView.
  virtual HistoryView* CreateHistoryView();

  // Invoked after refreshing the model, or resetting the search text.
  // This implementation updates whether the delete controls should be shown
  // as well as sending UMA metrics.
  virtual void ChangedModel();

 private:
  // Updates the query text of the model.
  virtual void DoSearch(const std::wstring& text);

  // Our host.
  NativeUIContents* contents_;

  // The view we return from GetView. The contents of this is the
  // bookmarks_view_
  SearchableUIContainer searchable_container_;

  // The model. This is deleted by the HistoryView we create.
  BaseHistoryModel* model_;

  DISALLOW_EVIL_CONSTRUCTORS(HistoryTabUI);
};

#endif  // CHROME_BROWSER_HISTORY_TAB_UI_H__
