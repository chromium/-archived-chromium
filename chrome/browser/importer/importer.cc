// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer.h"

#include <map>
#include <set>

#include "base/file_util.h"
#include "base/gfx/png_encoder.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/importer/firefox2_importer.h"
#include "chrome/browser/importer/firefox3_importer.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/firefox_profile_lock.h"
#include "chrome/browser/importer/ie_importer.h"
#include "chrome/browser/importer/toolbar_importer.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/views/importer_lock_view.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/gfx/favicon_size.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/win_util.h"
#include "chrome/views/window.h"
#include "skia/ext/image_operations.h"
#include "webkit/glue/image_decoder.h"

#include "generated_resources.h"

// ProfileWriter.

bool ProfileWriter::BookmarkModelIsLoaded() const {
  return profile_->GetBookmarkModel()->IsLoaded();
}

void ProfileWriter::AddBookmarkModelObserver(BookmarkModelObserver* observer) {
  profile_->GetBookmarkModel()->AddObserver(observer);
}

bool ProfileWriter::TemplateURLModelIsLoaded() const {
  return profile_->GetTemplateURLModel()->loaded();
}

void ProfileWriter::AddTemplateURLModelObserver(
    NotificationObserver* observer) {
  TemplateURLModel* model = profile_->GetTemplateURLModel();
  NotificationService::current()->AddObserver(
      observer, TEMPLATE_URL_MODEL_LOADED,
      Source<TemplateURLModel>(model));
  model->Load();
}

void ProfileWriter::AddPasswordForm(const PasswordForm& form) {
  profile_->GetWebDataService(Profile::EXPLICIT_ACCESS)->AddLogin(form);
}

void ProfileWriter::AddIE7PasswordInfo(const IE7PasswordInfo& info) {
  profile_->GetWebDataService(Profile::EXPLICIT_ACCESS)->AddIE7Login(info);
}

void ProfileWriter::AddHistoryPage(const std::vector<history::URLRow>& page) {
  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->
      AddPagesWithDetails(page);
}

void ProfileWriter::AddHomepage(const GURL& home_page) {
  DCHECK(profile_);

  PrefService* prefs = profile_->GetPrefs();
  // NOTE: We set the kHomePage value, but keep the NewTab page as the homepage.
  prefs->SetString(prefs::kHomePage, ASCIIToWide(home_page.spec()));
  prefs->ScheduleSavePersistentPrefs(g_browser_process->file_thread());
}

void ProfileWriter::AddBookmarkEntry(
    const std::vector<BookmarkEntry>& bookmark,
    const std::wstring& first_folder_name,
    int options) {
  BookmarkModel* model = profile_->GetBookmarkModel();
  DCHECK(model->IsLoaded());

  bool first_run = (options & FIRST_RUN) != 0;
  std::wstring real_first_folder = first_run ? first_folder_name :
      GenerateUniqueFolderName(model, first_folder_name);

  bool show_bookmark_toolbar = false;
  std::set<BookmarkNode*> groups_added_to;
  for (std::vector<BookmarkEntry>::const_iterator it = bookmark.begin();
       it != bookmark.end(); ++it) {
    // Don't insert this url if it isn't valid.
    if (!it->url.is_valid())
      continue;

    // We suppose that bookmarks are unique by Title, URL, and Folder.  Since
    // checking for uniqueness may not be always the user's intention we have
    // this as an option.
    if (options & ADD_IF_UNIQUE &&
        DoesBookmarkExist(model, *it, real_first_folder, first_run)) {
      continue;
    }

    // Set up groups in BookmarkModel in such a way that path[i] is
    // the subgroup of path[i-1]. Finally they construct a path in the
    // model:
    //   path[0] \ path[1] \ ... \ path[size() - 1]
    BookmarkNode* parent =
        (it->in_toolbar ? model->GetBookmarkBarNode() : model->other_node());
    for (std::vector<std::wstring>::const_iterator i = it->path.begin();
         i != it->path.end(); ++i) {
      BookmarkNode* child = NULL;
      const std::wstring& folder_name =
          (!first_run && !it->in_toolbar && (i == it->path.begin())) ?
          real_first_folder : *i;

      for (int index = 0; index < parent->GetChildCount(); ++index) {
        BookmarkNode* node = parent->GetChild(index);
        if ((node->GetType() == history::StarredEntry::BOOKMARK_BAR ||
             node->GetType() == history::StarredEntry::USER_GROUP) &&
            node->GetTitle() == folder_name) {
          child = node;
          break;
        }
      }
      if (child == NULL)
        child = model->AddGroup(parent, parent->GetChildCount(), folder_name);
      parent = child;
    }
    groups_added_to.insert(parent);
    model->AddURLWithCreationTime(parent, parent->GetChildCount(),
        it->title, it->url, it->creation_time);

    // If some items are put into toolbar, it looks like the user was using
    // it in their last browser. We turn on the bookmarks toolbar.
    if (it->in_toolbar)
      show_bookmark_toolbar = true;
  }

  // Reset the date modified time of the groups we added to. We do this to
  // make sure the 'recently added to' combobox in the bubble doesn't get random
  // groups.
  for (std::set<BookmarkNode*>::const_iterator i = groups_added_to.begin();
       i != groups_added_to.end(); ++i) {
    model->ResetDateGroupModified(*i);
  }

  if (show_bookmark_toolbar)
    ShowBookmarkBar();
}

