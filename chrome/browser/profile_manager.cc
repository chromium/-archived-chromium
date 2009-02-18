// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/profile_manager.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_tracker.h"

#include "generated_resources.h"

#if defined(OS_POSIX)
// TODO(port): get rid of this include. It's used just to provide declarations
// and stub definitions for classes we encouter during the porting effort.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

// static
void ProfileManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kProfileName, L"");
  prefs->RegisterStringPref(prefs::kProfileNickname, L"");
  prefs->RegisterStringPref(prefs::kProfileID, L"");
}

// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  for (ProfileManager::const_iterator i = pm->begin(); i != pm->end(); ++i)
    (*i)->ShutdownSessionService();
}

ProfileManager::ProfileManager() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->AddObserver(this);
}

ProfileManager::~ProfileManager() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->RemoveObserver(this);

  // Destroy all profiles that we're keeping track of.
  for (ProfileVector::const_iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    delete *iter;
  }
  profiles_.clear();

  // Get rid of available profile list
  for (AvailableProfileVector::const_iterator iter =
           available_profiles_.begin();
       iter != available_profiles_.end(); ++iter) {
    delete *iter;
  }
  available_profiles_.clear();
}

FilePath ProfileManager::GetDefaultProfileDir(
    const FilePath& user_data_dir) {
  FilePath default_profile_dir(user_data_dir);
  default_profile_dir = default_profile_dir.Append(
      FilePath::FromWStringHack(chrome::kNotSignedInProfile));
  return default_profile_dir;
}

FilePath ProfileManager::GetDefaultProfilePath(
    const FilePath &profile_dir) {
  FilePath default_prefs_path(profile_dir);
  default_prefs_path = default_prefs_path.Append(chrome::kPreferencesFilename);
  return default_prefs_path;
}

Profile* ProfileManager::GetDefaultProfile(const FilePath& user_data_dir) {
  // Initialize profile, creating default if necessary
  FilePath default_profile_dir = GetDefaultProfileDir(user_data_dir);
  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(default_profile_dir);
  if (NULL != profile)
    return profile;

  if (!ProfileManager::IsProfile(default_profile_dir)) {
    // If the profile directory doesn't exist, create it.
    profile = ProfileManager::CreateProfile(default_profile_dir,
        L"",  // No name.
        L"",  // No nickname.
        chrome::kNotSignedInID);
    if (!profile)
      return NULL;
    bool result = AddProfile(profile);
    DCHECK(result);
  } else {
    // The profile already exists on disk, just load it.
    profile = AddProfileByPath(default_profile_dir);
    if (!profile)
      return NULL;

    if (profile->GetID() != chrome::kNotSignedInID) {
      // Something must've gone wrong with the profile section of the
      // Preferences file, fix it.
      profile->SetID(chrome::kNotSignedInID);
      profile->SetName(chrome::kNotSignedInProfile);
    }
  }
  DCHECK(profile);
  return profile;
}

Profile* ProfileManager::AddProfileByPath(const FilePath& path) {
  Profile* profile = GetProfileByPath(path);
  if (profile)
    return profile;

  profile = Profile::CreateProfile(path);
  if (AddProfile(profile)) {
    return profile;
  } else {
    return NULL;
  }
}

void ProfileManager::NewWindowWithProfile(Profile* profile) {
  DCHECK(profile);
  Browser* browser = Browser::Create(profile);
  browser->AddTabWithURL(GURL(), GURL(), PageTransition::TYPED, true, NULL);
  browser->window()->Show();
}

Profile* ProfileManager::AddProfileByID(const std::wstring& id) {
  AvailableProfile* available = GetAvailableProfileByID(id);
  if (!available)
    return NULL;

  FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.Append(available->directory());

  return AddProfileByPath(path);
}

AvailableProfile* ProfileManager::GetAvailableProfileByID(
    const std::wstring& id) {
  AvailableProfileVector::const_iterator iter = available_profiles_.begin();
  for (; iter != available_profiles_.end(); ++iter) {
    if ((*iter)->id() == id) {
      return *iter;
    }
  }

  return NULL;
}

