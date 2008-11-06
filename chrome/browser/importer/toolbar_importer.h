// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The functionality provided here allows the user to import their bookmarks 
// (favorites) from Google Toolbar.  
//
// Currently the only configuration information we need is to check whether or 
// not the user currently has their GAIA cookie.  This is done by the functions
// exposed through the ToolbarImportUtils namespace.
//
// Toolbar5Importer is a class which exposes the functionality needed to
// communicate with the Google Toolbar v5 front-end, negotiate the download of
// Toolbar bookmarks, parse them, and install them on the client.

#ifndef CHROME_BROWSER_IMPORTER_TOOLBAR_IMPROTER_H__
#define CHROME_BROWSER_IMPORTER_TOOLBAR_IMPROTER_H__

#include <string>
#include <vector>

#include "chrome/browser/importer/importer.h"
#include "chrome/browser/url_fetcher.h"

class XmlReader;

namespace ToolbarImporterUtils {

bool IsGoogleGAIACookieInstalled();

};

class Toolbar5Importer : public URLFetcher::Delegate,
                         public Importer {
 public:
  Toolbar5Importer();
  virtual ~Toolbar5Importer();

  // Importer view calls this method to being the process.
  virtual void StartImport(ProfileInfo profile_info,
                           uint16 items,
                           ProfileWriter* writer,
                           MessageLoop* delegate_loop,
                           ImporterHost* host);

  // Importer view call this method when the user clicks the cancel button
  // in the ImporterView UI.  We need to post a message to our loop
  // to cancel network retrieval.
  virtual void Cancel();

  // URLFetcher::Delegate method called back from the URLFetcher object.
  void OnURLFetchComplete(const URLFetcher* source,
                          const GURL& url,
                          const URLRequestStatus& status,
                          int response_code,
                          const ResponseCookies& cookies,
                          const std::string& data);

 private:
  FRIEND_TEST(Toolbar5ImporterTest, BookmarkParse);
 
  // Internal state
  enum INTERNAL_STATE {
    NOT_USED = -1,
    INITIALIZED,
    GET_AUTHORIZATION_TOKEN,
    GET_BOOKMARKS,
    PARSE_BOOKMARKS,
    DONE
  };

  typedef std::vector<std::wstring> BOOKMARK_FOLDER;

  // URLs for connecting to the toolbar front end
  static const std::string kT5AuthorizationTokenUrl;
  static const std::string kT5FrontEndUrlTemplate;

  // Token replacement tags
  static const std::string kRandomNumberToken;
  static const std::string kAuthorizationToken;
  static const std::string kAuthorizationTokenPrefix;
  static const std::string kAuthorizationTokenSuffix;
  static const std::string kMaxNumToken;
  static const std::string kMaxTimestampToken;

  // XML tag names
  static const std::string kXmlApiReplyXmlTag;
  static const std::string kBookmarksXmlTag;
  static const std::string kBookmarkXmlTag;
  static const std::string kTitleXmlTag;
  static const std::string kUrlXmlTag;
  static const std::string kTimestampXmlTag;
  static const std::string kLabelsXmlTag;
  static const std::string kLabelsXmlCloseTag;
  static const std::string kLabelXmlTag;
  static const std::string kAttributesXmlTag;

  // Flow control
  void ContinueImport();
  void EndImport();
  void BeginImportBookmarks();
  void EndImportBookmarks(bool success);

  // Network I/O
  void GetAuthenticationFromServer();
  void GetBookmarkDataFromServer(const std::string& response);
  void GetBookmarsFromServerDataResponse(const std::string& response);

  // XML Parsing
  bool ParseAuthenticationTokenResponse(const std::string& response,
                                        std::string* token);

  static bool ParseBookmarksFromReader(
      XmlReader* reader,
      std::vector< ProfileWriter::BookmarkEntry >* bookmarks);

  static bool LocateNextOpenTag(XmlReader* reader);
  static bool LocateNextTagByName(XmlReader* reader, const std::string& tag);
  static bool LocateNextTagWithStopByName(
      XmlReader* reader, 
      const std::string& tag, 
      const std::string& stop);

  static bool ExtractBookmarkInformation(
      XmlReader* reader,
      ProfileWriter::BookmarkEntry* bookmark_entry,
      std::vector<BOOKMARK_FOLDER>* bookmark_folders);
  static bool ExtractNamedValueFromXmlReader(XmlReader* reader,
                                             const std::string& name,
                                             std::string* buffer);
  static bool ExtractTitleFromXmlReader(XmlReader* reader,
                                        ProfileWriter::BookmarkEntry* entry);
  static bool ExtractUrlFromXmlReader(XmlReader* reader,
                                      ProfileWriter::BookmarkEntry* entry);
  static bool ExtractTimeFromXmlReader(XmlReader* reader,
                                       ProfileWriter::BookmarkEntry* entry);
  static bool ExtractFoldersFromXmlReader(
      XmlReader* reader,
      std::vector<BOOKMARK_FOLDER>* bookmark_folders);

  // Bookmark creation
  void AddBookMarksToChrome(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks);

  // Hosts the writer used in this importer.
  ProfileWriter* writer_;

  // Internal state
  INTERNAL_STATE state_;

  // Bitmask of Importer::ImportItem
  uint16  items_to_import_;

  // The fetchers need to be available to cancel the network call on user cancel
  URLFetcher * token_fetcher_;
  URLFetcher * data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(Toolbar5Importer);
};

#endif  // CHROME_BROWSER_IMPORTER_TOOLBAR_IMPROTER_H__
