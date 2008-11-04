// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_TOOLBAR_IMPROTER_H__
#define CHROME_BROWSER_IMPORTER_TOOLBAR_IMPROTER_H__

#include <string>
#include <vector>

#include "chrome/browser/importer/importer.h"
#include "chrome/browser/url_fetcher.h"

class XmlReader;

enum TOOLBAR_VERSION {
  NO_VERSION = -1,
  DEPRECATED,
  VERSION_4,
  VERSION_5
};

class ToolbarImporterUtils {
 public:

  static bool IsToolbarInstalled();
  static bool IsGoogleGAIACookieInstalled();
  static TOOLBAR_VERSION GetToolbarVersion();

 private:
  static const HKEY kToolbarInstallRegistryRoots[2];
  static const TCHAR* kToolbarRootRegistryFolder;
  static const TCHAR* kToolbarVersionRegistryFolder;
  static const TCHAR* kToolbarVersionRegistryKey;

  ToolbarImporterUtils() {}
  ~ToolbarImporterUtils() {}

  DISALLOW_COPY_AND_ASSIGN(ToolbarImporterUtils);
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
                           ImporterHost* host);

  // URLFetcher::Delegate method
  void OnURLFetchComplete(const URLFetcher* source,
                          const GURL& url,
                          const URLRequestStatus& status,
                          int response_code,
                          const ResponseCookies& cookies,
                          const std::string& data);

 private:
  // Internal state
  enum INTERNAL_STATE {
    NOT_USED = -1,
    INITIALIZED,
    GET_AUTHORIZATION_TOKEN,
    GET_BOOKMARKS,
    DONE
  };

  // URLs for connecting to the toolbar front end
  static const std::string kT5AuthorizationTokenUrl;
  static const std::string kT5FrontEndUrlTemplate;
  static const std::string kT4FrontEndUrlTemplate;

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
  static const std::string kLabelXmlTag;
  static const std::string kAttributesXmlTag;
  static const std::string kAttributeXmlTag;
  static const std::string kNameXmlTag;
  static const std::string kValueXmlTag;
  static const std::string kFaviconAttributeXmlName;

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
  void ConstructFEConnectionString(const std::string& token,
                                   std::string* conn_string);

  bool ParseBookmarksFromReader(
      XmlReader* reader,
      std::vector< ProfileWriter::BookmarkEntry >* bookmarks,
      std::vector< history::ImportedFavIconUsage >* favicons);

  bool LocateNextTagByName(XmlReader* reader, const std::string& tag);

  bool ExtractBookmarkInformation(XmlReader* reader,
                                  ProfileWriter::BookmarkEntry* bookmark_entry,
                                  history::ImportedFavIconUsage* favicon_entry);
  bool ExtractNamedValueFromXmlReader(XmlReader* reader,
                                      const std::string& name,
                                      std::string* buffer);
  bool ExtractTitleFromXmlReader(XmlReader* reader,
                                 ProfileWriter::BookmarkEntry* entry);
  bool ExtractUrlFromXmlReader(XmlReader* reader,
                               ProfileWriter::BookmarkEntry* entry);
  bool ExtractTimeFromXmlReader(XmlReader* reader,
                                ProfileWriter::BookmarkEntry* entry);
  bool ExtractFolderFromXmlReader(XmlReader* reader,
                                  ProfileWriter::BookmarkEntry* entry);
  bool ExtractFaviconFromXmlReader(
      XmlReader* reader,
      ProfileWriter::BookmarkEntry* bookmark_entry,
      history::ImportedFavIconUsage* favicon_entry);

  // Bookmark creation
  void AddBookMarksToChrome(
    const std::vector< ProfileWriter::BookmarkEntry >& bookmarks,
    const std::vector< history::ImportedFavIconUsage >& favicons);

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
