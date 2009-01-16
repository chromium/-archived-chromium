// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The functionality provided here allows the user to import their bookmarks
// (favorites) from Google Toolbar.

#ifndef CHROME_BROWSER_IMPORTER_TOOLBAR_IMPORTER_H__
#define CHROME_BROWSER_IMPORTER_TOOLBAR_IMPORTER_H__

#include <string>
#include <vector>

#include "chrome/browser/importer/importer.h"
#include "chrome/browser/net/url_fetcher.h"

class XmlReader;

// Currently the only configuration information we need is to check whether or
// not the user currently has their GAIA cookie.  This is done by the function
// exposed through the ToolbarImportUtils namespace.
namespace ToolbarImporterUtils {
bool IsGoogleGAIACookieInstalled();
}  // namespace ToolbarImporterUtils

// Toolbar5Importer is a class which exposes the functionality needed to
// communicate with the Google Toolbar v5 front-end, negotiate the download of
// Toolbar bookmarks, parse them, and install them on the client.
// Toolbar5Importer should not have StartImport called more than once.  Futher
// if StartImport is called, then the class must not be destroyed until it
// has either completed or Toolbar5Importer->Cancel() has been called.
class Toolbar5Importer : public URLFetcher::Delegate, public Importer {
 public:
  Toolbar5Importer();
  virtual ~Toolbar5Importer();

  // Importer view calls this method to begin the process.  The items parameter
  // should only either be NONE or FAVORITES, since as of right now these are
  // the only items this importer supports.  This method provides implementation
  // of Importer::StartImport.
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
  virtual void OnURLFetchComplete(const URLFetcher* source,
                          const GURL& url,
                          const URLRequestStatus& status,
                          int response_code,
                          const ResponseCookies& cookies,
                          const std::string& data);

 private:
  FRIEND_TEST(Toolbar5ImporterTest, BookmarkParse);

  // Internal states of the toolbar importer.
  enum InternalStateEnum {
    NOT_USED = -1,
    INITIALIZED,
    GET_AUTHORIZATION_TOKEN,
    GET_BOOKMARKS,
    PARSE_BOOKMARKS,
    DONE
  };

  typedef std::vector<std::wstring> BookmarkFolderType;

  // URLs for connecting to the toolbar front end are defined below.
  static const char kT5AuthorizationTokenUrl[];
  static const char kT5FrontEndUrlTemplate[];

  // Token replacement tags are defined below.
  static const char kRandomNumberToken[];
  static const char kAuthorizationToken[];
  static const char kAuthorizationTokenPrefix[];
  static const char kAuthorizationTokenSuffix[];
  static const char kMaxNumToken[];
  static const char kMaxTimestampToken[];

  // XML tag names are defined below.
  static const char kXmlApiReplyXmlTag[];
  static const char kBookmarksXmlTag[];
  static const char kBookmarkXmlTag[];
  static const char kTitleXmlTag[];
  static const char kUrlXmlTag[];
  static const char kTimestampXmlTag[];
  static const char kLabelsXmlTag[];
  static const char kLabelsXmlCloseTag[];
  static const char kLabelXmlTag[];
  static const char kAttributesXmlTag[];

  // Flow control for asynchronous import is controlled by the methods below.
  // ContinueImport is called back by each import action taken.  BeginXXX
  // and EndXXX are responsible for updating the state of the asynchronous
  // import.  EndImport is responsible for state cleanup and notifying the
  // caller that import has completed.
  void ContinueImport();
  void EndImport();
  void BeginImportBookmarks();
  void EndImportBookmarks();

  // Network I/O is done by the methods below.  These three methods are called
  // in the order provided.  The last two are called back with the HTML
  // response provided by the Toolbar server.
  void GetAuthenticationFromServer();
  void GetBookmarkDataFromServer(const std::string& response);
  void GetBookmarksFromServerDataResponse(const std::string& response);

  // XML Parsing is implemented with the methods below.
  bool ParseAuthenticationTokenResponse(const std::string& response,
                                        std::string* token);

  static bool ParseBookmarksFromReader(
      XmlReader* reader,
      std::vector<ProfileWriter::BookmarkEntry>* bookmarks);

  static bool LocateNextOpenTag(XmlReader* reader);
  static bool LocateNextTagByName(XmlReader* reader, const std::string& tag);
  static bool LocateNextTagWithStopByName(
      XmlReader* reader,
      const std::string& tag,
      const std::string& stop);

  static bool ExtractBookmarkInformation(
      XmlReader* reader,
      ProfileWriter::BookmarkEntry* bookmark_entry,
      std::vector<BookmarkFolderType>* bookmark_folders);
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
      std::vector<BookmarkFolderType>* bookmark_folders);

  // Bookmark creation is done by the method below.
  void AddBookmarksToChrome(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks);

  // The writer used in this importer is stored in writer_.
  ProfileWriter* writer_;

  // Internal state is stored in state_.
  InternalStateEnum state_;

  // Bitmask of Importer::ImportItem is stored in items_to_import_.
  uint16  items_to_import_;

  // The fetchers need to be available to cancel the network call on user cancel
  // hence they are stored as member variables.
  URLFetcher* token_fetcher_;
  URLFetcher* data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(Toolbar5Importer);
};

#endif  // CHROME_BROWSER_IMPORTER_TOOLBAR_IMPORTER_H__