void ProfileWriter::AddFavicons(
    const std::vector<history::ImportedFavIconUsage>& favicons) {
  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->
      SetImportedFavicons(favicons);
}

typedef std::map<std::string, const TemplateURL*> HostPathMap;

// Returns the key for the map built by BuildHostPathMap. If url_string is not
// a valid URL, an empty string is returned, otherwise host+path is returned.
static std::string HostPathKeyForURL(const GURL& url) {
  return url.is_valid() ? url.host() + url.path() : std::string();
}

// Builds the key to use in HostPathMap for the specified TemplateURL. Returns
// an empty string if a host+path can't be generated for the TemplateURL.
// If an empty string is returned, the TemplateURL should not be added to
// HostPathMap.
//
// If |try_url_if_invalid| is true, and |t_url| isn't valid, a string is built
// from the raw TemplateURL string. Use a value of true for |try_url_if_invalid|
// when checking imported URLs as the imported URL may not be valid yet may
// match the host+path of one of the default URLs. This is used to catch the
// case of IE using an invalid OSDD URL for Live Search, yet the host+path
// matches our prepopulate data. IE's URL for Live Search is something like
// 'http://...{Language}...'. As {Language} is not a valid OSDD parameter value
// the TemplateURL is invalid.
static std::string BuildHostPathKey(const TemplateURL* t_url,
                                    bool try_url_if_invalid) {
  if (t_url->url()) {
    if (try_url_if_invalid && !t_url->url()->IsValid())
      return HostPathKeyForURL(GURL(WideToUTF8(t_url->url()->url())));

    if (t_url->url()->SupportsReplacement()) {
      return HostPathKeyForURL(
          t_url->url()->ReplaceSearchTerms(
              *t_url, L"random string",
              TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::wstring()));
    }
  }
  return std::string();
}

// Builds a set that contains an entry of the host+path for each TemplateURL in
// the TemplateURLModel that has a valid search url.
static void BuildHostPathMap(const TemplateURLModel& model,
                             HostPathMap* host_path_map) {
  std::vector<const TemplateURL*> template_urls = model.GetTemplateURLs();
  for (size_t i = 0; i < template_urls.size(); ++i) {
    const std::string host_path = BuildHostPathKey(template_urls[i], false);
    if (!host_path.empty()) {
      const TemplateURL* existing_turl = (*host_path_map)[host_path];
      if (!existing_turl ||
          (template_urls[i]->show_in_default_list() &&
           !existing_turl->show_in_default_list())) {
        // If there are multiple TemplateURLs with the same host+path, favor
        // those shown in the default list.  If there are multiple potential
        // defaults, favor the first one, which should be the more commonly used
        // one.
        (*host_path_map)[host_path] = template_urls[i];
      }
    }  // else case, TemplateURL doesn't have a search url, doesn't support
       // replacement, or doesn't have valid GURL. Ignore it.
  }
}

