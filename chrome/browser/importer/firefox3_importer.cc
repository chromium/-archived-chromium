// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox3_importer.h"

#include <set>

#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/importer/firefox2_importer.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/time_format.h"
#include "generated_resources.h"

using base::Time;

// Wraps the function sqlite3_close() in a class that is
// used in scoped_ptr_malloc.

namespace {

class DBClose {
 public:
  inline void operator()(sqlite3* x) const {
    sqlite3_close(x);
  }
};

}  // namespace

void Firefox3Importer::StartImport(ProfileInfo profile_info,
                                   uint16 items, ProfileWriter* writer,
                                   MessageLoop* delagate_loop,
                                   ImporterHost* host) {
  writer_ = writer;
  source_path_ = profile_info.source_path;
  app_path_ = profile_info.app_path;
  importer_host_ = host;


  // The order here is important!
  NotifyStarted();
  if ((items & HOME_PAGE) && !cancelled())
    ImportHomepage();  // Doesn't have a UI item.
  if ((items & FAVORITES) && !cancelled()) {
    NotifyItemStarted(FAVORITES);
    ImportBookmarks();
    NotifyItemEnded(FAVORITES);
  }
  if ((items & SEARCH_ENGINES) && !cancelled()) {
    NotifyItemStarted(SEARCH_ENGINES);
    ImportSearchEngines();
    NotifyItemEnded(SEARCH_ENGINES);
  }
  if ((items & PASSWORDS) && !cancelled()) {
    NotifyItemStarted(PASSWORDS);
    ImportPasswords();
    NotifyItemEnded(PASSWORDS);
  }
  if ((items & HISTORY) && !cancelled()) {
    NotifyItemStarted(HISTORY);
    ImportHistory();
    NotifyItemEnded(HISTORY);
  }
  NotifyEnded();
}

void Firefox3Importer::ImportHistory() {
  std::wstring file = source_path_;
  file_util::AppendToPath(&file, L"places.sqlite");
  if (!file_util::PathExists(file))
    return;

  sqlite3* sqlite;
  if (sqlite3_open(WideToUTF8(file).c_str(), &sqlite) != SQLITE_OK)
    return;
  scoped_ptr_malloc<sqlite3, DBClose> db(sqlite);

  SQLStatement s;
  // |visit_type| represent the transition type of URLs (typed, click,
  // redirect, bookmark, etc.) We eliminate some URLs like sub-frames and
  // redirects, since we don't want them to appear in history.
  // Firefox transition types are defined in:
  //   toolkit/components/places/public/nsINavHistoryService.idl
  const char* stmt = "SELECT h.url, h.title, h.visit_count, "
                     "h.hidden, h.typed, v.visit_date "
                     "FROM moz_places h JOIN moz_historyvisits v "
                     "ON h.id = v.place_id "
                     "WHERE v.visit_type <= 3";

  if (s.prepare(db.get(), stmt) != SQLITE_OK)
    return;

  std::vector<history::URLRow> rows;
  while (s.step() == SQLITE_ROW && !cancelled()) {
    GURL url(s.column_string(0));

    // Filter out unwanted URLs.
    if (!CanImportURL(url))
      continue;

    history::URLRow row(url);
    row.set_title(s.column_wstring(1));
    row.set_visit_count(s.column_int(2));
    row.set_hidden(s.column_int(3) == 1);
    row.set_typed_count(s.column_int(4));
    row.set_last_visit(Time::FromTimeT(s.column_int64(5)/1000000));

    rows.push_back(row);
  }
  if (!rows.empty() && !cancelled()) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddHistoryPage, rows));
  }
}

