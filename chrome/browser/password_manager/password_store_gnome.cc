// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_gnome.h"

#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"

using std::map;
using std::string;
using std::vector;

// Schema is analagous to the fields in PasswordForm.
const GnomeKeyringPasswordSchema PasswordStoreGnome::kGnomeSchema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET, {
    { "origin_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "action_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "username_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "username_value", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "password_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "submit_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "ssl_valid", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { "preferred", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { "date_created", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "blacklisted_by_user", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { "scheme", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { NULL }
  }
};

PasswordStoreGnome::PasswordStoreGnome() {
}

PasswordStoreGnome::~PasswordStoreGnome() {
}

bool PasswordStoreGnome::Init() {
  return PasswordStore::Init() && gnome_keyring_is_available();
}

void PasswordStoreGnome::AddLoginImpl(const PasswordForm& form) {
  AutoLock l(gnome_keyring_lock_);
  GnomeKeyringResult result = gnome_keyring_store_password_sync(
      &kGnomeSchema,
      NULL,  // Default keyring.
      // TODO(johnmaguire@google.com): Localise this.
      "Form password stored by Chrome",
      WideToASCII(form.password_value).c_str(),
      "origin_url", form.origin.spec().c_str(),
      "action_url", form.action.spec().c_str(),
      "username_element", form.username_element.c_str(),
      "username_value", form.username_value.c_str(),
      "password_element", form.password_element.c_str(),
      "submit_element", form.submit_element.c_str(),
      "signon_realm", form.signon_realm.c_str(),
      "ssl_valid", form.ssl_valid,
      "preferred", form.preferred,
      "date_created", Int64ToString(base::Time::Now().ToTimeT()).c_str(),
      "blacklisted_by_user", form.blacklisted_by_user,
      "scheme", form.scheme,
      NULL);

  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring save failed: "
               << gnome_keyring_result_to_message(result);
  }
}

void PasswordStoreGnome::UpdateLoginImpl(const PasswordForm& form) {
  AddLoginImpl(form);  // Add & Update are the same in gnome keyring.
}

void PasswordStoreGnome::RemoveLoginImpl(const PasswordForm& form) {
  AutoLock l(gnome_keyring_lock_);
  GnomeKeyringResult result = gnome_keyring_delete_password_sync(
      &kGnomeSchema,
      "origin_url", form.origin.spec().c_str(),
      "action_url", form.action.spec().c_str(),
      "username_element", form.username_element.c_str(),
      "username_value", form.username_value.c_str(),
      "password_element", form.password_element.c_str(),
      "submit_element", form.submit_element.c_str(),
      "signon_realm", form.signon_realm.c_str(),
      "ssl_valid", form.ssl_valid,
      "preferred", form.preferred,
      "date_created", Int64ToString(form.date_created.ToTimeT()).c_str(),
      "blacklisted_by_user", form.blacklisted_by_user,
      "scheme", form.scheme,
      NULL);
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring delete failed: "
               << gnome_keyring_result_to_message(result);
  }
}

void PasswordStoreGnome::GetLoginsImpl(GetLoginsRequest* request) {
  AutoLock l(gnome_keyring_lock_);
  GList* found = NULL;
  // Search gnome keyring for matching passwords.
  GnomeKeyringResult result = gnome_keyring_find_itemsv_sync(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      &found,
      "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      request->form.signon_realm.c_str(),
      NULL);
  vector<PasswordForm*> forms;
  if (result == GNOME_KEYRING_RESULT_NO_MATCH) {
    NotifyConsumer(request, forms);
    return;
  } else if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    NotifyConsumer(request, forms);
    return;
  }

  // Parse all the results from the returned GList into a
  // vector<PasswordForm*>. PasswordForms are allocated on the heap. These
  // will be deleted by the consumer.
  GList* element = g_list_first(found);
  while (element != NULL) {
    GnomeKeyringFound* data = static_cast<GnomeKeyringFound*>(element->data);
    char* password = data->secret;

    GnomeKeyringAttributeList* attributes = data->attributes;
    // Read the string & int attributes into the appropriate map.
    map<string, string> string_attribute_map;
    map<string, uint32> uint_attribute_map;
    for (unsigned int i = 0; i < attributes->len; ++i) {
      GnomeKeyringAttribute attribute =
          gnome_keyring_attribute_list_index(attributes, i);
      if (attribute.type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
        string_attribute_map[string(attribute.name)] =
            string(attribute.value.string);
      } else if (attribute.type == GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32) {
        uint_attribute_map[string(attribute.name)] = attribute.value.integer;
      }
    }

    PasswordForm* form = new PasswordForm();
    form->origin = GURL(string_attribute_map["origin_url"]);
    form->action = GURL(string_attribute_map["action_url"]);
    form->username_element =
        ASCIIToWide(string(string_attribute_map["username_element"]));
    form->username_value =
        ASCIIToWide(string(string_attribute_map["username_value"]));
    form->password_element =
        ASCIIToWide(string(string_attribute_map["password_element"]));
    form->password_value = ASCIIToWide(string(password));
    form->submit_element =
        ASCIIToWide(string(string_attribute_map["submit_element"]));
    form->signon_realm = uint_attribute_map["signon_realm"];
    form->ssl_valid = uint_attribute_map["ssl_valid"];
    form->preferred = uint_attribute_map["preferred"];
    string date = string_attribute_map["date_created"];
    int64 date_created = 0;
    DCHECK(StringToInt64(date, &date_created) && date_created != 0);
    form->date_created = base::Time::FromTimeT(date_created);
    form->blacklisted_by_user = uint_attribute_map["blacklisted_by_user"];
    form->scheme =
        static_cast<PasswordForm::Scheme>(uint_attribute_map["scheme"]);

    forms.push_back(form);

    element = g_list_next(element);
  }
  gnome_keyring_found_list_free(found);
  found = NULL;

  NotifyConsumer(request, forms);
}