void ProfileWriter::AddKeywords(const std::vector<TemplateURL*>& template_urls,
                                int default_keyword_index,
                                bool unique_on_host_and_path) {
  TemplateURLModel* model = profile_->GetTemplateURLModel();
  HostPathMap host_path_map;
  if (unique_on_host_and_path)
    BuildHostPathMap(*model, &host_path_map);

  for (std::vector<TemplateURL*>::const_iterator i = template_urls.begin();
       i != template_urls.end(); ++i) {
    TemplateURL* t_url = *i;
    bool default_keyword =
        default_keyword_index >= 0 &&
        (i - template_urls.begin() == default_keyword_index);

    // TemplateURLModel requires keywords to be unique. If there is already a
    // TemplateURL with this keyword, don't import it again.
    const TemplateURL* turl_with_keyword =
        model->GetTemplateURLForKeyword(t_url->keyword());
    if (turl_with_keyword != NULL) {
      if (default_keyword)
        model->SetDefaultSearchProvider(turl_with_keyword);
      delete t_url;
      continue;
    }

    // For search engines if there is already a keyword with the same
    // host+path, we don't import it. This is done to avoid both duplicate
    // search providers (such as two Googles, or two Yahoos) as well as making
    // sure the search engines we provide aren't replaced by those from the
    // imported browser.
    if (unique_on_host_and_path &&
        host_path_map.find(
            BuildHostPathKey(t_url, true)) != host_path_map.end()) {
      if (default_keyword) {
        const TemplateURL* turl_with_host_path =
            host_path_map[BuildHostPathKey(t_url, true)];
        if (turl_with_host_path)
          model->SetDefaultSearchProvider(turl_with_host_path);
        else
          NOTREACHED();  // BuildHostPathMap should only insert non-null values.
      }
      delete t_url;
      continue;
    }
    if (t_url->url() && t_url->url()->IsValid()) {
      model->Add(t_url);
      if (default_keyword && t_url->url() && 
          t_url->url()->SupportsReplacement())
        model->SetDefaultSearchProvider(t_url);
    } else {
      // Don't add invalid TemplateURLs to the model.
      delete t_url;
    }
  }
}

void ProfileWriter::ShowBookmarkBar() {
  DCHECK(profile_);

  PrefService* prefs = profile_->GetPrefs();
  // Check whether the bookmark bar is shown in current pref.
  if (!prefs->GetBoolean(prefs::kShowBookmarkBar)) {
    // Set the pref and notify the notification service.
    prefs->SetBoolean(prefs::kShowBookmarkBar, true);
    prefs->ScheduleSavePersistentPrefs(g_browser_process->file_thread());
    Source<Profile> source(profile_);
    NotificationService::current()->Notify(
        NOTIFY_BOOKMARK_BAR_VISIBILITY_PREF_CHANGED, source,
        NotificationService::NoDetails());
  }
}

std::wstring ProfileWriter::GenerateUniqueFolderName(
    BookmarkModel* model,
    const std::wstring& folder_name) {
  // Build a set containing the folder names of the other folder.
  std::set<std::wstring> other_folder_names;
  BookmarkNode* other = model->other_node();
  for (int i = 0; i < other->GetChildCount(); ++i) {
    BookmarkNode* node = other->GetChild(i);
    if (node->is_folder())
      other_folder_names.insert(node->GetTitle());
  }

  if (other_folder_names.find(folder_name) == other_folder_names.end())
    return folder_name;  // Name is unique, use it.

  // Otherwise iterate until we find a unique name.
  for (int i = 1; i < 100; ++i) {
    std::wstring name = folder_name + StringPrintf(L" (%d)", i);
    if (other_folder_names.find(name) == other_folder_names.end())
      return name;
  }

  return folder_name;
}

bool ProfileWriter::DoesBookmarkExist(
    BookmarkModel* model,
    const BookmarkEntry& entry,
    const std::wstring& first_folder_name,
    bool first_run) {
  std::vector<BookmarkNode*> nodes_with_same_url;
  model->GetNodesByURL(entry.url, &nodes_with_same_url);
  if (nodes_with_same_url.empty())
    return false;
  
  for (size_t i = 0; i < nodes_with_same_url.size(); ++i) {
    BookmarkNode* node = nodes_with_same_url[i];
    if (entry.title != node->GetTitle())
      continue;

    // Does the path match?
    bool found_match = true;
    BookmarkNode* parent = node->GetParent();
    for (std::vector<std::wstring>::const_reverse_iterator path_it =
             entry.path.rbegin();
         (path_it != entry.path.rend()) && found_match; ++path_it) {
      const std::wstring& folder_name =
          (!first_run && path_it + 1 == entry.path.rend()) ?
          first_folder_name : *path_it;
      if (NULL == parent || *path_it != folder_name)
        found_match = false;
      else
        parent = parent->GetParent();
    }

    // We need a post test to differentiate checks such as
    // /home/hello and /hello. The parent should either by the other folder
    // node, or the bookmarks bar, depending upon first_run and
    // entry.in_toolbar.
    if (found_match &&
        ((first_run && entry.in_toolbar && parent !=
          model->GetBookmarkBarNode()) ||
         ((!first_run || !entry.in_toolbar) &&
           parent != model->other_node()))) {
      found_match = false;
    }

    if (found_match)
      return true;  // Found a match with the same url path and title.
  }
  return false;
}

