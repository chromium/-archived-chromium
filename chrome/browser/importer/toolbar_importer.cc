// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/toolbar_importer.h"

#include <limits>
#include "base/string_util.h"
#include "base/rand_util.h"
#include "base/registry.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/libxml_utils.h"
#include "net/base/data_url.h"
#include "net/base/cookie_monster.h"

#include "generated_resources.h"


//
// ToolbarImporterUtils
//
const HKEY ToolbarImporterUtils::kToolbarInstallRegistryRoots[] =
    {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
const TCHAR* ToolbarImporterUtils::kToolbarRootRegistryFolder =
    L"Software\\Google\\Google Toolbar";
const TCHAR* ToolbarImporterUtils::kToolbarVersionRegistryFolder =
    L"SOFTWARE\\Google\\Google Toolbar\\Component";
const TCHAR* ToolbarImporterUtils::kToolbarVersionRegistryKey =
    L"CurrentVersion";


bool ToolbarImporterUtils::IsToolbarInstalled() {
  for (int index = 0;
      index < arraysize(kToolbarInstallRegistryRoots);
      ++index) {
    RegKey key(kToolbarInstallRegistryRoots[index],
               kToolbarRootRegistryFolder);
    if (key.Valid())
      return true;
  }
  return false;
}

bool ToolbarImporterUtils::IsGoogleGAIACookieInstalled() {
  URLRequestContext* context = Profile::GetDefaultRequestContext();
  net::CookieMonster* store= context->cookie_store();
  GURL url("http://.google.com/");
  net::CookieMonster::CookieOptions options = net::CookieMonster::NORMAL;
  std::string cookies = store->GetCookiesWithOptions(url, options);
  std::vector<std::string> cookie_list;
  SplitString(cookies, L';', &cookie_list);
  for (std::vector<std::string>::iterator current = cookie_list.begin();
       current != cookie_list.end();
       ++current) {
    size_t position = (*current).find("SID=");
    if (0 == position)
        return true;
  }
  return false;
}

TOOLBAR_VERSION ToolbarImporterUtils::GetToolbarVersion() {
  TOOLBAR_VERSION toolbar_version = NO_VERSION;
  for (int index = 0;
       (index < arraysize(kToolbarInstallRegistryRoots)) &&
       NO_VERSION == toolbar_version;
       ++index) {
    RegKey key(kToolbarInstallRegistryRoots[index],
               kToolbarVersionRegistryFolder);
    if (key.Valid() && key.ValueExists(kToolbarVersionRegistryKey)) {
      TCHAR version_buffer[128];
      DWORD version_buffer_length = sizeof(version_buffer);
      if (key.ReadValue(kToolbarVersionRegistryKey,
                        &version_buffer,
                        &version_buffer_length)) {
        int version_value = _wtoi(version_buffer);
        switch (version_value) {
          case 5: {
            toolbar_version = VERSION_5;
            break;
           }
          default: {
            toolbar_version = DEPRECATED;
            break;
          }
        }
      }
    }
  }
  return toolbar_version;
}

//
// Toolbar5Importer
//

const std::string Toolbar5Importer::kXmlApiReplyXmlTag = "xml_api_reply";
const std::string Toolbar5Importer::kBookmarksXmlTag = "bookmarks";
const std::string Toolbar5Importer::kBookmarkXmlTag = "bookmark";
const std::string Toolbar5Importer::kTitleXmlTag = "title";
const std::string Toolbar5Importer::kUrlXmlTag = "url";
const std::string Toolbar5Importer::kTimestampXmlTag = "timestamp";
const std::string Toolbar5Importer::kLabelsXmlTag = "labels";
const std::string Toolbar5Importer::kLabelXmlTag = "label";
const std::string Toolbar5Importer::kAttributesXmlTag = "attributes";
const std::string Toolbar5Importer::kAttributeXmlTag = "attribute";
const std::string Toolbar5Importer::kNameXmlTag = "name";
const std::string Toolbar5Importer::kValueXmlTag = "favicon";
const std::string Toolbar5Importer::kFaviconAttributeXmlName = "favicon_url";

const std::string Toolbar5Importer::kRandomNumberToken = "{random_number}";
const std::string Toolbar5Importer::kAuthorizationToken = "{auth_token}";
const std::string Toolbar5Importer::kAuthorizationTokenPrefix = "/*";
const std::string Toolbar5Importer::kAuthorizationTokenSuffix = "*/";
const std::string Toolbar5Importer::kMaxNumToken = "{max_num}";
const std::string Toolbar5Importer::kMaxTimestampToken = "{max_timestamp}";

const std::string Toolbar5Importer::kT5AuthorizationTokenUrl =
    "http://www.google.com/notebook/token?zx={random_number}";
const std::string Toolbar5Importer::kT5FrontEndUrlTemplate =
    "http://www.google.com/notebook/toolbar?cmd=list&tok={auth_token}& "
    "num={max_num}&min={max_timestamp}&all=0&zx={random_number}";
const std::string Toolbar5Importer::kT4FrontEndUrlTemplate =
    "http://www.google.com/bookmarks/?output=xml&num={max_num}&"
    "min={max_timestamp}&all=0&zx={random_number}";

// Importer methods.
Toolbar5Importer::Toolbar5Importer() : writer_(NULL),
                                       state_(NOT_USED),
                                       items_to_import_(NONE),
                                       token_fetcher_(NULL),
                                       data_fetcher_(NULL) {
}

Toolbar5Importer::~Toolbar5Importer() {
  DCHECK(!token_fetcher_);
  DCHECK(!data_fetcher_);
}

void Toolbar5Importer::StartImport(ProfileInfo profile_info,
                                   uint16 items,
                                   ProfileWriter* writer,
                                   ImporterHost* host) {
  DCHECK(writer);
  DCHECK(host);

  importer_host_ = host;
  writer_ = writer;
  items_to_import_ = items;
  state_ = INITIALIZED;

  NotifyStarted();
  ContinueImport();
}

void Toolbar5Importer::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  if (200 != response_code) {  // HTTP/Ok
    // Display to the user an error dialog and cancel the import
    EndImportBookmarks(false);
    return;
  }

  switch (state_) {
    case GET_AUTHORIZATION_TOKEN:
      GetBookmarkDataFromServer(data);
      break;
    case GET_BOOKMARKS:
      GetBookmarsFromServerDataResponse(data);
      break;
    default:
      NOTREACHED() << "Invalid state.";
      EndImportBookmarks(false);
      break;
  }
}

