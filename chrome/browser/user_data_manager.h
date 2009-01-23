// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class encapsulates the implementation of multiple profiles by using
// the user-data-dir functionality.

#ifndef CHROME_BROWSER_USER_DATA_MANAGER_H_
#define CHROME_BROWSER_USER_DATA_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"

class MessageLoop;

// Provides an abstraction of profiles on top of the user data directory
// feature. Given the root of the user data directories, it provides
// functionality to enumerate the existing profiles and start Chrome in a
// given profile.
// Also holds a shared instance of its own for convenience though it's not a
// singleton class. The shared instance should be created by the main thread,
// then other threads can access and use the shared instance.
class UserDataManager {
 public:
  // Creates the shared instance of this class. This method is not thread-safe,
  // so the shared instance should be created on the main thread.
  static void Create();

  // Returns the shared instance. CreateInstance must be called before callling
  // this method.
  static UserDataManager* Get();

  // Creates a new instance with the given root folder for storing user data
  // folders.
  explicit UserDataManager(const std::wstring& user_data_root);

  ~UserDataManager();

  // Returns the name of the current profile.
  std::wstring current_profile_name() const { return current_profile_name_; }

  // Returns whether the current profile is the default profile or not.
  bool is_current_profile_default() const {
    return is_current_profile_default_;
  }

  // Populates the given vector with a list of all the profiles.
  // This function should be called on the file thread.
  void GetProfiles(std::vector<std::wstring>* profiles) const;

  // Creates a desktop shortcut for the given profile name.
  // Returns false if the shortcut creation fails; true otherwise.
  bool CreateDesktopShortcutForProfile(const std::wstring& profile_name) const;

  // Starts a new Chrome instance in the given profile name.
  void LaunchChromeForProfile(const std::wstring& profile_name) const;

  // Starts a new Chrome instance in the profile with the given index. The
  // index is zero based, and refers to the position of the profile in the
  // list of profile names in alphabetical order.
  // This method launches Chrome asynchornously since it enumerates profiles
  // on a separate thread.
  void LaunchChromeForProfile(int index) const;

 private:
  // Gets the name of the profile from the name of the folder.
  // Returns false if the folder does not correspond to a profile folder, true
  // otherwise.
  static bool GetProfileNameFromFolderName(const std::wstring& folder_name,
                                           std::wstring* profile_name);

  // Returns the name of the folder from the name of the profile.
  static std::wstring GetFolderNameFromProfileName(
      const std::wstring& profile_name);

  // Returns the path of the user data folder for the given profile.
  std::wstring GetUserDataFolderForProfile(
      const std::wstring& profile_name) const;

  // Returns the command to start the app in the given profile.
  std::wstring GetCommandForProfile(const std::wstring& profile_name) const;

  // Shared instance.
  static UserDataManager* instance_;

  // Root folder.
  std::wstring user_data_root_;

  // Current user data folder.
  std::wstring current_folder_name_;

  // Whether the current profile is the default profile.
  bool is_current_profile_default_;

  // Current profile name.
  std::wstring current_profile_name_;

  DISALLOW_COPY_AND_ASSIGN(UserDataManager);
};

// Helper class to enumerate the profiles asynchronously on the file thread.
// It calls the given delegate instance when the enumeration is complete.
// USAGE: Create an instance of the helper with a delegate instance, call the
// asynchronous method GetProfiles. The delegate instance will be called when
// enumerating profiles is done.
// IMPORTANT: It's the responsibility of the caller to call OnDelegateDeleted
// method when the delegate instance is deleted. Typically OnDelegateDeleted
// should be called in the destructor of the delegate. This is the way to
// tell the helper to not call the delegate when enumerating profiles is done.
class GetProfilesHelper
    : public base::RefCountedThreadSafe<GetProfilesHelper> {
 public:
  // Interface the delegate classes should implement.
  class Delegate {
   public:
    virtual void OnGetProfilesDone(
        const std::vector<std::wstring>& profiles) = 0;
    virtual ~Delegate() { }
  };

  explicit GetProfilesHelper(Delegate* delegate);

  // Asynchronous call to get the list of profiles. Calls the delegate when done
  // on either the given target loop or the message loop on which this function
  // is called if target loop is NULL.
  void GetProfiles(MessageLoop* target_loop);

  // Records that the delegate is deleted.
  void OnDelegateDeleted();

 private:
  // Helper to get the profiles from user data manager.
  void GetProfilesFromManager();

  // Helper to invoke the delegate.
  void InvokeDelegate(std::vector<std::wstring>* profiles);

  // Delegate to call.
  Delegate* delegate_;
  // Message loop to post tasks on completion of loading profiles.
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(GetProfilesHelper);
};

#endif  // CHROME_BROWSER_USER_DATA_MANAGER_H_
