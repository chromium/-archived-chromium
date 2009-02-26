// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox2_importer.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/mork_reader.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_parser.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/data_url.h"

using base::Time;

// Firefox2Importer.

Firefox2Importer::Firefox2Importer() : parsing_bookmarks_html_file_(false) {
}

Firefox2Importer::~Firefox2Importer() {
}

void Firefox2Importer::StartImport(ProfileInfo profile_info,
                                   uint16 items, ProfileWriter* writer,
                                   MessageLoop* delagate_loop,
                                   ImporterHost* host) {
  writer_ = writer;
  source_path_ = profile_info.source_path;
  app_path_ = profile_info.app_path;
  importer_host_ = host;

  parsing_bookmarks_html_file_ = (profile_info.browser_type == BOOKMARKS_HTML);

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

// static
void Firefox2Importer::LoadDefaultBookmarks(const std::wstring& app_path,
                                            std::set<GURL> *urls) {
  // TODO(port): Code below is correct only on Windows.
  // Firefox keeps its default bookmarks in a bookmarks.html file that
  // lives at: <Firefox install dir>\defaults\profile\bookmarks.html
  FilePath file = FilePath::FromWStringHack(app_path);
  file.AppendASCII("defaults");
  file.AppendASCII("profile");
  file.AppendASCII("bookmarks.html");

  urls->clear();

  // Read the whole file.
  std::string content;
  file_util::ReadFileToString(file.ToWStringHack(), &content);
  std::vector<std::string> lines;
  SplitString(content, '\n', &lines);

  std::string charset;
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line;
    TrimString(lines[i], " ", &line);

    // Get the encoding of the bookmark file.
    if (ParseCharsetFromLine(line, &charset))
      continue;

    // Get the bookmark.
    std::wstring title;
    GURL url, favicon;
    std::wstring shortcut;
    Time add_date;
    std::wstring post_data;
    if (ParseBookmarkFromLine(line, charset, &title, &url,
                              &favicon, &shortcut, &add_date,
                              &post_data))
      urls->insert(url);
  }
}

// static
TemplateURL* Firefox2Importer::CreateTemplateURL(const std::wstring& title,
                                                 const std::wstring& keyword,
                                                 const GURL& url) {
  // Skip if the keyword or url is invalid.
  if (keyword.empty() && url.is_valid())
    return NULL;

  TemplateURL* t_url = new TemplateURL();
  // We set short name by using the title if it exists.
  // Otherwise, we use the shortcut.
  t_url->set_short_name(!title.empty() ? title : keyword);
  t_url->set_keyword(keyword);
  t_url->SetURL(TemplateURLRef::DisplayURLToURLRef(UTF8ToWide(url.spec())),
                0, 0);
  return t_url;
}

