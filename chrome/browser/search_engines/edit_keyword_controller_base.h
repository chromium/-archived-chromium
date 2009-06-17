// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_EDIT_KEYWORD_CONTROLLER_BASE_H_
#define CHROME_BROWSER_SEARCH_ENGINES_EDIT_KEYWORD_CONTROLLER_BASE_H_

#include <string>

class Profile;
class TemplateURL;

// EditKeywordControllerBase provides the platform independent logic and
// interface for implementing a dialog for editing keyword searches.
class EditKeywordControllerBase {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Invoked from the EditKeywordController when the user accepts the edits.
    // NOTE: |template_url| is the value supplied to EditKeywordController's
    // constructor, and may be null. A null value indicates a new TemplateURL
    // should be created rather than modifying an existing TemplateURL.
    virtual void OnEditedKeyword(const TemplateURL* template_url,
                                 const std::wstring& title,
                                 const std::wstring& keyword,
                                 const std::wstring& url) = 0;
  };

  // The |template_url| and/or |edit_keyword_delegate| may be NULL.
  EditKeywordControllerBase(const TemplateURL* template_url,
                            Delegate* edit_keyword_delegate,
                            Profile* profile);
  virtual ~EditKeywordControllerBase() {}

 protected:
  // Interface to platform specific view
  virtual std::wstring GetURLInput() const = 0;
  virtual std::wstring GetKeywordInput() const = 0;
  virtual std::wstring GetTitleInput() const = 0;

  // Check if content of Title entry is valid.
  bool IsTitleValid() const;

  // Returns true if the currently input URL is valid. The URL is valid if it
  // contains no search terms and is a valid url, or if it contains a search
  // term and replacing that search term with a character results in a valid
  // url.
  bool IsURLValid() const;

  // Fixes up and returns the URL the user has input. The returned URL is
  // suitable for use by TemplateURL.
  std::wstring GetURL() const;

  // Returns whether the currently entered keyword is valid. The keyword is
  // valid if it is non-empty and does not conflict with an existing entry.
  // NOTE: this is just the keyword, not the title and url.
  bool IsKeywordValid() const;

  // Deletes an unused TemplateURL, if its add was cancelled and it's not
  // already owned by the TemplateURLModel.
  void AcceptAddOrEdit();

  // Deletes an unused TemplateURL, if its add was cancelled and it's not
  // already owned by the TemplateURLModel.
  void CleanUpCancelledAdd();

  const TemplateURL* template_url() const {
    return template_url_;
  }

  const Profile* profile() const {
    return profile_;
  }

 private:
  // The TemplateURL we're displaying information for. It may be NULL. If we
  // have a keyword_editor_view, we assume that this TemplateURL is already in
  // the TemplateURLModel; if not, we assume it isn't.
  const TemplateURL* template_url_;

  // We may have been created by this, in which case we will call back to it on
  // success to add/modify the entry.  May be NULL.
  Delegate* edit_keyword_delegate_;

  // Profile whose TemplateURLModel we're modifying.
  Profile* profile_;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_EDIT_KEYWORD_CONTROLLER_BASE_H_
