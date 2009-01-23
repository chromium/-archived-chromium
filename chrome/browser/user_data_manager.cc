// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_data_manager.h"

#include <windows.h>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/l10n_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/shell_util.h"

#include "chromium_strings.h"

namespace {

// Helper to start chrome for a given profile index. The helper takes care of
// enumerating profiles on the file thread and then it launches Chrome for the
// appropriate profile on the original thread.
// An instance of this class should always be created on the heap, and it will
// delete itself when the launch is done.
class LaunchChromeForProfileIndexHelper : GetProfilesHelper::Delegate {
 public:
  // Creates an instance with the given data manager and to launch chrome for
  // the profile with the given index.
  LaunchChromeForProfileIndexHelper(const UserDataManager* manager, int index);
  virtual ~LaunchChromeForProfileIndexHelper();

  // Starts the asynchronous launch.
  void StartLaunch();

  // GetProfilesHelper::Delegate method.
  void OnGetProfilesDone(const std::vector<std::wstring>& profiles);

 private:
  int index_;
  const UserDataManager* manager_;
  scoped_refptr<GetProfilesHelper> profiles_helper_;

  DISALLOW_COPY_AND_ASSIGN(LaunchChromeForProfileIndexHelper);
};

}  // namespace

LaunchChromeForProfileIndexHelper::LaunchChromeForProfileIndexHelper(
    const UserDataManager* manager,
    int index)
    : manager_(manager),
      index_(index),
// Don't complain about using "this" in initializer list.
MSVC_PUSH_DISABLE_WARNING(4355)
      profiles_helper_(new GetProfilesHelper(this)) {
MSVC_POP_WARNING()
  DCHECK(manager);
}

LaunchChromeForProfileIndexHelper::~LaunchChromeForProfileIndexHelper() {
  profiles_helper_->OnDelegateDeleted();
}

void LaunchChromeForProfileIndexHelper::StartLaunch() {
  profiles_helper_->GetProfiles(NULL);
}

void LaunchChromeForProfileIndexHelper::OnGetProfilesDone(
    const std::vector<std::wstring>& profiles) {
  if (index_ >= 0 && index_ < static_cast<int>(profiles.size()))
    manager_->LaunchChromeForProfile(profiles[index_]);

  // We are done, delete ourselves.
  delete this;
}

// Separator used in folder names between the prefix and the profile name.
// For e.g. a folder for the profile "Joe" would be named "User Data-Joe".
static const wchar_t kProfileFolderSeparator[] = L"-";

// static
UserDataManager* UserDataManager::instance_ = NULL;

// static
void UserDataManager::Create() {
  DCHECK(!instance_);
  std::wstring user_data;
  PathService::Get(chrome::DIR_USER_DATA, &user_data);
  instance_ = new UserDataManager(user_data);
}

// static
UserDataManager* UserDataManager::Get() {
  DCHECK(instance_);
  return instance_;
}

UserDataManager::UserDataManager(const std::wstring& user_data_root)
    : user_data_root_(user_data_root) {
  // Determine current profile name and current folder name.
  current_folder_name_ = file_util::GetFilenameFromPath(user_data_root);
  bool success = GetProfileNameFromFolderName(current_folder_name_,
                                              &current_profile_name_);
  // The current profile is a default profile if the current user data folder
  // name is just kUserDataDirname or when the folder name doesn't have the
  // kUserDataDirname as prefix.
  is_current_profile_default_ =
      !success || (current_folder_name_ == chrome::kUserDataDirname);

  // (TODO:munjal) Fix issue 5070:
  // http://code.google.com/p/chromium/issues/detail?id=5070

  file_util::UpOneDirectory(&user_data_root_);
}

UserDataManager::~UserDataManager() {
}

// static
bool UserDataManager::GetProfileNameFromFolderName(
    const std::wstring& folder_name,
    std::wstring* profile_name) {
  // The folder name should start with a specific prefix for it to be a valid
  // profile folder.
  if (folder_name.find(chrome::kUserDataDirname) != 0)
    return false;

  // Seems like we cannot use arraysize macro for externally defined constants.
  // Is there a way?
  unsigned int prefix_length = wcslen(chrome::kUserDataDirname);
  // Subtract 1 from the size of the array for trailing null character.
  unsigned int separator_length = arraysize(kProfileFolderSeparator) - 1;

  // It's safe to use profile_name variable for intermediate values since we
  // will always return true now.
  *profile_name = folder_name.substr(prefix_length);
  // Remove leading separator if present.
  if (profile_name->find_first_of(kProfileFolderSeparator) == 0)
    *profile_name = profile_name->substr(separator_length);

  if (profile_name->empty())
    *profile_name = chrome::kNotSignedInProfile;

  return true;
}

// static
std::wstring UserDataManager::GetFolderNameFromProfileName(
    const std::wstring& profile_name) {
  std::wstring folder_name = chrome::kUserDataDirname;
  if (profile_name != chrome::kNotSignedInProfile) {
    folder_name += kProfileFolderSeparator;
    folder_name += profile_name;
  }
  return folder_name;
}

