// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class keeps track of the currently-active profiles in the runtime.

#ifndef CHROME_BROWSER_PROFILE_MANAGER_H__
#define CHROME_BROWSER_PROFILE_MANAGER_H__

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/profile.h"

// This is a small storage class that simply represents some metadata about
// profiles that are available in the current user data directory.
// These are cached in local state so profiles don't need to be scanned
// for their metadata on every launch.
class AvailableProfile {
 public:
  AvailableProfile(const std::wstring& name,
                   const std::wstring& id,
                   const std::wstring& directory)
      : name_(name), id_(id), directory_(directory) {}

  // Decodes a DictionaryValue into an AvailableProfile
  static AvailableProfile* FromValue(DictionaryValue* value) {
    DCHECK(value);
    std::wstring name, id, directory;
    value->GetString(L"name", &name);
    value->GetString(L"id", &id);
    value->GetString(L"directory", &directory);
    return new AvailableProfile(name, id, directory);
  }

  // Encodes this AvailableProfile into a new DictionaryValue
  DictionaryValue* ToValue() {
    DictionaryValue* value = new DictionaryValue;
    value->SetString(L"name", name_);
    value->SetString(L"id", id_);
    value->SetString(L"directory", directory_);
    return value;
  }

  std::wstring name() const { return name_; }
  std::wstring id() const { return id_; }
  std::wstring directory() const { return directory_; }

 private:
  std::wstring name_;       // User-visible profile name
  std::wstring id_;         // Profile identifier
  std::wstring directory_;  // Subdirectory containing profile (not full path)

  DISALLOW_EVIL_CONSTRUCTORS(AvailableProfile);
};

class ProfileManager {
 public:
  ProfileManager();
  virtual ~ProfileManager();

  // ProfileManager prefs are loaded as soon as the profile is created.
  static void RegisterUserPrefs(PrefService* prefs);

  // Invokes ShutdownSessionService() on all profiles.
  static void ShutdownSessionServices();

  // Returns the default profile.  This adds the profile to the
  // ProfileManager if it doesn't already exist.  This method returns NULL if
  // the profile doesn't exist and we can't create it.
  Profile* GetDefaultProfile(const std::wstring& user_data_dir);

  // If a profile with the given path is currently managed by this object,
  // return a pointer to the corresponding Profile object;
  // otherwise return NULL.
  Profile* GetProfileByPath(const std::wstring& path) const;

  // If a profile with the given ID is currently managed by this object,
  // return a pointer to the corresponding Profile object;
  // otherwise returns NULL.
  Profile* GetProfileByID(const std::wstring& id) const;

  // Adds a profile to the set of currently-loaded profiles.  Returns a
  // pointer to a Profile object corresponding to the given path.
  Profile* AddProfileByPath(const std::wstring& path);

  // Adds a profile to the set of currently-loaded profiles.  Returns a
  // pointer to a Profile object corresponding to the given profile ID.
  // If no profile with the given ID is known, returns NULL.
  Profile* AddProfileByID(const std::wstring& id);

  // Adds a pre-existing Profile object to the set managed by this
  // ProfileManager.  This ProfileManager takes ownership of the Profile.
  // The Profile should not already be managed by this ProfileManager.
  // Returns true if the profile was added, false otherwise.
  bool AddProfile(Profile* profile);

  // Removes a profile from the set of currently-loaded profiles. The path must
  // be exactly the same (including case) as when GetProfileByPath was called.
  void RemoveProfileByPath(const std::wstring& path);

  // Removes a profile from the set of currently-loaded profiles.
  // (Does not delete the profile object.)
  void RemoveProfile(Profile* profile);

  // These allow iteration through the current list of profiles.
  typedef std::vector<Profile*> ProfileVector;
  typedef ProfileVector::iterator iterator;
  typedef ProfileVector::const_iterator const_iterator;

  const_iterator begin() { return profiles_.begin(); }
  const_iterator end() { return profiles_.end(); }

  typedef std::vector<AvailableProfile*> AvailableProfileVector;
  const AvailableProfileVector& available_profiles() const {
    return available_profiles_;
  }

  // Creates a new window with the given profile.
  void NewWindowWithProfile(Profile* profile);

  // ------------------ static utility functions -------------------

  // Returns the path to the profile directory based on the user data directory.
  static std::wstring GetDefaultProfileDir(const std::wstring& user_data_dir);

  // Returns the path to the profile given the user profile directory.
  static std::wstring GetDefaultProfilePath(const std::wstring& profile_dir);

  // Tries to determine whether the given path represents a profile
  // directory, and returns true if it thinks it does.
  static bool IsProfile(const std::wstring& path);

  // Tries to copy profile data from the source path to the destination path,
  // returning true if successful.
  static bool CopyProfileData(const std::wstring& source_path,
                              const std::wstring& destination_path);

  // Creates a new profile at the specified path with the given name and ID.
  // |name| is the full-length human-readable name for the profile
  // |nickname| is a shorter name for the profile--can be empty string
  // This method should always return a valid Profile (i.e., should never
  // return NULL).
  static Profile* CreateProfile(const std::wstring& path,
                                const std::wstring& name,
                                const std::wstring& nickname,
                                const std::wstring& id);

  // Returns the canonical form of the given ID string.
  static std::wstring CanonicalizeID(const std::wstring& id);

 private:
  // Returns the AvailableProfile entry associated with the given ID,
  // or NULL if no match is found.
  AvailableProfile* GetAvailableProfileByID(const std::wstring& id);

  // We keep a simple vector of profiles rather than something fancier
  // because we expect there to be a small number of profiles active.
  ProfileVector profiles_;

  AvailableProfileVector available_profiles_;

  DISALLOW_EVIL_CONSTRUCTORS(ProfileManager);
};

#endif  // CHROME_BROWSER_PROFILE_MANAGER_H__