void Firefox3Importer::ImportBookmarks() {
  std::wstring file = source_path_;
  file_util::AppendToPath(&file, L"places.sqlite");
  if (!file_util::PathExists(file))
    return;

  sqlite3* sqlite;
  if (sqlite3_open(WideToUTF8(file).c_str(), &sqlite) != SQLITE_OK)
    return;
  scoped_ptr_malloc<sqlite3, DBClose> db(sqlite);

  // Get the bookmark folders that we are interested in.
  int toolbar_folder_id = -1;
  int menu_folder_id = -1;
  int unsorted_folder_id = -1;
  LoadRootNodeID(db.get(), &toolbar_folder_id, &menu_folder_id,
                 &unsorted_folder_id);

  // Load livemark IDs.
  std::set<int> livemark_id;
  LoadLivemarkIDs(db.get(), &livemark_id);

  // Load the default bookmarks. Its storage is the same as Firefox 2.
  std::set<GURL> default_urls;
  Firefox2Importer::LoadDefaultBookmarks(app_path_, &default_urls);

  BookmarkList list;
  GetTopBookmarkFolder(db.get(), toolbar_folder_id, &list);
  GetTopBookmarkFolder(db.get(), menu_folder_id, &list);
  GetTopBookmarkFolder(db.get(), unsorted_folder_id, &list);
  size_t count = list.size();
  for (size_t i = 0; i < count; ++i)
    GetWholeBookmarkFolder(db.get(), &list, i);

  std::vector<ProfileWriter::BookmarkEntry> bookmarks;
  std::vector<TemplateURL*> template_urls;
  FaviconMap favicon_map;

  // TODO(jcampan): http://b/issue?id=1196285 we do not support POST based
  //                keywords yet.  We won't include them in the list.
  std::set<int> post_keyword_ids;
  SQLStatement s;
  const char* stmt = "SELECT b.id FROM moz_bookmarks b "
      "INNER JOIN moz_items_annos ia ON ia.item_id = b.id "
      "INNER JOIN moz_anno_attributes aa ON ia.anno_attribute_id = aa.id "
      "WHERE aa.name = 'bookmarkProperties/POSTData'";
  if (s.prepare(db.get(), stmt) == SQLITE_OK) {
    while (s.step() == SQLITE_ROW && !cancelled())
      post_keyword_ids.insert(s.column_int(0));
  } else {
    NOTREACHED();
  }

  std::wstring firefox_folder =
      l10n_util::GetString(IDS_BOOKMARK_GROUP_FROM_FIREFOX);
  for (size_t i = 0; i < list.size(); ++i) {
    BookmarkItem* item = list[i];

    // The type of bookmark items is 1.
    if (item->type != 1)
      continue;

    // Skip the default bookmarks and unwanted URLs.
    if (!CanImportURL(item->url) ||
        default_urls.find(item->url) != default_urls.end() ||
        post_keyword_ids.find(item->id) != post_keyword_ids.end())
      continue;

    // Find the bookmark path by tracing their links to parent folders.
    std::vector<std::wstring> path;
    BookmarkItem* child = item;
    bool found_path = false;
    bool is_in_toolbar = false;
    while (child->parent >= 0) {
      BookmarkItem* parent = list[child->parent];
      if (parent->id == toolbar_folder_id) {
        // This bookmark entry should be put in the bookmark bar.
        // But we put it in the Firefox group after first run, so
        // that do not mess up the bookmark bar.
        if (first_run()) {
          is_in_toolbar = true;
        } else {
          path.insert(path.begin(), parent->title);
          path.insert(path.begin(), firefox_folder);
        }
        found_path = true;
        break;
      } else if (parent->id == menu_folder_id ||
                 parent->id == unsorted_folder_id) {
        // After the first run, the item will be placed in a folder in
        // the "Other bookmarks".
        if (!first_run())
          path.insert(path.begin(), firefox_folder);
        found_path = true;
        break;
      } else if (livemark_id.find(parent->id) != livemark_id.end()) {
        // If the entry is under a livemark folder, we don't import it.
        break;
      }
      path.insert(path.begin(), parent->title);
      child = parent;
    }

    if (!found_path)
      continue;

    ProfileWriter::BookmarkEntry entry;
    entry.creation_time = item->date_added;
    entry.title = item->title;
    entry.url = item->url;
    entry.path = path;
    entry.in_toolbar = is_in_toolbar;

    bookmarks.push_back(entry);

    if (item->favicon)
      favicon_map[item->favicon].insert(item->url);

    // This bookmark has a keyword, we import it to our TemplateURL model.
    TemplateURL* t_url = Firefox2Importer::CreateTemplateURL(
        item->title, UTF8ToWide(item->keyword), item->url);
    if (t_url)
      template_urls.push_back(t_url);
  }

  STLDeleteContainerPointers(list.begin(), list.end());

  // Write into profile.
  if (!bookmarks.empty() && !cancelled()) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddBookmarkEntry, bookmarks,
        l10n_util::GetString(IDS_BOOKMARK_GROUP_FROM_FIREFOX),
        first_run() ? ProfileWriter::FIRST_RUN : 0));
  }
  if (!template_urls.empty() && !cancelled()) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddKeywords, template_urls, -1, false));
  } else {
    STLDeleteContainerPointers(template_urls.begin(), template_urls.end());
  }
  if (!favicon_map.empty() && !cancelled()) {
    std::vector<history::ImportedFavIconUsage> favicons;
    LoadFavicons(db.get(), favicon_map, &favicons);
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddFavicons, favicons));
  }
}

