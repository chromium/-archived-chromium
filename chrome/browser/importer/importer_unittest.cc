// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <atlbase.h>
#include <windows.h>
#include <unknwn.h>
#include <intshcut.h>
#include <pstore.h>
#include <urlhist.h>
#include <shlguid.h>
#include <vector>

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/importer/ie_importer.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/win_util.h"

class ImporterTest : public testing::Test {
 public:
 protected:
  virtual void SetUp() {
    // Creates a new profile in a new subdirectory in the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_path_));
    file_util::AppendToPath(&test_path_, L"ImporterTest");
    file_util::Delete(test_path_, true);
    CreateDirectory(test_path_.c_str(), NULL);
    profile_path_ = test_path_;
    file_util::AppendToPath(&profile_path_, L"profile");
    CreateDirectory(profile_path_.c_str(), NULL);
    app_path_ = test_path_;
    file_util::AppendToPath(&app_path_, L"app");
    CreateDirectory(app_path_.c_str(), NULL);
  }

  virtual void TearDown() {
    // Deletes the profile and cleans up the profile directory.
    ASSERT_TRUE(file_util::Delete(test_path_, true));
    ASSERT_FALSE(file_util::PathExists(test_path_));
  }

  MessageLoopForUI message_loop_;
  std::wstring test_path_;
  std::wstring profile_path_;
  std::wstring app_path_;
};

const int kMaxPathSize = 5;

typedef struct {
  const bool in_toolbar;
  const int path_size;
  const wchar_t* path[kMaxPathSize];
  const wchar_t* title;
  const char* url;
} BookmarkList;

typedef struct {
  const char* origin;
  const char* action;
  const char* realm;
  const wchar_t* username_element;
  const wchar_t* username;
  const wchar_t* password_element;
  const wchar_t* password;
  bool blacklisted;
} PasswordList;

static const BookmarkList kIEBookmarks[] = {
  {true, 0, {},
   L"TheLink",
   "http://www.links-thelink.com/"},
  {true, 1, {L"SubFolderOfLinks"},
   L"SubLink",
   "http://www.links-sublink.com/"},
  {false, 0, {},
   L"Google Home Page",
   "http://www.google.com/"},
  {false, 0, {},
   L"TheLink",
   "http://www.links-thelink.com/"},
  {false, 1, {L"SubFolder"},
   L"Title",
   "http://www.link.com/"},
  {false, 0, {},
   L"WithPortAndQuery",
   "http://host:8080/cgi?q=query"},
  {false, 1, {L"a"},
   L"\x4E2D\x6587",
   "http://chinese-title-favorite/"},
};

static const wchar_t* kIEIdentifyUrl =
    L"http://A79029D6-753E-4e27-B807-3D46AB1545DF.com:8080/path?key=value";
static const wchar_t* kIEIdentifyTitle =
    L"Unittest GUID";

bool IsWindowsVista() {
  OSVERSIONINFO info = {0};
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&info);
  return (info.dwMajorVersion >=6);
}

// Returns true if the |entry| is in the |list|.
bool FindBookmarkEntry(const ProfileWriter::BookmarkEntry& entry,
                       const BookmarkList* list, int list_size) {
  for (int i = 0; i < list_size; ++i)
    if (list[i].in_toolbar == entry.in_toolbar &&
        list[i].path_size == entry.path.size() &&
        list[i].url == entry.url.spec() &&
        list[i].title == entry.title) {
      bool equal = true;
      for (int k = 0; k < list[i].path_size; ++k)
        if (list[i].path[k] != entry.path[k]) {
          equal = false;
          break;
        }

      if (equal)
        return true;
    }
  return false;
}

class TestObserver : public ProfileWriter,
                     public ImporterHost::Observer {
 public:
  TestObserver() : ProfileWriter(NULL) {
    bookmark_count_ = 0;
    history_count_ = 0;
    password_count_ = 0;
  }

  virtual void ImportItemStarted(ImportItem item) {}
  virtual void ImportItemEnded(ImportItem item) {}
  virtual void ImportStarted() {}
  virtual void ImportEnded() {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kIEBookmarks), bookmark_count_);
    EXPECT_EQ(1, history_count_);