// static
void Firefox2Importer::ImportBookmarksFile(
    const std::wstring& file_path,
    const std::set<GURL>& default_urls,
    bool first_run,
    const std::wstring& first_folder_name,
    Importer* importer,
    std::vector<ProfileWriter::BookmarkEntry>* bookmarks,
    std::vector<TemplateURL*>* template_urls,
    std::vector<history::ImportedFavIconUsage>* favicons) {
  std::string content;
  file_util::ReadFileToString(file_path, &content);
  std::vector<std::string> lines;
  SplitString(content, '\n', &lines);
  
  std::vector<ProfileWriter::BookmarkEntry> toolbar_bookmarks;
  std::wstring last_folder = first_folder_name;
  bool last_folder_on_toolbar = false;
  std::vector<std::wstring> path;
  size_t toolbar_folder = 0;
  std::string charset;
  for (size_t i = 0; i < lines.size() && (!importer || !importer->cancelled());
       ++i) {
    std::string line;
    TrimString(lines[i], " ", &line);

    // Get the encoding of the bookmark file.
    if (ParseCharsetFromLine(line, &charset))
      continue;

    // Get the folder name.
    if (ParseFolderNameFromLine(line, charset, &last_folder,
                                &last_folder_on_toolbar))
      continue;

    // Get the bookmark entry.
    std::wstring title, shortcut;
    GURL url, favicon;
    Time add_date;
    std::wstring post_data;
    // TODO(jcampan): http://b/issue?id=1196285 we do not support POST based
    //                keywords yet.
    if (ParseBookmarkFromLine(line, charset, &title,
                              &url, &favicon, &shortcut, &add_date,
                              &post_data) &&
        post_data.empty() &&
        CanImportURL(GURL(url)) &&
        default_urls.find(url) == default_urls.end()) {
      if (toolbar_folder > path.size() && path.size() > 0) {
        NOTREACHED();  // error in parsing.
        break;
      }

      ProfileWriter::BookmarkEntry entry;
      entry.creation_time = add_date;
      entry.url = url;
      entry.title = title;

      if (first_run && toolbar_folder) {
        // Flatten the items in toolbar.
        entry.in_toolbar = true;
        entry.path.assign(path.begin() + toolbar_folder, path.end());
        toolbar_bookmarks.push_back(entry);
      } else {
        // Insert the item into the "Imported from Firefox" folder after
        // the first run.
        entry.path.assign(path.begin(), path.end());
        if (first_run)
          entry.path.erase(entry.path.begin());
        bookmarks->push_back(entry);
      }

      // Save the favicon. DataURLToFaviconUsage will handle the case where
      // there is no favicon.
      if (favicons)
        DataURLToFaviconUsage(url, favicon, favicons);

      if (template_urls) {
        // If there is a SHORTCUT attribute for this bookmark, we
        // add it as our keywords.
        TemplateURL* t_url = CreateTemplateURL(title, shortcut, url);
        if (t_url)
          template_urls->push_back(t_url);
      }

      continue;
    }

    // Bookmarks in sub-folder are encapsulated with <DL> tag.
    if (StartsWithASCII(line, "<DL>", true)) {
      path.push_back(last_folder);
      last_folder.clear();
      if (last_folder_on_toolbar && !toolbar_folder)
        toolbar_folder = path.size();
    } else if (StartsWithASCII(line, "</DL>", true)) {
      if (path.empty())
        break;  // Mismatch <DL>.
      path.pop_back();
      if (toolbar_folder > path.size())
        toolbar_folder = 0;
    }
  }

  bookmarks->insert(bookmarks->begin(), toolbar_bookmarks.begin(),
                    toolbar_bookmarks.end());
}

void Firefox2Importer::ImportBookmarks() {
  // Load the default bookmarks.
  std::set<GURL> default_urls;
  if (!parsing_bookmarks_html_file_)
    LoadDefaultBookmarks(app_path_, &default_urls);

  // Parse the bookmarks.html file.
  std::vector<ProfileWriter::BookmarkEntry> bookmarks, toolbar_bookmarks;
  std::vector<TemplateURL*> template_urls;
  std::vector<history::ImportedFavIconUsage> favicons;
  std::wstring file = source_path_;
  if (!parsing_bookmarks_html_file_)
    file_util::AppendToPath(&file, L"bookmarks.html");
  std::wstring first_folder_name;
  if (parsing_bookmarks_html_file_)
    first_folder_name = l10n_util::GetString(IDS_BOOKMARK_GROUP);
  else
    first_folder_name = l10n_util::GetString(IDS_BOOKMARK_GROUP_FROM_FIREFOX);

  ImportBookmarksFile(file, default_urls, first_run(),
                      first_folder_name, this, &bookmarks, &template_urls,
                      &favicons);

  // Write data into profile.
  if (!bookmarks.empty() && !cancelled()) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddBookmarkEntry, bookmarks,
        first_folder_name,
        first_run() ? ProfileWriter::FIRST_RUN : 0));
  }
  if (!parsing_bookmarks_html_file_ && !template_urls.empty() &&
      !cancelled()) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddKeywords, template_urls, -1, false));
  } else {
    STLDeleteContainerPointers(template_urls.begin(), template_urls.end());
  }
  if (!favicons.empty()) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddFavicons, favicons));
  }
}

void Firefox2Importer::ImportPasswords() {
  // Initializes NSS3.
  NSSDecryptor decryptor;
  if (!decryptor.Init(source_path_, source_path_) &&
      !decryptor.Init(app_path_, source_path_))
    return;

  // Firefox 2 uses signons2.txt to store the pssswords. If it doesn't
  // exist, we try to find its older version.
  std::wstring file = source_path_;
  file_util::AppendToPath(&file, L"signons2.txt");
  if (!file_util::PathExists(file)) {
    file = source_path_;
    file_util::AppendToPath(&file, L"signons.txt");
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

void Firefox2Importer::ImportHistory() {
  std::wstring file = source_path_;
  file_util::AppendToPath(&file, L"history.dat");
  ImportHistoryFromFirefox2(file, main_loop_, writer_);
}

void Firefox2Importer::ImportSearchEngines() {
  std::vector<std::wstring> files;
  GetSearchEnginesXMLFiles(&files);

  std::vector<TemplateURL*> search_engines;
  ParseSearchEnginesFromXMLFiles(files, &search_engines);
  main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
      &ProfileWriter::AddKeywords, search_engines,
      GetFirefoxDefaultSearchEngineIndex(search_engines, source_path_),
      true));
}

