// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/toolbar_importer.h"

#include <limits>

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/rand_util.h"
#include "chrome/browser/first_run.h"
#include "chrome/common/libxml_utils.h"
#include "grit/generated_resources.h"
#include "net/base/cookie_monster.h"
#include "net/base/data_url.h"
#include "net/url_request/url_request_context.h"

//
// ToolbarImporterUtils
//
static const char* kGoogleDomainUrl = "http://.google.com/";
static const wchar_t kSplitStringToken = L';';
static const char* kGoogleDomainSecureCookieId = "SID=";

bool toolbar_importer_utils::IsGoogleGAIACookieInstalled() {
  URLRequestContext* context = Profile::GetDefaultRequestContext();
  net::CookieMonster* store = context->cookie_store();
  GURL url(kGoogleDomainUrl);
  net::CookieMonster::CookieOptions options;
  options.set_include_httponly();  // The SID cookie might be httponly.
  std::string cookies = store->GetCookiesWithOptions(url, options);
  std::vector<std::string> cookie_list;
  SplitString(cookies, kSplitStringToken, &cookie_list);
  for (std::vector<std::string>::iterator current = cookie_list.begin();
       current != cookie_list.end();
       ++current) {
    size_t position = (*current).find(kGoogleDomainSecureCookieId);
    if (0 == position)
      return true;
  }
  return false;
}

//
// Toolbar5Importer
//
const char Toolbar5Importer::kXmlApiReplyXmlTag[] = "xml_api_reply";
const char Toolbar5Importer::kBookmarksXmlTag[] = "bookmarks";
const char Toolbar5Importer::kBookmarkXmlTag[] = "bookmark";
const char Toolbar5Importer::kTitleXmlTag[] = "title";
const char Toolbar5Importer::kUrlXmlTag[] = "url";
const char Toolbar5Importer::kTimestampXmlTag[] = "timestamp";
const char Toolbar5Importer::kLabelsXmlTag[] = "labels";
const char Toolbar5Importer::kLabelsXmlCloseTag[] = "/labels";
const char Toolbar5Importer::kLabelXmlTag[] = "label";
const char Toolbar5Importer::kAttributesXmlTag[] = "attributes";

const char Toolbar5Importer::kRandomNumberToken[] = "{random_number}";
const char Toolbar5Importer::kAuthorizationToken[] = "{auth_token}";
const char Toolbar5Importer::kAuthorizationTokenPrefix[] = "/*";
const char Toolbar5Importer::kAuthorizationTokenSuffix[] = "*/";
const char Toolbar5Importer::kMaxNumToken[] = "{max_num}";
const char Toolbar5Importer::kMaxTimestampToken[] = "{max_timestamp}";

const char Toolbar5Importer::kT5AuthorizationTokenUrl[] =
    "http://www.google.com/notebook/token?zx={random_number}";
const char Toolbar5Importer::kT5FrontEndUrlTemplate[] =
    "http://www.google.com/notebook/toolbar?cmd=list&tok={auth_token}&"
    "num={max_num}&min={max_timestamp}&all=0&zx={random_number}";

// Importer methods.

// The constructor should set the initial state to NOT_USED.
Toolbar5Importer::Toolbar5Importer()
    : writer_(NULL),
      state_(NOT_USED),
      items_to_import_(NONE),
      token_fetcher_(NULL),
      data_fetcher_(NULL) {
}

// The destructor insures that the fetchers are currently not being used, as
// their thread-safe implementation requires that they are cancelled from the
// thread in which they were constructed.
Toolbar5Importer::~Toolbar5Importer() {
  DCHECK(!token_fetcher_);
  DCHECK(!data_fetcher_);
}

void Toolbar5Importer::StartImport(ProfileInfo profile_info,
                                   uint16 items,
                                   ProfileWriter* writer,
                                   MessageLoop* delagate_loop,
                                   ImporterHost* host) {
  DCHECK(writer);
  DCHECK(host);

  importer_host_ = host;
  delagate_loop_ = delagate_loop;
  writer_ = writer;
  items_to_import_ = items;
  state_ = INITIALIZED;

  NotifyStarted();
  ContinueImport();
}