#if 0  // This part of the test is disabled. See bug #2466
    if (IsWindowsVista())
      EXPECT_EQ(0, password_count_);
    else
      EXPECT_EQ(1, password_count_);
#endif
  }

  virtual bool BookmarkModelIsLoaded() const {
    // Profile is ready for writing.
    return true;
  }

  virtual void AddBookmarkModelObserver(BookmarkModelObserver* observer) {
    NOTREACHED();
  }

  virtual bool TemplateURLModelIsLoaded() const {
    return true;
  }

  virtual void AddTemplateURLModelObserver(NotificationObserver* observer) {
    NOTREACHED();
  }

  virtual void AddPasswordForm(const PasswordForm& form) {
    // Importer should obtain this password form only.
    EXPECT_EQ(GURL("http://localhost:8080/security/index.htm"), form.origin);
    EXPECT_EQ("http://localhost:8080/", form.signon_realm);
    EXPECT_EQ(L"user", form.username_element);
    EXPECT_EQ(L"1", form.username_value);
    EXPECT_EQ(L"", form.password_element);
    EXPECT_EQ(L"2", form.password_value);
    EXPECT_EQ("", form.action.spec());
    ++password_count_;
  }

  virtual void AddHistoryPage(const std::vector<history::URLRow>& page) {
    // Importer should read the specified URL.
    for (size_t i = 0; i < page.size(); ++i)
      if (page[i].title() == kIEIdentifyTitle &&
          page[i].url() == GURL(kIEIdentifyUrl))
        ++history_count_;
  }

  virtual void AddBookmarkEntry(const std::vector<BookmarkEntry>& bookmark,
                                const std::wstring& first_folder_name,
                                int options) {
    // Importer should import the IE Favorites folder the same as the list.
    for (size_t i = 0; i < bookmark.size(); ++i) {
      if (FindBookmarkEntry(bookmark[i], kIEBookmarks,
                            arraysize(kIEBookmarks)))
        ++bookmark_count_;
    }
  }

  virtual void AddKeyword(std::vector<TemplateURL*> template_url,
                          int default_keyword_index) {
    // TODO(jcampan): bug 1169230: we should test keyword importing for IE.
    // In order to do that we'll probably need to mock the Windows registry.
    NOTREACHED();
    STLDeleteContainerPointers(template_url.begin(), template_url.end());
  }

 private:
  int bookmark_count_;
  int history_count_;
  int password_count_;
};