void Firefox2Importer::ImportHomepage() {
  GURL homepage = GetHomepage(source_path_);
  if (homepage.is_valid() && !IsDefaultHomepage(homepage, app_path_)) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddHomepage, homepage));
  }
}

void Firefox2Importer::GetSearchEnginesXMLFiles(
    std::vector<std::wstring>* files) {
  // Search engines are contained in XML files in a searchplugins directory that
  // can be found in 2 locations:
  // - Firefox install dir (default search engines)
  // - the profile dir (user added search engines)
  std::wstring dir(app_path_);
  file_util::AppendToPath(&dir, L"searchplugins");
  FindXMLFilesInDir(dir, files);

  std::wstring profile_dir = source_path_;
  file_util::AppendToPath(&profile_dir, L"searchplugins");
  FindXMLFilesInDir(profile_dir, files);
}

// static
bool Firefox2Importer::ParseCharsetFromLine(const std::string& line,
                                            std::string* charset) {
  const char kCharset[] = "charset=";
  if (StartsWithASCII(line, "<META", true) &&
      line.find("CONTENT=\"") != std::string::npos) {
    size_t begin = line.find(kCharset);
    if (begin == std::string::npos)
      return false;
    begin += std::string(kCharset).size();
    size_t end = line.find_first_of('\"', begin);
    *charset = line.substr(begin, end - begin);
    return true;
  }
  return false;
}

// static
bool Firefox2Importer::ParseFolderNameFromLine(const std::string& line,
                                               const std::string& charset,
                                               std::wstring* folder_name,
                                               bool* is_toolbar_folder) {
  const char kFolderOpen[] = "<DT><H3";
  const char kFolderClose[] = "</H3>";
  const char kToolbarFolderAttribute[] = "PERSONAL_TOOLBAR_FOLDER";

  if (!StartsWithASCII(line, kFolderOpen, true))
    return false;

  size_t end = line.find(kFolderClose);
  size_t tag_end = line.rfind('>', end) + 1;
  // If no end tag or start tag is broken, we skip to find the folder name.
  if (end == std::string::npos || tag_end < arraysize(kFolderOpen))
    return false;

  CodepageToWide(line.substr(tag_end, end - tag_end), charset.c_str(),
                 OnStringUtilConversionError::SKIP, folder_name);
  HTMLUnescape(folder_name);

  std::string attribute_list = line.substr(arraysize(kFolderOpen),
      tag_end - arraysize(kFolderOpen) - 1);
  std::string value;
  if (GetAttribute(attribute_list, kToolbarFolderAttribute, &value) &&
      LowerCaseEqualsASCII(value, "true"))
    *is_toolbar_folder = true;
  else
    *is_toolbar_folder = false;

  return true;
}

