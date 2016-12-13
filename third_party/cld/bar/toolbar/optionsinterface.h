// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Author: Vadim Berman <vadimb>

#ifndef BAR_TOOLBAR_OPTIONSINTERFACE_H_
#define BAR_TOOLBAR_OPTIONSINTERFACE_H_

#include "cld/bar/toolbar/option_constants.h"

// Interface to the Options class. See more comments in "bar/toolbar/options.h".

// Forward event args declaration.
struct OnOptionArgs;
struct OnChangeArgs;
struct OnDefaultArgs;
struct OnRestrictedArgs;
class ExternalOptions;
template <typename T> class Callback;

// The Toolbar options service. You can access it from toolbar.options().
struct IOptions : public OptionConstants {
 public:
public:
  struct Definition {
    Option option;
    Type type;
    Location location;
    CString name;                  // For internal options, it equals to the
    // registry name.
    ResetBehavior reset_behavior;
    CComVariant default_value;
    bool synchronizable;           // Can be synced across computers.
    CString legacy_registry_name;  // The name in T1..T3 (options version 1.1)
    CString class_id;              // The clsid of the component that the option
    // belongs to.
  };


  virtual ~IOptions() {}

  // Same as GetOptionCount but different type to avoid tiresome static_casts.
  virtual Option GetLastOption() = 0;

  // (Re)Init external options from satellite resource.
  virtual void InitExternalOptions() = 0;

  // Get option range for a external component by class id.
  virtual Option GetExternalOptionId(
    const CString& class_id, const CString& name) = 0;


  virtual ExternalOptions* GetExternalOptions() = 0;

  virtual void ResetSyncCacheOpened() = 0;

  // Forces to unload the current synchronizable option data loaded in memory.
  // This covers a rare corner case happened in sync, when option data needs
  // to switch between different users's cached data without any new data
  // change involved. In such case, PersistedChangeNumber() never changes,
  // therefore Refresh() function will not detect the data change, and we need
  // to call this function to force options to unload its data in memory.
  virtual void UnloadSynchronizableOptionData() = 0;

  // Clone the existing options. This method will clone only the data, callbacks
  // will not be cloned.
  virtual IOptions* Clone() = 0;

  // Tells the options system to not persist the changes to the registry. Every
  // call to BeginModify() must be balanced with call to EndModify();
  virtual void BeginModify() = 0;
  // Persists or discards the modified options.
  virtual void EndModify(ModifyCompletion action) = 0;
  // PERMANENTLY disable persistance.
  // This is used after uninstall to prevent "zombie" toolbars from writing
  // to the registry.
  virtual void DisablePersist() = 0;

  // Discards any in memory changes (while within Begin/EndModify)
  virtual void DiscardModifications() = 0;

  // Resets all the options to default value. It will not save the options.
  virtual void ResetToDefaults() = 0;
  // Deletes all the options values (not set to default, DELETE). This call will
  // also delete event the options set with KEEP. It delete really ALL
  virtual void ObliterateAll() = 0;

  // If current options are out of sync, re-load the modified values
  virtual void Refresh() = 0;

  // Options types definitions.
  virtual Type GetType(Option option) = 0;
  virtual Location GetLocation(Option option) = 0;
  virtual const TCHAR* GetName(Option option) = 0;
  virtual CString GetRegistryName(Option option) = 0;
  virtual const TCHAR* GetLegacyName(Option option, DWORD version) = 0;
  virtual const TCHAR* GetClassId(Option option) = 0;

  // Returns true if the option is a serve cache copy.
  virtual bool IsServerCache(Option option) = 0;
  // Sets the value of is_server_cache_ of an option.
  virtual void SetIsServerCache(Option option, bool value) = 0;
  // Sets the option store for the current sync user.
  virtual void SetOptionServerCacheStore(const CString& current_sync_user) = 0;

  // Returns true if the options has been modified and not saved.
  virtual bool IsModified(Option option) = 0;
  virtual void SetModified(Option option, bool modified) = 0;
  // Returns true if some option has been modified.
  virtual bool AnyOptionModified() = 0;

  // Default value
  virtual CComVariant GetDefaultValue(Option option) = 0;
  virtual void ResetToDefault(Option option) = 0;