bool CreateUrlFile(std::wstring file, std::wstring url) {
  CComPtr<IUniformResourceLocator> locator;
  HRESULT result = locator.CoCreateInstance(CLSID_InternetShortcut, NULL,
                                            CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;
  CComPtr<IPersistFile> persist_file;
  result = locator->QueryInterface(IID_IPersistFile,
                                   reinterpret_cast<void**>(&persist_file));
  if (FAILED(result))
    return false;
  result = locator->SetURL(url.c_str(), 0);
  if (FAILED(result))
    return false;
  result = persist_file->Save(file.c_str(), TRUE);
  if (FAILED(result))
    return false;
  return true;
}

void ClearPStoreType(IPStore* pstore, const GUID* type, const GUID* subtype) {
  CComPtr<IEnumPStoreItems> item;
  HRESULT result = pstore->EnumItems(0, type, subtype, 0, &item);
  if (result == PST_E_OK) {
    wchar_t* item_name;
    while (SUCCEEDED(item->Next(1, &item_name, 0))) {
      pstore->DeleteItem(0, type, subtype, item_name, NULL, 0);
      CoTaskMemFree(item_name);
    }
  }
  pstore->DeleteSubtype(0, type, subtype, 0);
  pstore->DeleteType(0, type, 0);
}

void WritePStore(IPStore* pstore, const GUID* type, const GUID* subtype) {
  struct PStoreItem {
    wchar_t* name;
    int data_size;
    char* data;
  } items[] = {
    {L"http://localhost:8080/security/index.htm#ref:StringData", 8,
     "\x31\x00\x00\x00\x32\x00\x00\x00"},
    {L"http://localhost:8080/security/index.htm#ref:StringIndex", 20,
     "\x57\x49\x43\x4b\x18\x00\x00\x00\x02\x00"
     "\x00\x00\x2f\x00\x74\x00\x01\x00\x00\x00"},
    {L"user:StringData", 4,
     "\x31\x00\x00\x00"},
    {L"user:StringIndex", 20,
     "\x57\x49\x43\x4b\x18\x00\x00\x00\x01\x00"
     "\x00\x00\x2f\x00\x74\x00\x00\x00\x00\x00"},
  };

  for (int i = 0; i < arraysize(items); ++i) {
    HRESULT res = pstore->WriteItem(0, type, subtype, items[i].name,
        items[i].data_size, reinterpret_cast<BYTE*>(items[i].data),
        NULL, 0, 0);
    ASSERT_TRUE(res == PST_E_OK);
  }
}

TEST_F(ImporterTest, IEImporter) {
  // Sets up a favorites folder.
  win_util::ScopedCOMInitializer com_init;
  std::wstring path = test_path_;
  file_util::AppendToPath(&path, L"Favorites");
  CreateDirectory(path.c_str(), NULL);
  CreateDirectory((path + L"\\SubFolder").c_str(), NULL);
  CreateDirectory((path + L"\\Links").c_str(), NULL);
  CreateDirectory((path + L"\\Links\\SubFolderOfLinks").c_str(), NULL);
  CreateDirectory((path + L"\\\x0061").c_str(), NULL);
  ASSERT_TRUE(CreateUrlFile(path + L"\\Google Home Page.url",
                            L"http://www.google.com/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\SubFolder\\Title.url",
                            L"http://www.link.com/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\TheLink.url",
                            L"http://www.links-thelink.com/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\WithPortAndQuery.url",
                            L"http://host:8080/cgi?q=query"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\\x0061\\\x4E2D\x6587.url",
                            L"http://chinese-title-favorite/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\Links\\TheLink.url",
                            L"http://www.links-thelink.com/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\Links\\SubFolderOfLinks\\SubLink.url",
                            L"http://www.links-sublink.com/"));
  file_util::WriteFile(path + L"\\InvalidUrlFile.url", "x", 1);
  file_util::WriteFile(path + L"\\PlainTextFile.txt", "x", 1);

  // Sets up dummy password data.
  HRESULT res;
  #if 0  // This part of the test is disabled. See bug #2466
  CComPtr<IPStore> pstore;
  HMODULE pstorec_dll;
  GUID type = IEImporter::kUnittestGUID;
  GUID subtype = IEImporter::kUnittestGUID;
  // PStore is read-only in Windows Vista.
  if (!IsWindowsVista()) {
    typedef HRESULT (WINAPI *PStoreCreateFunc)(IPStore**, DWORD, DWORD, DWORD);
    pstorec_dll = LoadLibrary(L"pstorec.dll");
    PStoreCreateFunc PStoreCreateInstance =
        (PStoreCreateFunc)GetProcAddress(pstorec_dll, "PStoreCreateInstance");
    res = PStoreCreateInstance(&pstore, 0, 0, 0);
    ASSERT_TRUE(res == S_OK);
    ClearPStoreType(pstore, &type, &subtype);
    PST_TYPEINFO type_info;
    type_info.szDisplayName = L"TestType";
    type_info.cbSize = 8;
    pstore->CreateType(0, &type, &type_info, 0);
    pstore->CreateSubtype(0, &type, &subtype, &type_info, NULL, 0);
    WritePStore(pstore, &type, &subtype);
  }
#endif

  // Sets up a special history link.
  CComPtr<IUrlHistoryStg2> url_history_stg2;
  res = url_history_stg2.CoCreateInstance(CLSID_CUrlHistory, NULL,
                                          CLSCTX_INPROC_SERVER);
  ASSERT_TRUE(res == S_OK);
  res = url_history_stg2->AddUrl(kIEIdentifyUrl, kIEIdentifyTitle, 0);
  ASSERT_TRUE(res == S_OK);

  // Starts to import the above settings.
  MessageLoop* loop = MessageLoop::current();
  scoped_refptr<ImporterHost> host = new ImporterHost(loop);

  TestObserver* observer = new TestObserver();
  host->SetObserver(observer);
  ProfileInfo profile_info;
  profile_info.browser_type = MS_IE;
  profile_info.source_path = test_path_;

  loop->PostTask(FROM_HERE, NewRunnableMethod(host.get(),
      &ImporterHost::StartImportSettings, profile_info,
      HISTORY | PASSWORDS | FAVORITES, observer, true));
  loop->Run();

  // Cleans up.
  url_history_stg2->DeleteUrl(kIEIdentifyUrl, 0);
  url_history_stg2.Release();
#if 0  // This part of the test is disabled. See bug #2466
  if (!IsWindowsVista()) {
    ClearPStoreType(pstore, &type, &subtype);
    // Releases it befor unload the dll.
    pstore.Release();
    FreeLibrary(pstorec_dll);
  }
#endif
}

TEST_F(ImporterTest, IE7Importer) {
  // This is the unencrypted values of my keys under Storage2.
  // The passwords have been manually changed to abcdef... but the size remains
  // the same.
  unsigned char data1[] = "\x0c\x00\x00\x00\x38\x00\x00\x00\x2c\x00\x00\x00"
                          "\x57\x49\x43\x4b\x18\x00\x00\x00\x02\x00\x00\x00"
                          "\x67\x00\x72\x00\x01\x00\x00\x00\x00\x00\x00\x00"
                          "\x00\x00\x00\x00\x4e\xfa\x67\x76\x22\x94\xc8\x01"
                          "\x08\x00\x00\x00\x12\x00\x00\x00\x4e\xfa\x67\x76"
                          "\x22\x94\xc8\x01\x0c\x00\x00\x00\x61\x00\x62\x00"
                          "\x63\x00\x64\x00\x65\x00\x66\x00\x67\x00\x68\x00"
                          "\x00\x00\x61\x00\x62\x00\x63\x00\x64\x00\x65\x00"
                          "\x66\x00\x67\x00\x68\x00\x69\x00\x6a\x00\x6b\x00"
                          "\x6c\x00\x00\x00";

  unsigned char data2[] = "\x0c\x00\x00\x00\x38\x00\x00\x00\x24\x00\x00\x00"
                          "\x57\x49\x43\x4b\x18\x00\x00\x00\x02\x00\x00\x00"
                          "\x67\x00\x72\x00\x01\x00\x00\x00\x00\x00\x00\x00"
                          "\x00\x00\x00\x00\xa8\xea\xf4\xe5\x9f\x9a\xc8\x01"
                          "\x09\x00\x00\x00\x14\x00\x00\x00\xa8\xea\xf4\xe5"
                          "\x9f\x9a\xc8\x01\x07\x00\x00\x00\x61\x00\x62\x00"
                          "\x63\x00\x64\x00\x65\x00\x66\x00\x67\x00\x68\x00"
                          "\x69\x00\x00\x00\x61\x00\x62\x00\x63\x00\x64\x00"
                          "\x65\x00\x66\x00\x67\x00\x00\x00";



  std::vector<unsigned char> decrypted_data1;
  decrypted_data1.resize(arraysize(data1));
  memcpy(&decrypted_data1.front(), data1, sizeof(data1));

  std::vector<unsigned char> decrypted_data2;
  decrypted_data2.resize(arraysize(data2));
  memcpy(&decrypted_data2.front(), data2, sizeof(data2));

  std::wstring password;
  std::wstring username;
  ASSERT_TRUE(ie7_password::GetUserPassFromData(decrypted_data1, &username,
                                                &password));
  EXPECT_EQ(L"abcdefgh", username);
  EXPECT_EQ(L"abcdefghijkl", password);

  ASSERT_TRUE(ie7_password::GetUserPassFromData(decrypted_data2, &username,
                                                &password));
  EXPECT_EQ(L"abcdefghi", username);
  EXPECT_EQ(L"abcdefg", password);
}

static const BookmarkList kFirefox2Bookmarks[] = {
  {true, 1, {L"Folder"},
   L"On Toolbar's Subfolder",
   "http://on.toolbar/bookmark/folder"},
  {true, 0, {},
   L"On Bookmark Toolbar",
   "http://on.toolbar/bookmark"},
  {false, 1, {L"Folder"},
   L"New Bookmark",
   "http://domain/"},
  {false, 0, {},
   L"<Name>",
   "http://domain.com/q?a=\"er\"&b=%3C%20%20%3E"},
  {false, 0, {},
   L"Google Home Page",
   "http://www.google.com/"},
  {false, 0, {},
   L"\x4E2D\x6587",
   "http://chinese.site.cn/path?query=1#ref"},
  {false, 0, {},
   L"mail",
   "mailto:username@host"},
};

static const PasswordList kFirefox2Passwords[] = {
  {"https://www.google.com/", "", "https://www.google.com/",
    L"", L"", L"", L"", true},
  {"http://localhost:8080/", "", "http://localhost:8080/corp.google.com",
    L"", L"http", L"", L"Http1+1abcdefg", false},
  {"http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/",
    L"loginuser", L"usr", L"loginpass", L"pwd", false},
  {"http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/",
    L"loginuser", L"firefox", L"loginpass", L"firefox", false},
  {"http://localhost/", "", "http://localhost/",
    L"loginuser", L"hello", L"", L"world", false},
};

typedef struct {
  const wchar_t* keyword;
  const wchar_t* url;
} KeywordList;

static const KeywordList kFirefox2Keywords[] = {
  // Searh plugins
  { L"amazon.com",
    L"http://www.amazon.com/exec/obidos/external-search/?field-keywords="
    L"{searchTerms}&mode=blended" },
  { L"answers.com",
    L"http://www.answers.com/main/ntquery?s={searchTerms}&gwp=13" },
  { L"search.creativecommons.org",
    L"http://search.creativecommons.org/?q={searchTerms}" },
  { L"search.ebay.com",
    L"http://search.ebay.com/search/search.dll?query={searchTerms}&"
    L"MfcISAPICommand=GetResult&ht=1&ebaytag1=ebayreg&srchdesc=n&"
    L"maxRecordsReturned=300&maxRecordsPerPage=50&SortProperty=MetaEndSort" },
  { L"google.com",
    L"http://www.google.com/search?q={searchTerms}&ie=utf-8&oe=utf-8&aq=t" },
  { L"search.yahoo.com",
    L"http://search.yahoo.com/search?p={searchTerms}&ei=UTF-8" },
  { L"flickr.com",
    L"http://www.flickr.com/photos/tags/?q={searchTerms}" },
  { L"imdb.com",
    L"http://www.imdb.com/find?q={searchTerms}" },
  { L"webster.com",
    L"http://www.webster.com/cgi-bin/dictionary?va={searchTerms}" },
  // Search keywords.
  { L"google", L"http://www.google.com/" },
  { L"< > & \" ' \\ /", L"http://g.cn/"},
};

static const int kDefaultFirefox2KeywordIndex = 8;

class FirefoxObserver : public ProfileWriter,
                        public ImporterHost::Observer {
 public:
  FirefoxObserver() : ProfileWriter(NULL) {
    bookmark_count_ = 0;
    history_count_ = 0;
    password_count_ = 0;
    keyword_count_ = 0;
  }

  virtual void ImportItemStarted(ImportItem item) {}
  virtual void ImportItemEnded(ImportItem item) {}
  virtual void ImportStarted() {}
  virtual void ImportEnded() {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kFirefox2Bookmarks), bookmark_count_);
    EXPECT_EQ(1, history_count_);
    EXPECT_EQ(arraysize(kFirefox2Passwords), password_count_);
    EXPECT_EQ(arraysize(kFirefox2Keywords), keyword_count_);
    EXPECT_EQ(kFirefox2Keywords[kDefaultFirefox2KeywordIndex].keyword,
              default_keyword_);
    EXPECT_EQ(kFirefox2Keywords[kDefaultFirefox2KeywordIndex].url,
              default_keyword_url_);
  }

  virtual bool BookmarkModelIsLoaded() const {
    // Profile is ready for writing.
    return true;
  }

  virtual void AddBookmarkModelObserver(BookmarkModelObserver* observer) {
    NOTREACHED();
  }

  virtual bool TemplateURLModelIsLoaded() const {
    return true;
  }

  virtual void AddTemplateURLModelObserver(NotificationObserver* observer) {
    NOTREACHED();
  }

  virtual void AddPasswordForm(const PasswordForm& form) {
    PasswordList p = kFirefox2Passwords[password_count_];
    EXPECT_EQ(p.origin, form.origin.spec());
    EXPECT_EQ(p.realm, form.signon_realm);
    EXPECT_EQ(p.action, form.action.spec());
    EXPECT_EQ(p.username_element, form.username_element);
    EXPECT_EQ(p.username, form.username_value);
    EXPECT_EQ(p.password_element, form.password_element);
    EXPECT_EQ(p.password, form.password_value);
    EXPECT_EQ(p.blacklisted, form.blacklisted_by_user);
    ++password_count_;
  }

  virtual void AddHistoryPage(const std::vector<history::URLRow>& page) {
    EXPECT_EQ(1, page.size());
    EXPECT_EQ("http://en-us.www.mozilla.com/", page[0].url().spec());
    EXPECT_EQ(L"Firefox Updated", page[0].title());
    ++history_count_;
  }

  virtual void AddBookmarkEntry(const std::vector<BookmarkEntry>& bookmark,
                                const std::wstring& first_folder_name,
                                int options) {
    for (size_t i = 0; i < bookmark.size(); ++i) {
      if (FindBookmarkEntry(bookmark[i], kFirefox2Bookmarks,
                            arraysize(kFirefox2Bookmarks)))
        ++bookmark_count_;
    }
  }

  virtual void AddKeywords(const std::vector<TemplateURL*>& template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path) {
    for (size_t i = 0; i < template_urls.size(); ++i) {
      // The order might not be deterministic, look in the expected list for
      // that template URL.
      bool found = false;
      std::wstring keyword = template_urls[i]->keyword();
      for (int j = 0; j < arraysize(kFirefox2Keywords); ++j) {
        const wchar_t* comp_keyword = kFirefox2Keywords[j].keyword;
        bool equal = (keyword == comp_keyword);
        if (template_urls[i]->keyword() == kFirefox2Keywords[j].keyword) {
          EXPECT_EQ(kFirefox2Keywords[j].url, template_urls[i]->url()->url());
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
      ++keyword_count_;
    }

    if (default_keyword_index != -1) {
      EXPECT_LT(default_keyword_index, static_cast<int>(template_urls.size()));
      TemplateURL* default_turl = template_urls[default_keyword_index];
      default_keyword_ = default_turl->keyword();
      default_keyword_url_ = default_turl->url()->url();
    }

    STLDeleteContainerPointers(template_urls.begin(), template_urls.end());
  }

  void AddFavicons(const std::vector<history::ImportedFavIconUsage>& favicons) {
  }

 private:
  int bookmark_count_;
  int history_count_;
  int password_count_;
  int keyword_count_;
  std::wstring default_keyword_;
  std::wstring default_keyword_url_;
};

TEST_F(ImporterTest, Firefox2Importer) {
  std::wstring data_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  file_util::AppendToPath(&data_path, L"firefox2_profile\\*");
  ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, true));
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  file_util::AppendToPath(&data_path, L"firefox2_nss");
  ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, false));

  std::wstring search_engine_path = app_path_;
  file_util::AppendToPath(&search_engine_path, L"searchplugins");
  CreateDirectory(search_engine_path.c_str(), NULL);
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  file_util::AppendToPath(&data_path, L"firefox2_searchplugins");
  if (!file_util::PathExists(data_path)) {
    // TODO(maruel):  Create test data that we can open source!
    LOG(ERROR) << L"Missing internal test data";
    return;
  }
  ASSERT_TRUE(file_util::CopyDirectory(data_path, search_engine_path, false));

  MessageLoop* loop = MessageLoop::current();
  scoped_refptr<ImporterHost> host = new ImporterHost(loop);
  FirefoxObserver* observer = new FirefoxObserver();
  host->SetObserver(observer);
  ProfileInfo profile_info;
  profile_info.browser_type = FIREFOX2;
  profile_info.app_path = app_path_;
  profile_info.source_path = profile_path_;

  loop->PostTask(FROM_HERE, NewRunnableMethod(host.get(),
      &ImporterHost::StartImportSettings, profile_info,
      HISTORY | PASSWORDS | FAVORITES | SEARCH_ENGINES, observer, true));
  loop->Run();
}