// static
bool Firefox2Importer::ParseBookmarkFromLine(const std::string& line,
                                             const std::string& charset,
                                             std::wstring* title,
                                             GURL* url,
                                             GURL* favicon,
                                             std::wstring* shortcut,
                                             Time* add_date,
                                             std::wstring* post_data) {
  const char kItemOpen[] = "<DT><A";
  const char kItemClose[] = "</A>";
  const char kFeedURLAttribute[] = "FEEDURL";
  const char kHrefAttribute[] = "HREF";
  const char kIconAttribute[] = "ICON";
  const char kShortcutURLAttribute[] = "SHORTCUTURL";
  const char kAddDateAttribute[] = "ADD_DATE";
  const char kPostDataAttribute[] = "POST_DATA";

  title->clear();
  *url = GURL();
  *favicon = GURL();
  shortcut->clear();
  post_data->clear();
  *add_date = Time();

  if (!StartsWithASCII(line, kItemOpen, true))
    return false;

  size_t end = line.find(kItemClose);
  size_t tag_end = line.rfind('>', end) + 1;
  if (end == std::string::npos || tag_end < arraysize(kItemOpen))
    return false;  // No end tag or start tag is broken.

  std::string attribute_list = line.substr(arraysize(kItemOpen),
      tag_end - arraysize(kItemOpen) - 1);

  // We don't import Live Bookmark folders, which is Firefox's RSS reading
  // feature, since the user never necessarily bookmarked them and we don't
  // have this feature to update their contents.
  std::string value;
  if (GetAttribute(attribute_list, kFeedURLAttribute, &value))
    return false;

  // Title
  CodepageToWide(line.substr(tag_end, end - tag_end), charset.c_str(),
                 OnStringUtilConversionError::SKIP, title);
  HTMLUnescape(title);

  // URL
  if (GetAttribute(attribute_list, kHrefAttribute, &value)) {
    ReplaceSubstringsAfterOffset(&value, 0, "%22", "\"");
    *url = GURL(value);
  }

  // Favicon
  if (GetAttribute(attribute_list, kIconAttribute, &value))
    *favicon = GURL(value);

  // Keyword
  if (GetAttribute(attribute_list, kShortcutURLAttribute, &value)) {
    CodepageToWide(value, charset.c_str(), OnStringUtilConversionError::SKIP,
                   shortcut);
    HTMLUnescape(shortcut);
  }

  // Add date
  if (GetAttribute(attribute_list, kAddDateAttribute, &value)) {
    int64 time = StringToInt64(value);
    // Upper bound it at 32 bits.
    if (0 < time && time < (1LL << 32))
      *add_date = Time::FromTimeT(time);
  }

  // Post data.
  if (GetAttribute(attribute_list, kPostDataAttribute, &value)) {
    CodepageToWide(value, charset.c_str(),
                   OnStringUtilConversionError::SKIP, post_data);
    HTMLUnescape(post_data);
  }

  return true;
}

// static
bool Firefox2Importer::GetAttribute(const std::string& attribute_list,
                                    const std::string& attribute,
                                    std::string* value) {
  const char kQuote[] = "\"";

  size_t begin = attribute_list.find(attribute + "=" + kQuote);
  if (begin == std::string::npos)
    return false;  // Can't find the attribute.

  begin = attribute_list.find(kQuote, begin) + 1;
  size_t end = attribute_list.find(kQuote, begin);
  if (end == std::string::npos)
    return false;  // The value is not quoted.

  *value = attribute_list.substr(begin, end - begin);
  return true;
}

// static
void Firefox2Importer::HTMLUnescape(std::wstring *text) {
  ReplaceSubstringsAfterOffset(text, 0, L"&lt;", L"<");
  ReplaceSubstringsAfterOffset(text, 0, L"&gt;", L">");
  ReplaceSubstringsAfterOffset(text, 0, L"&amp;", L"&");
  ReplaceSubstringsAfterOffset(text, 0, L"&quot;", L"\"");
  ReplaceSubstringsAfterOffset(text, 0, L"&#39;", L"\'");
}

// static
void Firefox2Importer::FindXMLFilesInDir(
    const std::wstring& dir,
    std::vector<std::wstring>* xml_files) {
  file_util::FileEnumerator file_enum(FilePath::FromWStringHack(dir), false,
                                      file_util::FileEnumerator::FILES,
                                      FILE_PATH_LITERAL("*.xml"));
  std::wstring file(file_enum.Next().ToWStringHack());
  while (!file.empty()) {
    xml_files->push_back(file);
    file = file_enum.Next().ToWStringHack();
  }
}

// static
void Firefox2Importer::DataURLToFaviconUsage(
    const GURL& link_url,
    const GURL& favicon_data,
    std::vector<history::ImportedFavIconUsage>* favicons) {
  if (!link_url.is_valid() || !favicon_data.is_valid() ||
      !favicon_data.SchemeIs(chrome::kDataScheme))
    return;

  // Parse the data URL.
  std::string mime_type, char_set, data;
  if (!net::DataURL::Parse(favicon_data, &mime_type, &char_set, &data) ||
      data.empty())
    return;

  history::ImportedFavIconUsage usage;
  if (!ReencodeFavicon(reinterpret_cast<const unsigned char*>(&data[0]),
                       data.size(), &usage.png_data))
    return;  // Unable to decode.

  // We need to make up a URL for the favicon. We use a version of the page's
  // URL so that we can be sure it will not collide.
  usage.favicon_url = GURL(std::string("made-up-favicon:") + link_url.spec());

  // We only have one URL per favicon for Firefox 2 bookmarks.
  usage.urls.insert(link_url);

  favicons->push_back(usage);
}
