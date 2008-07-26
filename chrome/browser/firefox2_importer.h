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

#ifndef CHROME_BROWSER_FIREFOX2_IMPORTER_H_
#define CHROME_BROWSER_FIREFOX2_IMPORTER_H_

#include "chrome/browser/importer.h"

class TemplateURL;

// Importer for Mozilla Firefox 2.
class Firefox2Importer : public Importer {
 public:
  Firefox2Importer();
  virtual ~Firefox2Importer();

  // Importer methods.
  virtual void StartImport(ProfileInfo profile_info,
                           uint16 items,
                           ProfileWriter* writer,
                           ImporterHost* host);

  // Loads the default bookmarks in the Firefox installed at |firefox_app_path|,
  // and stores their locations in |urls|.
  static void LoadDefaultBookmarks(const std::wstring& firefox_app_path,
                                   std::set<GURL> *urls);

  // Creates a TemplateURL with the |keyword| and |url|. |title| may be empty.
  // This function transfers ownership of the created TemplateURL to the caller.
  static TemplateURL* CreateTemplateURL(const std::wstring& title,
                                        const std::wstring& keyword,
                                        const GURL& url);

 private:
  FRIEND_TEST(FirefoxImporterTest, Firefox2BookmarkParse);
  FRIEND_TEST(FirefoxImporterTest, Firefox2CookesParse);

  void ImportBookmarks();
  void ImportPasswords();
  void ImportHistory();
  void ImportSearchEngines();
  // Import the user's home page, unless it is set to default home page as
  // defined in browserconfig.properties.
  void ImportHomepage();

  // Fills |files| with the paths to the files containing the search engine
  // descriptions.
  void GetSearchEnginesXMLFiles(std::vector<std::wstring>* files);

  // Helper methods for parsing bookmark file.
  // Firefox 2 saves its bookmarks in a html file. We are interested in the
  // bookmarks and folders, and their hierarchy. A folder starts with a
  // heading tag, which contains it title. All bookmarks and sub-folders is
  // following, and bracketed by a <DL> tag:
  //   <DT><H3 PERSONAL_TOOLBAR_FOLDER="true" ...>title</H3>
  //   <DL><p>
  //      ... container ...
  //   </DL><p>
  // And a bookmark is presented by a <A> tag:
  //   <DT><A HREF="url" SHORTCUTURL="shortcut" ADD_DATE="11213014"...>name</A>
  // Reference: http://kb.mozillazine.org/Bookmarks.html
  static bool ParseCharsetFromLine(const std::string& line,
                                   std::string* charset);
  static bool ParseFolderNameFromLine(const std::string& line,
                                      const std::string& charset,
                                      std::wstring* folder_name,
                                      bool* is_toolbar_folder);
  // See above, this will also put the data: URL of the favicon into *favicon
  // if there is a favicon given.  |post_data| is set for POST base keywords to
  // the contents of the actual POST (with %s for the search term).
  static bool ParseBookmarkFromLine(const std::string& line,
                                    const std::string& charset,
                                    std::wstring* title,
                                    GURL* url,
                                    GURL* favicon,
                                    std::wstring* shortcut,
                                    Time* add_date,
                                    std::wstring* post_data);

  // Fetches the given attribute value from the |tag|. Returns true if
  // successful, and |value| will contain the value.
  static bool GetAttribute(const std::string& tag,
                           const std::string& attribute,
                           std::string* value);

  // There are some characters in html file will be escaped:
  //   '<', '>', '"', '\', '&'
  // Un-escapes them if the bookmark name has those characters.
  static void HTMLUnescape(std::wstring* text);

  // Fills |xml_files| with the file with an xml extension found under |dir|.
  static void FindXMLFilesInDir(const std::wstring& dir,
                                std::vector<std::wstring>* xml_files);

  // Given the URL of a page and a favicon data URL, adds an appropriate record
  // to the given favicon usage vector. Will do nothing if the favicon is not
  // valid.
  static void DataURLToFaviconUsage(
      const GURL& link_url,
      const GURL& favicon_data,
      std::vector<history::ImportedFavIconUsage>* favicons);

  ProfileWriter* writer_;
  std::wstring source_path_;
  std::wstring app_path_;

  DISALLOW_EVIL_CONSTRUCTORS(Firefox2Importer);
};

#endif  // CHROME_BROWSER_FIREFOX2_IMPORTER_H_