void Toolbar5Importer::ContinueImport() {
  DCHECK((items_to_import_ == FAVORITES) || (items_to_import_ == NONE)) <<
      "The items requested are not supported";

  // The order here is important.  Each Begin... will clear the flag
  // of its item before its task finishes and re-enters this method.
  if (NONE == items_to_import_) {
    EndImport();
  }
  if ((items_to_import_ & FAVORITES) && !cancelled()) {
    items_to_import_ &= ~FAVORITES;
    BeginImportBookmarks();
  }
  // TODO(brg): Import history, autocomplete, other toolbar information
  // for 2.0
}

void Toolbar5Importer::EndImport() {
  // By spec the fetcher's must be destroyed within the same
  // thread they are created.  The importer is destroyed in the ui_thread
  // so when we complete in the file_thread we destroy them first.
  if (NULL != token_fetcher_) {
    delete token_fetcher_;
    token_fetcher_ = NULL;
  }

  if (NULL != data_fetcher_) {
    delete data_fetcher_;
    data_fetcher_ = NULL;
  }

  NotifyEnded();
}

void Toolbar5Importer::BeginImportBookmarks() {
  NotifyItemStarted(FAVORITES);
  GetAuthenticationFromServer();
}

void Toolbar5Importer::EndImportBookmarks(bool success) {
  NotifyItemEnded(FAVORITES);
  ContinueImport();
}