static const BookmarkList kFirefox3Bookmarks[] = {
  {true, 0, {},
    L"Toolbar",
    "http://site/"},
  {false, 0, {},
    L"Title",
    "http://www.google.com/"},
};

static const PasswordList kFirefox3Passwords[] = {
  {"http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/",
    L"loginuser", L"abc", L"loginpass", L"123", false},
  {"http://localhost:8080/", "", "http://localhost:8080/localhost",
    L"", L"http", L"", L"Http1+1abcdefg", false},
};

static const KeywordList kFirefox3Keywords[] = {
  { L"amazon.com",
    L"http://www.amazon.com/exec/obidos/external-search/?field-keywords="
    L"{searchTerms}&mode=blended" },
  { L"answers.com",
    L"http://www.answers.com/main/ntquery?s={searchTerms}&gwp=13" },
  { L"search.creativecommons.org",
    L"http://search.creativecommons.org/?q={searchTerms}" },
  { L"search.ebay.com",
    L"http://search.ebay.com/search/search.dll?query={searchTerms}&"
    L"MfcISAPICommand=GetResult&ht=1&ebaytag1=ebayreg&srchdesc=n&"
    L"maxRecordsReturned=300&maxRecordsPerPage=50&SortProperty=MetaEndSort" },
  { L"google.com",
    L"http://www.google.com/search?q={searchTerms}&ie=utf-8&oe=utf-8&aq=t" },
  { L"en.wikipedia.org",
    L"http://en.wikipedia.org/wiki/Special:Search?search={searchTerms}" },
  { L"search.yahoo.com",
    L"http://search.yahoo.com/search?p={searchTerms}&ei=UTF-8" },
  { L"flickr.com",
    L"http://www.flickr.com/photos/tags/?q={searchTerms}" },
  { L"imdb.com",
    L"http://www.imdb.com/find?q={searchTerms}" },
  { L"webster.com",
    L"http://www.webster.com/cgi-bin/dictionary?va={searchTerms}" },
  // Search keywords.
  { L"\x4E2D\x6587", L"http://www.google.com/" },
};