// The public cancel method serves two functions, as a callback from the UI
// as well as an internal callback in case of cancel.  An internal callback
// is required since the URLFetcher must be destroyed from the thread it was
// created.
void Toolbar5Importer::Cancel() {
  // In the case when the thread is not importing messages we are to
  // cancel as soon as possible.
  Importer::Cancel();

  // If we are conducting network operations, post a message to the importer
  // thread for synchronization.
  if (NULL != delagate_loop_) {
    if (delagate_loop_ != MessageLoop::current()) {
      delagate_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &Toolbar5Importer::Cancel));
    } else {
      EndImport();
    }
  }
}

void Toolbar5Importer::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  if (cancelled()) {
    EndImport();
    return;
  }

  if (200 != response_code) {  // HTTP/Ok
    // Cancelling here will update the UI and bypass the rest of bookmark
    // import.
    EndImportBookmarks();
    return;
  }

  switch (state_) {
    case GET_AUTHORIZATION_TOKEN:
      GetBookmarkDataFromServer(data);
      break;
    case GET_BOOKMARKS:
      GetBookmarksFromServerDataResponse(data);
      break;
    default:
      NOTREACHED() << "Invalid state.";
      EndImportBookmarks();
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
    return;
  }
  if ((items_to_import_ & FAVORITES) && !cancelled()) {
    items_to_import_ &= ~FAVORITES;
    BeginImportBookmarks();
    return;
  }
  // TODO(brg): Import history, autocomplete, other toolbar information
  // in a future release.

  // This code should not be reached, but gracefully handles the possibility
  // that StartImport was called with unsupported items_to_import.
  if (!cancelled())
    EndImport();
}