// Notebook FE connection managers.
void Toolbar5Importer::GetAuthenticationFromServer() {
  // Authentication is a token string retreived from the authentication server
  // To access it we call the url below with a random number replacing the
  // value in the string.
  state_ = GET_AUTHORIZATION_TOKEN;

  // Random number construction.
  int random = base::RandInt(0, std::numeric_limits<int>::max());
  std::string random_string = UintToString(random);

  // Retrieve authorization token from the network.
  std::string url_string(kT5AuthorizationTokenUrl);
  url_string.replace(url_string.find(kRandomNumberToken),
                     kRandomNumberToken.size(),
                     random_string);
  GURL  url(url_string);

  token_fetcher_ = new  URLFetcher(url, URLFetcher::GET, this);
  token_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
  token_fetcher_->Start();
}

void Toolbar5Importer::GetBookmarkDataFromServer(const std::string& response) {
  state_ = GET_BOOKMARKS;

  // Parse and verify the authorization token from the response.
  std::string token;
  if (!ParseAuthenticationTokenResponse(response, &token)) {
    EndImportBookmarks(false);
    return;
  }

  // Build the Toolbar FE connection string, and call the server for
  // the xml blob.  We must tag the connection string with a random number.
  std::string conn_string = kT5FrontEndUrlTemplate;
  int random = base::RandInt(0, std::numeric_limits<int>::max());
  std::string random_string = UintToString(random);
  conn_string.replace(conn_string.find(kRandomNumberToken),
                      kRandomNumberToken.size(),
                      random_string);
  conn_string.replace(conn_string.find(kAuthorizationToken),
                      kAuthorizationToken.size(),
                      token);
  GURL  url(conn_string);

  data_fetcher_ = new URLFetcher(url, URLFetcher::GET, this);
  data_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
  data_fetcher_->Start();
}

void Toolbar5Importer::GetBookmarsFromServerDataResponse(
    const std::string& response) {
  bool retval = false;
  XmlReader reader;
  if (reader.Load(response) && !cancelled()) {
    // Construct Bookmarks
    std::vector< ProfileWriter::BookmarkEntry > bookmarks;
    std::vector< history::ImportedFavIconUsage > favicons;
    retval = ParseBookmarksFromReader(&reader, &bookmarks, &favicons);
    if (retval && !cancelled()) {
      AddBookMarksToChrome(bookmarks, favicons);
    }
  }
  EndImportBookmarks(retval);
}

bool Toolbar5Importer::ParseAuthenticationTokenResponse(
    const std::string& response,
    std::string* token) {
  DCHECK(token);

  *token = response;
  size_t position = token->find(kAuthorizationTokenPrefix);
  if (0 != position)
    return false;
  token->replace(position, kAuthorizationTokenPrefix.size(), "");

  position = token->find(kAuthorizationTokenSuffix);
  if (token->size() != (position + kAuthorizationTokenSuffix.size()))
    return false;
  token->replace(position, kAuthorizationTokenSuffix.size(), "");

  return true;
}

// Parsing
bool Toolbar5Importer::ParseBookmarksFromReader(
    XmlReader* reader,
    std::vector< ProfileWriter::BookmarkEntry >* bookmarks,
    std::vector< history::ImportedFavIconUsage >* favicons) {
  DCHECK(reader);
  DCHECK(bookmarks);
  DCHECK(favicons);

  // The XML blob returned from the server is described in the
  // Toolbar-Notebook/Bookmarks Protocol document located at
  // https://docs.google.com/a/google.com/Doc?docid=cgt3m7dr_24djt62m&hl=en
  // We are searching for the section with structure
  // <bookmarks><bookmark>...</bookmark><bookmark>...</bookmark></bookmarks>

  // Locate the |bookmarks| blob.
  if (!reader->SkipToElement())
    return false;

  if (!LocateNextTagByName(reader, kBookmarksXmlTag))
    return false;

  // Parse each |bookmark| blob
  while (LocateNextTagByName(reader, kBookmarkXmlTag)) {
    ProfileWriter::BookmarkEntry bookmark_entry;
    history::ImportedFavIconUsage favicon_entry;
    if (ExtractBookmarkInformation(reader, &bookmark_entry, &favicon_entry)) {
      bookmarks->push_back(bookmark_entry);
      if (!favicon_entry.favicon_url.is_empty())
        favicons->push_back(favicon_entry);
    }
  }

  return true;
}