static const int kDefaultFirefox3KeywordIndex = 8;

class Firefox3Observer : public ProfileWriter,
                         public ImporterHost::Observer {
 public:
  Firefox3Observer() : ProfileWriter(NULL) {
    bookmark_count_ = 0;
    history_count_ = 0;
    password_count_ = 0;
    keyword_count_ = 0;
  }

  virtual void ImportItemStarted(ImportItem item) {}
  virtual void ImportItemEnded(ImportItem item) {}
  virtual void ImportStarted() {}
  virtual void ImportEnded() {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kFirefox3Bookmarks), bookmark_count_);
    EXPECT_EQ(1, history_count_);
    EXPECT_EQ(arraysize(kFirefox3Passwords), password_count_);
    EXPECT_EQ(arraysize(kFirefox3Keywords), keyword_count_);
    EXPECT_EQ(kFirefox3Keywords[kDefaultFirefox3KeywordIndex].keyword,
              default_keyword_);
    EXPECT_EQ(kFirefox3Keywords[kDefaultFirefox3KeywordIndex].url,
              default_keyword_url_);
  }

  virtual bool BookmarkModelIsLoaded() const {
    // Profile is ready for writing.
    return true;
  }

  virtual void AddBookmarkModelObserver(BookmarkModelObserver* observer) {
    NOTREACHED();
  }

  virtual bool TemplateURLModelIsLoaded() const {
    return true;
  }

  virtual void AddTemplateURLModelObserver(NotificationObserver* observer) {
    NOTREACHED();
  }

  virtual void AddPasswordForm(const PasswordForm& form) {
    PasswordList p = kFirefox3Passwords[password_count_];
    EXPECT_EQ(p.origin, form.origin.spec());
    EXPECT_EQ(p.realm, form.signon_realm);
    EXPECT_EQ(p.action, form.action.spec());
    EXPECT_EQ(p.username_element, form.username_element);
    EXPECT_EQ(p.username, form.username_value);
    EXPECT_EQ(p.password_element, form.password_element);
    EXPECT_EQ(p.password, form.password_value);
    EXPECT_EQ(p.blacklisted, form.blacklisted_by_user);
    ++password_count_;
  }

  virtual void AddHistoryPage(const std::vector<history::URLRow>& page) {
    ASSERT_EQ(3, page.size());
    EXPECT_EQ("http://www.google.com/", page[0].url().spec());
    EXPECT_EQ(L"Google", page[0].title());
    EXPECT_EQ("http://www.google.com/", page[1].url().spec());
    EXPECT_EQ(L"Google", page[1].title());
    EXPECT_EQ("http://www.cs.unc.edu/~jbs/resources/perl/perl-cgi/programs/form1-POST.html",
              page[2].url().spec());
    EXPECT_EQ(L"example form (POST)", page[2].title());
    ++history_count_;
  }

  virtual void AddBookmarkEntry(const std::vector<BookmarkEntry>& bookmark,
                                const std::wstring& first_folder_name,
                                int options) {
    for (size_t i = 0; i < bookmark.size(); ++i) {
      if (FindBookmarkEntry(bookmark[i], kFirefox3Bookmarks,
                            arraysize(kFirefox3Bookmarks)))
        ++bookmark_count_;
    }
  }

  void AddKeywords(const std::vector<TemplateURL*>& template_urls,
                  int default_keyword_index,
                  bool unique_on_host_and_path) {
    for (size_t i = 0; i < template_urls.size(); ++i) {
      // The order might not be deterministic, look in the expected list for
      // that template URL.
      bool found = false;
      std::wstring keyword = template_urls[i]->keyword();
      for (int j = 0; j < arraysize(kFirefox3Keywords); ++j) {
        const wchar_t* comp_keyword = kFirefox3Keywords[j].keyword;
        bool equal = (keyword == comp_keyword);
        if (template_urls[i]->keyword() == kFirefox3Keywords[j].keyword) {
          EXPECT_EQ(kFirefox3Keywords[j].url, template_urls[i]->url()->url());
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
      ++keyword_count_;
    }

    if (default_keyword_index != -1) {
      EXPECT_LT(default_keyword_index, static_cast<int>(template_urls.size()));
      TemplateURL* default_turl = template_urls[default_keyword_index];
      default_keyword_ = default_turl->keyword();
      default_keyword_url_ = default_turl->url()->url();
    }

    STLDeleteContainerPointers(template_urls.begin(), template_urls.end());
  }

  void AddFavicons(const std::vector<history::ImportedFavIconUsage>& favicons) {
  }

 private:
  int bookmark_count_;
  int history_count_;
  int password_count_;
  int keyword_count_;
  std::wstring default_keyword_;
  std::wstring default_keyword_url_;
};

TEST_F(ImporterTest, Firefox3Importer) {
  std::wstring data_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  file_util::AppendToPath(&data_path, L"firefox3_profile\\*");
  ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, true));
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  file_util::AppendToPath(&data_path, L"firefox3_nss");
  ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, false));

  std::wstring search_engine_path = app_path_;
  file_util::AppendToPath(&search_engine_path, L"searchplugins");
  CreateDirectory(search_engine_path.c_str(), NULL);
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  file_util::AppendToPath(&data_path, L"firefox3_searchplugins");
  if (!file_util::PathExists(data_path)) {
    // TODO(maruel):  Create test data that we can open source!
    LOG(ERROR) << L"Missing internal test data";
    return;
  }
  ASSERT_TRUE(file_util::CopyDirectory(data_path, search_engine_path, false));

  MessageLoop* loop = MessageLoop::current();
  ProfileInfo profile_info;
  profile_info.browser_type = FIREFOX3;
  profile_info.app_path = app_path_;
  profile_info.source_path = profile_path_;
  scoped_refptr<ImporterHost> host = new ImporterHost(loop);
  Firefox3Observer* observer = new Firefox3Observer();
  host->SetObserver(observer);
  loop->PostTask(FROM_HERE, NewRunnableMethod(host.get(),
      &ImporterHost::StartImportSettings, profile_info,
      HISTORY | PASSWORDS | FAVORITES | SEARCH_ENGINES, observer, true));
  loop->Run();
}