std::wstring UserDataManager::GetUserDataFolderForProfile(
    const std::wstring& profile_name) const {
  std::wstring folder_name = GetFolderNameFromProfileName(profile_name);
  std::wstring folder_path(user_data_root_);
  file_util::AppendToPath(&folder_path, folder_name);
  return folder_path;
}

std::wstring UserDataManager::GetCommandForProfile(
    const std::wstring& profile_name) const {
  std::wstring user_data_dir = GetUserDataFolderForProfile(profile_name);
  std::wstring command;
  PathService::Get(base::FILE_EXE, &command);
  CommandLine command_line(command);
  command_line.AppendSwitchWithValue(switches::kUserDataDir,
                                     user_data_dir);
  std::wstring local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  command_line.AppendSwitchWithValue(switches::kParentProfile,
                                     local_state_path);
  return command_line.command_line_string();
}

void UserDataManager::LaunchChromeForProfile(
    const std::wstring& profile_name) const {
  std::wstring command = GetCommandForProfile(profile_name);
  base::LaunchApp(command, false, false, NULL);
}

void UserDataManager::LaunchChromeForProfile(int index) const {
  // Helper deletes itself when done.
  LaunchChromeForProfileIndexHelper* helper =
      new LaunchChromeForProfileIndexHelper(this, index);
  helper->StartLaunch();
}

void UserDataManager::GetProfiles(std::vector<std::wstring>* profiles) const {
  // This function should be called on the file thread.
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::FILE));
  file_util::FileEnumerator file_enum(
      FilePath::FromWStringHack(user_data_root_),
      false, file_util::FileEnumerator::DIRECTORIES);
  std::wstring folder_name;
  while (!(folder_name = file_enum.Next().ToWStringHack()).empty()) {
    folder_name = file_util::GetFilenameFromPath(folder_name);
    std::wstring profile_name;
    if (GetProfileNameFromFolderName(folder_name, &profile_name))
      profiles->push_back(profile_name);
  }
}

bool UserDataManager::CreateDesktopShortcutForProfile(
    const std::wstring& profile_name) const {
  std::wstring exe_path;
  std::wstring shortcut_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path) ||
      !ShellUtil::GetDesktopPath(false, &shortcut_path))
    return false;

  // Working directory.
  std::wstring exe_folder = file_util::GetDirectoryFromPath(exe_path);

  // Command and arguments.
  std::wstring cmd;
  cmd = StringPrintf(L"\"%ls\"", exe_path.c_str());
  std::wstring user_data_dir = GetUserDataFolderForProfile(profile_name);
  std::wstring args = CommandLine::PrefixedSwitchStringWithValue(
      switches::kUserDataDir,
      user_data_dir);
  args = StringPrintf(L"\"%ls\"", args.c_str());

  // Shortcut path.
  std::wstring shortcut_name = l10n_util::GetStringF(
      IDS_START_IN_PROFILE_SHORTCUT_NAME,
      profile_name);
  shortcut_name.append(L".lnk");
  file_util::AppendToPath(&shortcut_path, shortcut_name);

  return file_util::CreateShortcutLink(cmd.c_str(),
                                       shortcut_path.c_str(),
                                       exe_folder.c_str(),
                                       args.c_str(),
                                       NULL,
                                       exe_path.c_str(),
                                       0);
}

GetProfilesHelper::GetProfilesHelper(Delegate* delegate)
    : delegate_(delegate) {
}

void GetProfilesHelper::GetProfiles(MessageLoop* target_loop) {
  // If the target loop is not NULL then use the target loop, or if it's NULL
  // then use the current message loop to post a task on it later when we are
  // done building a list of profiles.
  if (target_loop) {
    message_loop_ = target_loop;
  } else {
    message_loop_ = MessageLoop::current();
  }
  DCHECK(message_loop_);
  MessageLoop* file_loop = ChromeThread::GetMessageLoop(ChromeThread::FILE);
  file_loop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &GetProfilesHelper::GetProfilesFromManager));
}

// Records that the delegate is closed.
void GetProfilesHelper::OnDelegateDeleted() {
  delegate_ = NULL;
}

void GetProfilesHelper::GetProfilesFromManager() {
  // This function should be called on the file thread.
  DCHECK(MessageLoop::current() ==
      ChromeThread::GetMessageLoop(ChromeThread::FILE));

  // If the delegate is gone by now, no need to do any work.
  if (!delegate_)
    return;

  scoped_ptr< std::vector<std::wstring> > profiles(
      new std::vector<std::wstring>);
  UserDataManager::Get()->GetProfiles(profiles.get());

  // Post a task on the original thread to call the delegate.
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &GetProfilesHelper::InvokeDelegate,
                        profiles.release()));
}

void GetProfilesHelper::InvokeDelegate(std::vector<std::wstring>* profiles) {
  scoped_ptr< std::vector<std::wstring> > udd_profiles(profiles);
  // If the delegate is gone by now, no need to do any work.
  if (delegate_)
    delegate_->OnGetProfilesDone(*udd_profiles.get());
}