// Importer.

// static
bool Importer::ReencodeFavicon(const unsigned char* src_data, size_t src_len,
                               std::vector<unsigned char>* png_data) {
  // Decode the favicon using WebKit's image decoder.
  webkit_glue::ImageDecoder decoder(gfx::Size(kFavIconSize, kFavIconSize));
  SkBitmap decoded = decoder.Decode(src_data, src_len);
  if (decoded.empty())
    return false;  // Unable to decode.

  if (decoded.width() != kFavIconSize || decoded.height() != kFavIconSize) {
    // The bitmap is not the correct size, re-sample.
    int new_width = decoded.width();
    int new_height = decoded.height();
    calc_favicon_target_size(&new_width, &new_height);
    decoded = skia::ImageOperations::Resize(
        decoded, skia::ImageOperations::RESIZE_LANCZOS3, new_width, new_height);
  }

  // Encode our bitmap as a PNG.
  SkAutoLockPixels decoded_lock(decoded);
  PNGEncoder::Encode(reinterpret_cast<unsigned char*>(decoded.getPixels()),
                     PNGEncoder::FORMAT_BGRA, decoded.width(),
                     decoded.height(), decoded.width() * 4, false, png_data);
  return true;
}

// ImporterHost.

ImporterHost::ImporterHost()
    : observer_(NULL),
      task_(NULL),
      importer_(NULL),
      file_loop_(g_browser_process->file_thread()->message_loop()),
      waiting_for_bookmarkbar_model_(false),
      waiting_for_template_url_model_(false),
      is_source_readable_(true),
      headless_(false) {
  DetectSourceProfiles();
}

ImporterHost::ImporterHost(MessageLoop* file_loop)
    : observer_(NULL),
      task_(NULL),
      importer_(NULL),
      file_loop_(file_loop),
      waiting_for_bookmarkbar_model_(false),
      waiting_for_template_url_model_(false),
      is_source_readable_(true),
      headless_(false) {
  DetectSourceProfiles();
}

ImporterHost::~ImporterHost() {
  STLDeleteContainerPointers(source_profiles_.begin(), source_profiles_.end());
  if (NULL != importer_)  importer_->Release();
}

void ImporterHost::Loaded(BookmarkModel* model) {
  model->RemoveObserver(this);
  waiting_for_bookmarkbar_model_ = false;
  InvokeTaskIfDone();
}

void ImporterHost::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK(type == TEMPLATE_URL_MODEL_LOADED);
  TemplateURLModel* model = Source<TemplateURLModel>(source).ptr();
  NotificationService::current()->RemoveObserver(
      this, TEMPLATE_URL_MODEL_LOADED,
      Source<TemplateURLModel>(model));
  waiting_for_template_url_model_ = false;
  InvokeTaskIfDone();
}

void ImporterHost::ShowWarningDialog() {
  if (headless_)
    OnLockViewEnd(false);
  else
    views::Window::CreateChromeWindow(GetActiveWindow(), gfx::Rect(),
                                      new ImporterLockView(this))->Show();
}

void ImporterHost::OnLockViewEnd(bool is_continue) {
  if (is_continue) {
    // User chose to continue, then we check the lock again to make
    // sure that Firefox has been closed. Try to import the settings
    // if successful. Otherwise, show a warning dialog.
    firefox_lock_->Lock();
    if (firefox_lock_->HasAcquired()) {
      is_source_readable_ = true;
      InvokeTaskIfDone();
    } else {
      ShowWarningDialog();
    }
  } else {
    // User chose to skip the import process. We should delete
    // the task and notify the ImporterHost to finish.
    delete task_;
    task_ = NULL;
    importer_ = NULL;
    ImportEnded();
  }
}