void Toolbar5Importer::EndImport() {
  if (state_ != DONE) {
    state_ = DONE;
    // By spec the fetchers must be destroyed within the same
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
}

void Toolbar5Importer::BeginImportBookmarks() {
  NotifyItemStarted(FAVORITES);
  GetAuthenticationFromServer();
}

void Toolbar5Importer::EndImportBookmarks() {
  NotifyItemEnded(FAVORITES);
  ContinueImport();
}


// Notebook front-end connection manager implementation follows.
void Toolbar5Importer::GetAuthenticationFromServer() {
  if (cancelled()) {
    EndImport();
    return;
  }

  // Authentication is a token string retrieved from the authentication server
  // To access it we call the url below with a random number replacing the
  // value in the string.
  state_ = GET_AUTHORIZATION_TOKEN;

  // Random number construction.
  int random = base::RandInt(0, std::numeric_limits<int>::max());
  std::string random_string = UintToString(random);

  // Retrieve authorization token from the network.
  std::string url_string(kT5AuthorizationTokenUrl);
  url_string.replace(url_string.find(kRandomNumberToken),
                     arraysize(kRandomNumberToken) - 1,
                     random_string);
  GURL url(url_string);

  token_fetcher_ = new  URLFetcher(url, URLFetcher::GET, this);
  token_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
  token_fetcher_->Start();
}

void Toolbar5Importer::GetBookmarkDataFromServer(const std::string& response) {
  if (cancelled()) {
    EndImport();
    return;
  }

  state_ = GET_BOOKMARKS;

  // Parse and verify the authorization token from the response.
  std::string token;
  if (!ParseAuthenticationTokenResponse(response, &token)) {
    EndImportBookmarks();
    return;
  }

  // Build the Toolbar FE connection string, and call the server for
  // the xml blob.  We must tag the connection string with a random number.
  std::string conn_string = kT5FrontEndUrlTemplate;
  int random = base::RandInt(0, std::numeric_limits<int>::max());
  std::string random_string = UintToString(random);
  conn_string.replace(conn_string.find(kRandomNumberToken),
                      arraysize(kRandomNumberToken) - 1,
                      random_string);
  conn_string.replace(conn_string.find(kAuthorizationToken),
                      arraysize(kAuthorizationToken) - 1,
                      token);
  GURL url(conn_string);

  data_fetcher_ = new URLFetcher(url, URLFetcher::GET, this);
  data_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
  data_fetcher_->Start();
}

void Toolbar5Importer::GetBookmarksFromServerDataResponse(
    const std::string& response) {
  if (cancelled()) {
    EndImport();
    return;
  }

  state_ = PARSE_BOOKMARKS;

  XmlReader reader;
  if (reader.Load(response) && !cancelled()) {
    // Construct Bookmarks
    std::vector<ProfileWriter::BookmarkEntry> bookmarks;
    if (ParseBookmarksFromReader(&reader, &bookmarks))
      AddBookmarksToChrome(bookmarks);
  }
  EndImportBookmarks();
}

bool Toolbar5Importer::ParseAuthenticationTokenResponse(
    const std::string& response,
    std::string* token) {
  DCHECK(token);

  *token = response;
  size_t position = token->find(kAuthorizationTokenPrefix);
  if (0 != position)
    return false;
  token->replace(position, arraysize(kAuthorizationTokenPrefix) - 1, "");

  position = token->find(kAuthorizationTokenSuffix);
  if (token->size() != (position + (arraysize(kAuthorizationTokenSuffix) - 1)))
    return false;
  token->replace(position, arraysize(kAuthorizationTokenSuffix) - 1, "");

  return true;
}

// Parsing
bool Toolbar5Importer::ParseBookmarksFromReader(
    XmlReader* reader,
    std::vector<ProfileWriter::BookmarkEntry>* bookmarks) {
  DCHECK(reader);
  DCHECK(bookmarks);

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
  while (LocateNextTagWithStopByName(reader, kBookmarkXmlTag,
                                     kBookmarksXmlTag)) {
    ProfileWriter::BookmarkEntry bookmark_entry;
    std::vector<BookmarkFolderType> folders;
    if (ExtractBookmarkInformation(reader, &bookmark_entry, &folders)) {
      // For each folder we create a new bookmark entry.  Duplicates will
      // be detected when we attempt to create the bookmark in the profile.
      for (std::vector<BookmarkFolderType>::iterator folder = folders.begin();
          folder != folders.end();
          ++folder) {
        bookmark_entry.path = *folder;
        bookmarks->push_back(bookmark_entry);
      }
    }
  }

  if (0 == bookmarks->size())
    return false;

  return true;
}

bool Toolbar5Importer::LocateNextOpenTag(XmlReader* reader) {
  DCHECK(reader);

  while (!reader->SkipToElement()) {
    if (!reader->Read())
      return false;
  }
  return true;
}

bool Toolbar5Importer::LocateNextTagByName(XmlReader* reader,
                                           const std::string& tag) {
  DCHECK(reader);

  // Locate the |tag| blob.
  while (tag != reader->NodeName()) {
    if (!reader->Read() || !LocateNextOpenTag(reader))
      return false;
  }
  return true;
}

bool Toolbar5Importer::LocateNextTagWithStopByName(XmlReader* reader,
                                                   const std::string& tag,
                                                   const std::string& stop) {
  DCHECK(reader);

  DCHECK_NE(tag, stop);
  // Locate the |tag| blob.
  while (tag != reader->NodeName()) {
    // Move to the next open tag.
    if (!reader->Read() || !LocateNextOpenTag(reader))
      return false;
    // If we encounter the stop word return false.
    if (stop == reader->NodeName())
      return false;
  }
  return true;
}

bool Toolbar5Importer::ExtractBookmarkInformation(
    XmlReader* reader,
    ProfileWriter::BookmarkEntry* bookmark_entry,
    std::vector<BookmarkFolderType>* bookmark_folders) {
  DCHECK(reader);
  DCHECK(bookmark_entry);
  DCHECK(bookmark_folders);

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
  // </attributes>
  // </bookmark>
  //
  // We parse the blob in order, title->url->timestamp etc.  Any failure
  // causes us to skip this bookmark.

  if (!ExtractTitleFromXmlReader(reader, bookmark_entry))
    return false;
  if (!ExtractUrlFromXmlReader(reader, bookmark_entry))
    return false;
  if (!ExtractTimeFromXmlReader(reader, bookmark_entry))
    return false;
  if (!ExtractFoldersFromXmlReader(reader, bookmark_folders))
    return false;

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

  if (!LocateNextTagWithStopByName(reader, kTitleXmlTag, kUrlXmlTag))
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

  if (!LocateNextTagWithStopByName(reader, kUrlXmlTag, kTimestampXmlTag))
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
  if (!LocateNextTagWithStopByName(reader, kTimestampXmlTag, kLabelsXmlTag))
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

bool Toolbar5Importer::ExtractFoldersFromXmlReader(
    XmlReader* reader,
    std::vector<BookmarkFolderType>* bookmark_folders) {
  DCHECK(reader);
  DCHECK(bookmark_folders);

  // Read in the labels for this bookmark from the xml.  There may be many
  // labels for any one bookmark.
  if (!LocateNextTagWithStopByName(reader, kLabelsXmlTag, kAttributesXmlTag))
    return false;

  // It is within scope to have an empty labels section, so we do not
  // return false if the labels are empty.
  if (!reader->Read() || !LocateNextOpenTag(reader))
    return false;

  std::vector<std::wstring> label_vector;
  while (kLabelXmlTag == reader->NodeName()) {
    std::string label_buffer;
    if (!reader->ReadElementContent(&label_buffer)) {
      label_buffer = "";
    }
    label_vector.push_back(UTF8ToWide(label_buffer));
    LocateNextOpenTag(reader);
  }

  if (0 == label_vector.size()) {
    if (!FirstRun::IsChromeFirstRun()) {
      bookmark_folders->resize(1);
      (*bookmark_folders)[0].push_back(
          l10n_util::GetString(IDS_BOOKMARK_GROUP_FROM_GOOGLE_TOOLBAR));
    }
    return true;
  }

  // We will be making one bookmark folder for each label.
  bookmark_folders->resize(label_vector.size());

  for (size_t index = 0; index < label_vector.size(); ++index) {
    // If this is the first run then we place favorites with no labels
    // in the title bar.  Else they are placed in the "Google Toolbar" folder.
    if (!FirstRun::IsChromeFirstRun() || !label_vector[index].empty()) {
      (*bookmark_folders)[index].push_back(
          l10n_util::GetString(IDS_BOOKMARK_GROUP_FROM_GOOGLE_TOOLBAR));
    }

    // If the label and is in the form "xxx:yyy:zzz" this was created from an
    // IE or Firefox folder.  We undo the label creation and recreate the
    // correct folder.
    std::vector<std::wstring> folder_names;
    SplitString(label_vector[index], L':', &folder_names);
    (*bookmark_folders)[index].insert((*bookmark_folders)[index].end(),
        folder_names.begin(), folder_names.end());
  }

  return true;
}

// Bookmark creation
void  Toolbar5Importer::AddBookmarksToChrome(
    const std::vector<ProfileWriter::BookmarkEntry>& bookmarks) {
  if (!bookmarks.empty() && !cancelled()) {
    int options = ProfileWriter::ADD_IF_UNIQUE |
        (import_to_bookmark_bar() ? ProfileWriter::IMPORT_TO_BOOKMARK_BAR : 0);
    main_loop_->PostTask(FROM_HERE, NewRunnableMethod(writer_,
        &ProfileWriter::AddBookmarkEntry, bookmarks,
        l10n_util::GetString(IDS_BOOKMARK_GROUP_FROM_GOOGLE_TOOLBAR),
        options));
  }
}