bool ProfileManager::AddProfile(Profile* profile) {
  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPath(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path (" <<
                    profile->GetPath().value() <<
                    ") as an already-loaded profile.";
    return false;
  }
  if (GetProfileByID(profile->GetID())) {
    NOTREACHED() << "Attempted to add profile with the same ID (" <<
                    profile->GetID() << ") as an already-loaded profile.";
    return false;
  }

  profiles_.insert(profiles_.end(), profile);
  return true;
}

void ProfileManager::RemoveProfile(Profile* profile) {
  for (ProfileVector::iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    if ((*iter) == profile) {
      profiles_.erase(iter);
      return;
    }
  }
}

void ProfileManager::RemoveProfileByPath(const FilePath& path) {
  for (ProfileVector::iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    if ((*iter)->GetPath() == path) {
      delete *iter;
      profiles_.erase(iter);
      return;
    }
  }

  NOTREACHED() << "Attempted to remove non-loaded profile: " << path.value();
}

Profile* ProfileManager::GetProfileByPath(const FilePath& path) const {
  for (ProfileVector::const_iterator iter = profiles_.begin();
       iter != profiles_.end(); ++iter) {
    if ((*iter)->GetPath() == path)
      return *iter;
  }

  return NULL;
}

Profile* ProfileManager::GetProfileByID(const std::wstring& id) const {
  for (ProfileVector::const_iterator iter = profiles_.begin();
    iter != profiles_.end(); ++iter) {
      if ((*iter)->GetID() == id)
        return *iter;
  }

  return NULL;
}

void ProfileManager::OnSuspend(base::SystemMonitor* monitor) {
  DCHECK(CalledOnValidThread());

  ProfileManager::const_iterator it = begin();
  while(it != end()) {
     g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
       NewRunnableFunction(&ProfileManager::SuspendProfile, *it));
     it++;
  }
}

void ProfileManager::OnResume(base::SystemMonitor* monitor) {
  DCHECK(CalledOnValidThread());
  ProfileManager::const_iterator it = begin();
  while (it != end()) {
    g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(&ProfileManager::ResumeProfile, *it));
    it++;
  }
}

void ProfileManager::SuspendProfile(Profile* profile) {
  DCHECK(profile);
  DCHECK(MessageLoop::current() ==
    ChromeThread::GetMessageLoop(ChromeThread::IO));

  URLRequestJobTracker::JobIterator it = g_url_request_job_tracker.begin();
  for (;it != g_url_request_job_tracker.end(); ++it)
    (*it)->Kill();

  profile->GetRequestContext()->http_transaction_factory()->Suspend(true);
}

void ProfileManager::ResumeProfile(Profile* profile) {
  DCHECK(profile);
  DCHECK(MessageLoop::current() ==
    ChromeThread::GetMessageLoop(ChromeThread::IO));
  profile->GetRequestContext()->http_transaction_factory()->Suspend(false);
}



// static
bool ProfileManager::IsProfile(const FilePath& path) {
  FilePath prefs_path = GetDefaultProfilePath(path);

  FilePath history_path = path;
  history_path = history_path.Append(chrome::kHistoryFilename);

  return file_util::PathExists(prefs_path) &&
         file_util::PathExists(history_path);
}

// static
bool ProfileManager::CopyProfileData(const FilePath& source_path,
                                     const FilePath& destination_path) {
  // create destination directory if necessary
  if (!file_util::PathExists(destination_path)) {
    bool result = file_util::CreateDirectory(destination_path);
    if (!result) {
      DLOG(WARNING) << "Unable to create destination directory " <<
                       destination_path.value();
      return false;
    }
  }

  // copy files in directory
  return file_util::CopyDirectory(source_path, destination_path, false);
}

// static
Profile* ProfileManager::CreateProfile(const FilePath& path,
                                       const std::wstring& name,
                                       const std::wstring& nickname,
                                       const std::wstring& id) {
  DCHECK_LE(nickname.length(), name.length());

  if (IsProfile(path)) {
    DCHECK(false) << "Attempted to create a profile with the path:\n"
        << path.value() << "\n but that path already contains a profile";
  }

  if (!file_util::PathExists(path)) {
    // TODO(tc): http://b/1094718 Bad things happen if we can't write to the
    // profile directory.  We should eventually be able to run in this
    // situation.
    if (!file_util::CreateDirectory(path))
      return NULL;
  }

  Profile* profile = Profile::CreateProfile(path);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kProfileName, name);
  prefs->SetString(prefs::kProfileNickname, nickname);
  prefs->SetString(prefs::kProfileID, id);

  return profile;
}

// static
std::wstring ProfileManager::CanonicalizeID(const std::wstring& id) {
  std::wstring no_whitespace;
  TrimWhitespace(id, TRIM_ALL, &no_whitespace);
  return StringToLowerASCII(no_whitespace);
}