bool Toolbar5Importer::LocateNextTagByName(XmlReader* reader,
                                           const std::string& tag) {
  // Locate the |tag| blob.
  while (tag != reader->NodeName()) {
    if (!reader->Read())
      return false;
  }
  return true;
}

bool Toolbar5Importer::ExtractBookmarkInformation(
    XmlReader* reader,
    ProfileWriter::BookmarkEntry* bookmark_entry,
    history::ImportedFavIconUsage* favicon_entry) {
  DCHECK(reader);
  DCHECK(bookmark_entry);
  DCHECK(favicon_entry);

  // The following is a typical bookmark entry.
  // The reader should be pointing to the <title> tag at the moment.
  //
  // <bookmark>
  // <title>MyTitle</title>
  // <url>http://www.sohu.com/</url>
  // <timestamp>1153328691085181</timestamp>
  // <id>N123nasdf239</id>
  // <notebook_id>Bxxxxxxx</notebook_id> (for bookmarks, a special id is used)
  // <section_id>Sxxxxxx</section_id>
  // <has_highlight>0</has_highlight>
  // <labels>
  // <label>China</label>
  // <label>^k</label> (if this special label is present, the note is deleted)
  // </labels>
  // <attributes>
  // <attribute>
  // <name>favicon_url</name>
  // <value>http://www.sohu.com/favicon.ico</value>
  // </attribute>
  // <attribute>
  // <name>favicon_timestamp</name>
  // <value>1153328653</value>
  // </attribute>
  // <attribute>
  // <name>notebook_name</name>
  // <value>My notebook 0</value>
  // </attribute>
  // <attribute>
  // <name>section_name</name>
  // <value>My section 0</value>
  // </attribute>
  // <attribute>
  // </attributes>
  // </bookmark>
  //
  // We parse the blob in order, title->url->timestamp etc.  Any failure
  // causes us to skip this bookmark.  Note Favicons are optional so failure
  // to find them is not a failure to parse the blob.

  if (!ExtractTitleFromXmlReader(reader, bookmark_entry))
    return false;
  if (!ExtractUrlFromXmlReader(reader, bookmark_entry))
    return false;
  if (!ExtractTimeFromXmlReader(reader, bookmark_entry))
    return false;
  if (!ExtractFolderFromXmlReader(reader, bookmark_entry))
    return false;
  ExtractFaviconFromXmlReader(reader, bookmark_entry, favicon_entry);

  return true;
}

bool Toolbar5Importer::ExtractNamedValueFromXmlReader(XmlReader* reader,
                                                      const std::string& name,
                                                      std::string* buffer) {
  DCHECK(reader);
  DCHECK(buffer);

  if (name != reader->NodeName())
    return false;
  if (!reader->ReadElementContent(buffer))
    return false;
  return true;
}

bool Toolbar5Importer::ExtractTitleFromXmlReader(
    XmlReader* reader,
    ProfileWriter::BookmarkEntry* entry) {
  DCHECK(reader);
  DCHECK(entry);

  if (!LocateNextTagByName(reader, kTitleXmlTag))
    return false;
  std::string buffer;
  if (!ExtractNamedValueFromXmlReader(reader, kTitleXmlTag, &buffer)) {
    return false;
  }
  entry->title = UTF8ToWide(buffer);
  return true;
}

bool Toolbar5Importer::ExtractUrlFromXmlReader(
    XmlReader* reader,
    ProfileWriter::BookmarkEntry* entry) {
  DCHECK(reader);
  DCHECK(entry);

  if (!LocateNextTagByName(reader, kUrlXmlTag))
    return false;
  std::string buffer;
  if (!ExtractNamedValueFromXmlReader(reader, kUrlXmlTag, &buffer)) {
    return false;
  }
  entry->url = GURL(buffer);
  return true;
}

