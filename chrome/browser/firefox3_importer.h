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

#ifndef CHROME_BROWSER_FIREFOX3_IMPORTER_H_
#define CHROME_BROWSER_FIREFOX3_IMPORTER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/importer.h"
#include "chrome/common/sqlite_utils.h"
#include "googleurl/src/gurl.h"

// Importer for Mozilla Firefox 3.
// Firefox 3 stores its persistent information in a new system called places.
// http://wiki.mozilla.org/Places
class Firefox3Importer : public Importer {
 public:
  Firefox3Importer() { }
  virtual ~Firefox3Importer() { }

  // Importer methods.
  virtual void StartImport(ProfileInfo profile_info,
                           uint16 items,
                           ProfileWriter* writer,
                           ImporterHost* host);

 private:
  typedef std::map<int64, std::set<GURL> > FaviconMap;

  void ImportBookmarks();
  void ImportPasswords();
  void ImportHistory();
  void ImportSearchEngines();
  // Import the user's home page, unless it is set to default home page as
  // defined in browserconfig.properties.
  void ImportHomepage();
  void GetSearchEnginesXMLFiles(std::vector<std::wstring>* files);

  // The struct stores the information about a bookmark item.
  struct BookmarkItem {
    int parent;
    int id;
    GURL url;
    std::wstring title;
    int type;
    std::string keyword;
    Time date_added;
    int64 favicon;
  };
  typedef std::vector<BookmarkItem*> BookmarkList;

  // Gets the specific IDs of bookmark root node from |db|.
  void LoadRootNodeID(sqlite3* db, int* toolbar_folder_id,
                      int* menu_folder_id, int* unsorted_folder_id);

  // Loads all livemark IDs from database |db|.
  void LoadLivemarkIDs(sqlite3* db, std::set<int>* livemark);

  // Gets the bookmark folder with given ID, and adds the entry in |list|
  // if successful.
  void GetTopBookmarkFolder(sqlite3* db, int folder_id, BookmarkList* list);

  // Loads all children of the given folder, and appends them to the |list|.
  void GetWholeBookmarkFolder(sqlite3* db, BookmarkList* list,
                              size_t position);

  // Loads the favicons given in the map from the database, loads the data,
  // and converts it into FaviconUsage structures.
  void LoadFavicons(sqlite3* db,
                    const FaviconMap& favicon_map,
                    std::vector<history::ImportedFavIconUsage>* favicons);

  ProfileWriter* writer_;
  std::wstring source_path_;
  std::wstring app_path_;

  DISALLOW_EVIL_CONSTRUCTORS(Firefox3Importer);
};

#endif  // CHROME_BROWSER_FIREFOX3_IMPORTER_H_