void Firefox3Importer::ImportPasswords() {
  // Initializes NSS3.
  NSSDecryptor decryptor;
  if (!decryptor.Init(source_path_, source_path_) &&
      !decryptor.Init(app_path_, source_path_))
    return;

  // Firefox 3 uses signons3.txt to store the passwords.
  std::wstring file = source_path_;
  file_util::AppendToPath(&file, L"signons3.txt");
  if (!file_util::PathExists(file)) {
    file = source_path_;
    file_util::AppendToPath(&file, L"signons2.txt");
  }

  std::string content;
  file_util::ReadFileToString(file, &content);
  std::vector<PasswordForm> forms;
  decryptor.ParseSignons(content, &forms);

  if (!cancelled()) {
    for (size_t i = 0; i < forms.size(); ++i) {
      main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
          &ProfileWriter::AddPasswordForm, forms[i]));
    }
  }
}

void Firefox3Importer::ImportSearchEngines() {
  std::vector<std::wstring> files;
  GetSearchEnginesXMLFiles(&files);

  std::vector<TemplateURL*> search_engines;
  ParseSearchEnginesFromXMLFiles(files, &search_engines);
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
      &ProfileWriter::AddKeywords, search_engines,
      GetFirefoxDefaultSearchEngineIndex(search_engines, source_path_), true));
}

void Firefox3Importer::ImportHomepage() {
  GURL homepage = GetHomepage(source_path_);
  if (homepage.is_valid() && !IsDefaultHomepage(homepage, app_path_)) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddHomepage, homepage));
  }
}

void Firefox3Importer::GetSearchEnginesXMLFiles(
    std::vector<std::wstring>* files) {
  std::wstring file = source_path_;
  file_util::AppendToPath(&file, L"search.sqlite");
  if (!file_util::PathExists(file))
    return;

  sqlite3* sqlite;
  if (sqlite3_open(WideToUTF8(file).c_str(), &sqlite) != SQLITE_OK)
    return;
  scoped_ptr_malloc<sqlite3, DBClose> db(sqlite);

  SQLStatement s;
  const char* stmt = "SELECT engineid FROM engine_data ORDER BY value ASC";

  if (s.prepare(db.get(), stmt) != SQLITE_OK)
    return;

  std::wstring app_path = app_path_;
  file_util::AppendToPath(&app_path, L"searchplugins");
  std::wstring profile_path = source_path_;
  file_util::AppendToPath(&profile_path, L"searchplugins");

  const std::wstring kAppPrefix = L"[app]/";
  const std::wstring kProfilePrefix = L"[profile]/";
  while (s.step() == SQLITE_ROW && !cancelled()) {
    std::wstring file;
    std::wstring engine = UTF8ToWide(s.column_string(0));
    // The string contains [app]/<name>.xml or [profile]/<name>.xml where the
    // [app] and [profile] need to be replaced with the actual app or profile
    // path.
    size_t index = engine.find(kAppPrefix);
    if (index != std::wstring::npos) {
      file = app_path;
      file_util::AppendToPath(
          &file,
          engine.substr(index + kAppPrefix.length()));  // Remove '[app]/'.
    } else if ((index = engine.find(kProfilePrefix)) != std::wstring::npos) {
      file = profile_path;
      file_util::AppendToPath(
          &file,
          engine.substr(index + kProfilePrefix.length()));  // Remove
                                                            // '[profile]/'.
    } else {
      NOTREACHED() << "Unexpected Firefox 3 search engine id";
      continue;
    }
    files->push_back(file);
  }
}

void Firefox3Importer::LoadRootNodeID(sqlite3* db,
                                      int* toolbar_folder_id,
                                      int* menu_folder_id,
                                      int* unsorted_folder_id) {
  const char kToolbarFolderName[] = "toolbar";
  const char kMenuFolderName[] = "menu";
  const char kUnsortedFolderName[] = "unfiled";

  SQLStatement s;
  const char* stmt = "SELECT root_name, folder_id FROM moz_bookmarks_roots";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  while (s.step() == SQLITE_ROW) {
    std::string folder = s.column_string(0);
    int id = s.column_int(1);
    if (folder == kToolbarFolderName)
      *toolbar_folder_id = id;
    else if (folder == kMenuFolderName)
      *menu_folder_id = id;
    else if (folder == kUnsortedFolderName)
      *unsorted_folder_id = id;
  }
}