  // Restricted options
  virtual bool IsRestrictedOption(Option option) = 0;
  virtual CComVariant GetRestrictedValue(Option option) = 0;
  virtual bool IsReadonlyOption(Option option) = 0;

  // Getters
  virtual int GetInt(Option option) = 0;
  virtual unsigned int GetUint(Option option) = 0;
  virtual CString GetString(Option option) = 0;
  virtual bool GetBool(Option option) = 0;
  // Get a setup option - this is an option which can be either yes, no or "Ask
  // the user".
  // option:   The option to query.
  // always_return_yes_or_no:
  //           If true, the function will convert "Ask" results to their
  //           defaults.
  // Please note: The option itself is an integer option.
  virtual UserOption GetToastOption(Option option,
      bool always_return_yes_or_no) = 0;

  // Setters
  virtual HRESULT SetInt(Option option, int value) = 0;
  virtual HRESULT SetUint(Option option, unsigned int value) = 0;
  virtual HRESULT SetString(Option option, const TCHAR* value) = 0;
  virtual HRESULT SetBool(Option option, bool value) = 0;
  // Set a user option.
  virtual HRESULT SetToastOption(Option option, UserOption value) = 0;
  // Allows setting of read-only (setup) options at runtime.
  virtual HRESULT SetReadOnlyBool(Option option, bool value) = 0;
  virtual HRESULT SetReadOnlyInt(Option option, int value) = 0;

  // Fires when the system need the obtain custom default value for option.
  // If you register handler for NONE option it will fire for every request
  // for option default value.
  virtual void SetOnDefaultHandler(Option option,
                                   Callback<OnDefaultArgs>* callback) = 0;

  // Fires just before new option value is to be saved (or deleted) to the
  // registry. If you register handler for NONE option it will fire for every
  // option save.
  virtual void SetOnSaveHandler(Option option,
      Callback<OnOptionArgs>* callback) = 0;

  // Fires after SaveAll() saves the options to the registry. If you call
  // SaveAll() and AnyOptionModified() returns false, this event will not fire.
  virtual void SetOnSaveAllHandler(Callback<OnOptionArgs>* callback) = 0;

  // Fires when a option value is modified. If you register handler for NONE
  // option it will fire for every option change.
  virtual void SetOnChangeHandler(Option option,
      Callback<OnChangeArgs>* callback) = 0;

  // Fires when the system need the obtain restricted value for option.
  // If you register handler for NONE option it will fire for every request
  // for restricted value. Option that do not wish to be restricted must return
  // VT_EMPTY for the value.
  virtual void SetOnRestrictedHandler(Option option,
      Callback<OnRestrictedArgs>* callback) = 0;

  // This function triggers a call to OnChangeGoogleHome.
  virtual HRESULT FireOnChangeGoogleHome() = 0;


  // TODO(zelidrag): These used to be private / protected before,
  // we should have a better way to refactor them
  // Returns true if the option is synchronizable.
  virtual bool Synchronizable(Option option) = 0;

  virtual bool GetBoolLocalForced(Option option) = 0;
  virtual int GetIntLocalForced(Option option) = 0;
  virtual unsigned int GetUintLocalForced(Option option) = 0;
  virtual CString GetStringLocalForced(Option option) = 0;

  virtual HRESULT SetBoolSyncForced(Option option, bool value) = 0;
  virtual HRESULT SetIntSyncForced(Option option, int value) = 0;
  virtual HRESULT SetUintSyncForced(Option option, unsigned int value) = 0;
  virtual HRESULT SetStringSyncForced(Option option, const TCHAR* value) = 0;

  virtual bool IsPersistedInServerCache(Option option) = 0;

  virtual HRESULT SetBoolLocalForced(Option option, bool value) = 0;
  virtual HRESULT SetIntLocalForced(Option option, int value) = 0;
  virtual HRESULT SetUintLocalForced(Option option, unsigned int value) = 0;
  virtual HRESULT SetStringLocalForced(Option option, const TCHAR* value) = 0;
  virtual HRESULT DeletePersistedLocalOption(Option option) = 0;
};



#endif  // BAR_TOOLBAR_OPTIONSINTERFACE_H_