void ImporterHost::StartImportSettings(const ProfileInfo& profile_info,
                                       uint16 items,
                                       ProfileWriter* writer,
                                       bool first_run) {
  // Preserves the observer and creates a task, since we do async import
  // so that it doesn't block the UI. When the import is complete, observer
  // will be notified.
  writer_ = writer;
  importer_ = CreateImporterByType(profile_info.browser_type);
  importer_->AddRef();
  importer_->set_first_run(first_run);
  task_ = NewRunnableMethod(importer_, &Importer::StartImport,
      profile_info, items, writer_.get(), file_loop_, this);

  // We should lock the Firefox profile directory to prevent corruption.
  if (profile_info.browser_type == FIREFOX2 ||
      profile_info.browser_type == FIREFOX3) {
    firefox_lock_.reset(new FirefoxProfileLock(profile_info.source_path));
    if (!firefox_lock_->HasAcquired()) {
      // If fail to acquire the lock, we set the source unreadable and
      // show a warning dialog.
      is_source_readable_ = false;
      ShowWarningDialog();
    }
  }

  if (profile_info.browser_type == GOOGLE_TOOLBAR5) {
    if (!ToolbarImporterUtils::IsGoogleGAIACookieInstalled()) {
      win_util::MessageBox(
          NULL,
          l10n_util::GetString(IDS_IMPORTER_GOOGLE_LOGIN_TEXT).c_str(),
          L"",
          MB_OK | MB_TOPMOST);

      GURL url("https://www.google.com/accounts/ServiceLogin");
      BrowsingInstance* instance = new BrowsingInstance(writer_->GetProfile());
      SiteInstance* site = instance->GetSiteInstanceForURL(url);
      Browser* browser = BrowserList::GetLastActive();
      browser->AddTabWithURL(url, GURL(), PageTransition::TYPED, true, site);

      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ImporterHost::OnLockViewEnd, false));

      is_source_readable_ = false;
    }
  }

  // BookmarkModel should be loaded before adding IE favorites. So we observe
  // the BookmarkModel if needed, and start the task after it has been loaded.
  if ((items & FAVORITES) && !writer_->BookmarkModelIsLoaded()) {
    writer_->AddBookmarkModelObserver(this);
    waiting_for_bookmarkbar_model_ = true;
  }

  // Observes the TemplateURLModel if needed to import search engines from the
  // other browser. We also check to see if we're importing bookmarks because
  // we can import bookmark keywords from Firefox as search engines.
  if ((items & SEARCH_ENGINES) || (items & FAVORITES)) {
    if (!writer_->TemplateURLModelIsLoaded()) {
      writer_->AddTemplateURLModelObserver(this);
      waiting_for_template_url_model_ = true;
    }
  }

  AddRef();
  InvokeTaskIfDone();
}

void ImporterHost::Cancel() {
  if (importer_)
    importer_->Cancel();
}

void ImporterHost::SetObserver(Observer* observer) {
  observer_ = observer;
}

void ImporterHost::InvokeTaskIfDone() {
  if (waiting_for_bookmarkbar_model_ || waiting_for_template_url_model_ ||
      !is_source_readable_)
    return;
  file_loop_->PostTask(FROM_HERE, task_);
}

void ImporterHost::ImportItemStarted(ImportItem item) {
  if (observer_)
    observer_->ImportItemStarted(item);
}

void ImporterHost::ImportItemEnded(ImportItem item) {
  if (observer_)
    observer_->ImportItemEnded(item);
}

void ImporterHost::ImportStarted() {
  if (observer_)
    observer_->ImportStarted();
}

void ImporterHost::ImportEnded() {
  firefox_lock_.reset();  // Release the Firefox profile lock.
  if (observer_)
    observer_->ImportEnded();
  Release();
}

Importer* ImporterHost::CreateImporterByType(ProfileType type) {
  switch (type) {
    case MS_IE:
      return new IEImporter();
    case BOOKMARKS_HTML:
    case FIREFOX2:
      return new Firefox2Importer();
    case FIREFOX3:
      return new Firefox3Importer();
    case GOOGLE_TOOLBAR5:
      return new Toolbar5Importer();
  }
  NOTREACHED();
  return NULL;
}