void Firefox3Importer::LoadLivemarkIDs(sqlite3* db,
                                       std::set<int>* livemark) {
  const char kFeedAnnotation[] = "livemark/feedURI";
  livemark->clear();

  SQLStatement s;
  const char* stmt = "SELECT b.item_id "
                     "FROM moz_anno_attributes a "
                     "JOIN moz_items_annos b ON a.id = b.anno_attribute_id "
                     "WHERE a.name = ? ";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  s.bind_string(0, kFeedAnnotation);
  while (s.step() == SQLITE_ROW && !cancelled())
    livemark->insert(s.column_int(0));
}

void Firefox3Importer::GetTopBookmarkFolder(sqlite3* db, int folder_id,
                                            BookmarkList* list) {
  SQLStatement s;
  const char* stmt = "SELECT b.title "
         "FROM moz_bookmarks b "
         "WHERE b.type = 2 AND b.id = ? "
         "ORDER BY b.position";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  s.bind_int(0, folder_id);
  if (s.step() == SQLITE_ROW) {
    BookmarkItem* item = new BookmarkItem;
    item->parent = -1;  // The top level folder has no parent.
    item->id = folder_id;
    item->title = s.column_wstring(0);
    item->type = 2;
    item->favicon = 0;
    list->push_back(item);
  }
}

void Firefox3Importer::GetWholeBookmarkFolder(sqlite3* db, BookmarkList* list,
                                              size_t position) {
  if (position >= list->size()) {
    NOTREACHED();
    return;
  }

  SQLStatement s;
  const char* stmt = "SELECT b.id, h.url, COALESCE(b.title, h.title), "
         "b.type, k.keyword, b.dateAdded, h.favicon_id "
         "FROM moz_bookmarks b "
         "LEFT JOIN moz_places h ON b.fk = h.id "
         "LEFT JOIN moz_keywords k ON k.id = b.keyword_id "
         "WHERE b.type IN (1,2) AND b.parent = ? "
         "ORDER BY b.position";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  s.bind_int(0, (*list)[position]->id);
  BookmarkList temp_list;
  while (s.step() == SQLITE_ROW) {
    BookmarkItem* item = new BookmarkItem;
    item->parent = static_cast<int>(position);
    item->id = s.column_int(0);
    item->url = GURL(s.column_string(1));
    item->title = s.column_wstring(2);
    item->type = s.column_int(3);
    item->keyword = s.column_string(4);
    item->date_added = Time::FromTimeT(s.column_int64(5)/1000000);
    item->favicon = s.column_int64(6);

    temp_list.push_back(item);
  }

  // Appends all items to the list.
  for (BookmarkList::iterator i = temp_list.begin();
       i != temp_list.end(); ++i) {
    list->push_back(*i);
    // Recursive add bookmarks in sub-folders.
    if ((*i)->type == 2)
      GetWholeBookmarkFolder(db, list, list->size() - 1);
  }
}

void Firefox3Importer::LoadFavicons(
    sqlite3* db,
    const FaviconMap& favicon_map,
    std::vector<history::ImportedFavIconUsage>* favicons) {
  SQLStatement s;
  const char* stmt = "SELECT url, data FROM moz_favicons WHERE id=?";
  if (s.prepare(db, stmt) != SQLITE_OK)
    return;

  for (FaviconMap::const_iterator i = favicon_map.begin();
       i != favicon_map.end(); ++i) {
    s.bind_int64(0, i->first);
    if (s.step() == SQLITE_ROW) {
      history::ImportedFavIconUsage usage;

      usage.favicon_url = GURL(s.column_string(0));
      if (!usage.favicon_url.is_valid())
        continue;  // Don't bother importing favicons with invalid URLs.

      std::vector<unsigned char> data;
      if (!s.column_blob_as_vector(1, &data) || data.empty())
        continue;  // Data definitely invalid.

      if (!ReencodeFavicon(&data[0], data.size(), &usage.png_data))
        continue;  // Unable to decode.

      usage.urls = i->second;
      favicons->push_back(usage);
    }
    s.reset();
  }
}