bool Toolbar5Importer::ExtractTimeFromXmlReader(
    XmlReader* reader,
    ProfileWriter::BookmarkEntry* entry) {
  DCHECK(reader);
  DCHECK(entry);
  if (!LocateNextTagByName(reader, kTimestampXmlTag))
    return false;
  std::string buffer;
  if (!ExtractNamedValueFromXmlReader(reader, kTimestampXmlTag, &buffer)) {
    return false;
  }
  int64 timestamp;
  if (!StringToInt64(buffer, &timestamp)) {
    return false;
  }
  entry->creation_time = base::Time::FromTimeT(timestamp);
  return true;
}

bool Toolbar5Importer::ExtractFolderFromXmlReader(
    XmlReader* reader,
    ProfileWriter::BookmarkEntry* entry) {
  DCHECK(reader);
  DCHECK(entry);

  if (!LocateNextTagByName(reader, kLabelsXmlTag))
    return false;
  if (!LocateNextTagByName(reader, kLabelXmlTag))
    return false;

  std::vector<std::wstring> label_vector;
  std::string label_buffer;
  while (kLabelXmlTag == reader->NodeName() &&
         false != reader->ReadElementContent(&label_buffer)) {
    label_vector.push_back(UTF8ToWide(label_buffer));
  }

  // if this is the first run then we place favorites with no labels
  // in the title bar.  Else they are placed in the "Google Toolbar" folder.
  if (first_run() && 0 == label_vector.size()) {
    entry->in_toolbar = true;
  } else {
    entry->in_toolbar = false;
    entry->path.push_back(l10n_util::GetString(
                                      IDS_BOOKMARK_GROUP_FROM_GOOGLE_TOOLBAR));
  }

  // If there is only one label and it is in the form "xxx:yyy:zzz" this
  // was created from a Firefox folder.  We undo the label creation and
  // recreate the correct folder.
  if (1 == label_vector.size()) {
    std::vector< std::wstring > folder_names;
    SplitString(label_vector[0], L':', &folder_names);
    entry->path.insert(entry->path.end(),
                       folder_names.begin(), folder_names.end());
  } else if (0 != label_vector.size()) {
    std::wstring folder_name = label_vector[0];
    entry->path.push_back(folder_name);
  }

  return true;
}

bool Toolbar5Importer::ExtractFaviconFromXmlReader(
    XmlReader* reader,
    ProfileWriter::BookmarkEntry* bookmark_entry,
    history::ImportedFavIconUsage* favicon_entry) {
  DCHECK(reader);
  DCHECK(bookmark_entry);
  DCHECK(favicon_entry);

  if (!LocateNextTagByName(reader, kAttributesXmlTag))
    return false;
  if (!LocateNextTagByName(reader, kAttributeXmlTag))
    return false;
  if (!LocateNextTagByName(reader, kNameXmlTag))
    return false;

  // Attributes are <name>...</name><value>...</value> pairs.  The first
  // attribute should be the favicon name tage, and the value is the url.
  std::string buffer;
  if (!ExtractNamedValueFromXmlReader(reader, kNameXmlTag, &buffer))
    return false;
  if (kFaviconAttributeXmlName != buffer)
    return false;
  if (!ExtractNamedValueFromXmlReader(reader, kValueXmlTag, &buffer))
    return false;

  // Validate the url
  GURL favicon = GURL(buffer);
  if (!favicon.is_valid())
    return false;

  favicon_entry->favicon_url = favicon;
  favicon_entry->urls.insert(bookmark_entry->url);

  return true;
}

// Bookmark creation
void  Toolbar5Importer::AddBookMarksToChrome(
    const std::vector< ProfileWriter::BookmarkEntry >& bookmarks,
    const std::vector< history::ImportedFavIconUsage >& favicons) {
  if (!bookmarks.empty() && !cancelled()) {
      main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
          &ProfileWriter::AddBookmarkEntry, bookmarks));
  }

  if (!favicons.empty()) {
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddFavicons, favicons));
  }
}