int ImporterHost::GetAvailableProfileCount() {
  return static_cast<int>(source_profiles_.size());
}

std::wstring ImporterHost::GetSourceProfileNameAt(int index) const {
  DCHECK(index < static_cast<int>(source_profiles_.size()));
  return source_profiles_[index]->description;
}

const ProfileInfo& ImporterHost::GetSourceProfileInfoAt(int index) const {
  DCHECK(index < static_cast<int>(source_profiles_.size()));
  return *source_profiles_[index];
}

void ImporterHost::DetectSourceProfiles() {
  // The order in which detect is called determines the order
  // in which the options appear in the dropdown combo-box
  if (ShellIntegration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles();
    DetectIEProfiles();
  } else {
    DetectIEProfiles();
    DetectFirefoxProfiles();
  }

  if (!FirstRun::IsChromeFirstRun()) DetectGoogleToolbarProfiles();
}

void ImporterHost::DetectIEProfiles() {
  // IE always exists and don't have multiple profiles.
  ProfileInfo* ie = new ProfileInfo();
  ie->description = l10n_util::GetString(IDS_IMPORT_FROM_IE);
  ie->browser_type = MS_IE;
  ie->source_path.clear();
  ie->app_path.clear();
  ie->services_supported = HISTORY | FAVORITES | COOKIES | PASSWORDS |
      SEARCH_ENGINES;
  source_profiles_.push_back(ie);
}

void ImporterHost::DetectFirefoxProfiles() {
  // Detects which version of Firefox is installed.
  int version = GetCurrentFirefoxMajorVersion();
  ProfileType firefox_type;
  if (version == 2) {
    firefox_type = FIREFOX2;
  } else if (version == 3) {
    firefox_type = FIREFOX3;
  } else {
    // Ignores other versions of firefox.
    return;
  }

  std::wstring ini_file = GetProfilesINI();
  DictionaryValue root;
  ParseProfileINI(ini_file, &root);

  std::wstring source_path;
  for (int i = 0; ; ++i) {
    std::wstring current_profile = L"Profile" + IntToWString(i);
    if (!root.HasKey(current_profile)) {
      // Profiles are continuously numbered. So we exit when we can't
      // find the i-th one.
      break;
    }
    std::wstring is_relative, path, profile_path;
    if (root.GetString(current_profile + L".IsRelative", &is_relative) &&
        root.GetString(current_profile + L".Path", &path)) {
      ReplaceSubstringsAfterOffset(&path, 0, L"/", L"\\");

      // IsRelative=1 means the folder path would be relative to the
      // path of profiles.ini. IsRelative=0 refers to a custom profile
      // location.
      if (is_relative == L"1") {
        profile_path = file_util::GetDirectoryFromPath(ini_file);
        file_util::AppendToPath(&profile_path, path);
      } else {
        profile_path = path;
      }

      // We only import the default profile when multiple profiles exist,
      // since the other profiles are used mostly by developers for testing.
      // Otherwise, Profile0 will be imported.
      std::wstring is_default;
      if ((root.GetString(current_profile + L".Default", &is_default) &&
           is_default == L"1") || i == 0) {
        source_path = profile_path;
        // We break out of the loop when we have found the default profile.
        if (is_default == L"1")
          break;
      }
    }
  }

  if (!source_path.empty()) {
    ProfileInfo* firefox = new ProfileInfo();
    firefox->description = l10n_util::GetString(IDS_IMPORT_FROM_FIREFOX);
    firefox->browser_type = firefox_type;
    firefox->source_path = source_path;
    firefox->app_path = GetFirefoxInstallPath();
    firefox->services_supported = HISTORY | FAVORITES | COOKIES | PASSWORDS |
        SEARCH_ENGINES;
    source_profiles_.push_back(firefox);
  }
}

void ImporterHost::DetectGoogleToolbarProfiles() {
  if (!FirstRun::IsChromeFirstRun()) {
    ProfileInfo* google_toolbar = new ProfileInfo();
    google_toolbar->browser_type = GOOGLE_TOOLBAR5;
    google_toolbar->description = l10n_util::GetString(
                                  IDS_IMPORT_FROM_GOOGLE_TOOLBAR);
    google_toolbar->source_path.clear();
    google_toolbar->app_path.clear();
    google_toolbar->services_supported = FAVORITES;
    source_profiles_.push_back(google_toolbar);
  }
}
