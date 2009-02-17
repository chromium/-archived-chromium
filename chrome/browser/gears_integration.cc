// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gears_integration.h"

#include "base/gfx/png_encoder.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_plugin_host.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/gears_api.h"
#include "googleurl/src/gurl.h"
#include "net/base/base64.h"
#include "skia/include/SkBitmap.h"
#include "webkit/glue/dom_operations.h"

// The following 2 helpers are borrowed from the Gears codebase.

const size_t kUserPathComponentMaxChars  = 64;

// Returns true if and only if the char meets the following criteria:
//
// - visible ASCII
// - None of the following characters: / \ : * ? " < > | ; ,
//
// This function is a heuristic that should identify most strings that are
// invalid pathnames on popular OSes. It's both overinclusive and
// underinclusive, though.
template<class CharT>
inline bool IsCharValidInPathComponent(CharT c) {
  // Not visible ASCII?
  // Note: the Gears version of this function excludes spaces (32) as well.  We
  // allow them for file names.
  if (c < 32 || c >= 127) {
    return false;
  }

  // Illegal characters?
  switch (c) {
    case '/':
    case '\\':
    case ':':
    case '*':
    case '?':
    case '"':
    case '<':
    case '>':
    case '|':
    case ';':
    case ',':
      return false;

    default:
      return true;
  }
}

// Modifies a string, replacing characters that are not valid in a file path
// component with the '_' character. Also replaces leading and trailing dots
// with the '_' character.
// See IsCharValidInPathComponent
template<class StringT>
inline void EnsureStringValidPathComponent(StringT &s) {
  if (s.empty()) {
    return;
  }

  typename StringT::iterator iter = s.begin();
  typename StringT::iterator end = s.end();

  // Does it start with a dot?
  if (*iter == '.') {
    *iter = '_';
    ++iter;  // skip it in the loop below
  }
  // Is every char valid?
  while (iter != end) {
    if (!IsCharValidInPathComponent(*iter)) {
      *iter = '_';
    }
    ++iter;
  }
  // Does it end with a dot?
  --iter;
  if (*iter == '.') {
    *iter = '_';
  }

  // Is it too long?
  if (s.size() > kUserPathComponentMaxChars)
    s.resize(kUserPathComponentMaxChars);
}

void GearsSettingsPressed(gfx::NativeWindow parent_wnd) {
  CPBrowsingContext context = static_cast<CPBrowsingContext>(
      reinterpret_cast<uintptr_t>(parent_wnd));
  CPHandleCommand(GEARSPLUGINCOMMAND_SHOW_SETTINGS, NULL, context);
}

// Gears only supports certain icon sizes.
enum GearsIconSizes {
  SIZE_16x16 = 0,
  SIZE_32x32,
  SIZE_48x48,
  SIZE_128x128,
  NUM_GEARS_ICONS,
};

// Helper function to convert a 16x16 favicon to a data: URL with the icon
// encoded as a PNG.
static GURL ConvertSkBitmapToDataURL(const SkBitmap& icon) {
  DCHECK(!icon.isNull());
  DCHECK(icon.width() == 16 && icon.height() == 16);

  // Get the FavIcon data.
  std::vector<unsigned char> icon_data;
  PNGEncoder::EncodeBGRASkBitmap(icon, false, &icon_data);

  // Base64-encode it (to make it a data URL).
  std::string icon_data_str(reinterpret_cast<char*>(&icon_data[0]),
                            icon_data.size());
  std::string icon_base64_encoded;
  net::Base64Encode(icon_data_str, &icon_base64_encoded);
  GURL icon_url("data:image/png;base64," + icon_base64_encoded);

  return icon_url;
}

// This class holds and manages the data passed to the
// GEARSPLUGINCOMMAND_CREATE_SHORTCUT plugin command.
class CreateShortcutCommand : public CPCommandInterface {
 public:
  CreateShortcutCommand(
      const std::string& name, const std::string& orig_name, 
      const std::string& url, const std::string& description,
      const std::vector<webkit_glue::WebApplicationInfo::IconInfo> &icons,
      const SkBitmap& fallback_icon,
      GearsCreateShortcutCallback* callback)
      : name_(name), url_(url), description_(description),
        orig_name_(orig_name), callback_(callback),
        calling_loop_(MessageLoop::current()) {
    // shortcut_data_ has the same lifetime as our strings, so we just
    // point it at their internal data.
    memset(&shortcut_data_, 0, sizeof(shortcut_data_));
    shortcut_data_.name = name_.c_str();
    shortcut_data_.url = url_.c_str();
    shortcut_data_.description = description_.c_str();
    shortcut_data_.orig_name = orig_name_.c_str();

    // Search the icons array for Gears-supported sizes and copy the strings.
    bool has_icon = false;

    for (size_t i = 0; i < icons.size(); ++i) {
      const webkit_glue::WebApplicationInfo::IconInfo& icon = icons[i];
      if (icon.width == 16 && icon.height == 16) {
        has_icon = true;
        InitIcon(SIZE_16x16, icon.url, 16, 16);
      } else if (icon.width == 32 && icon.height == 32) {
        has_icon = true;
        InitIcon(SIZE_32x32, icon.url, 32, 32);
      } else if (icon.width == 48 && icon.height == 48) {
        has_icon = true;
        InitIcon(SIZE_48x48, icon.url, 48, 48);
      } else if (icon.width == 128 && icon.height == 128) {
        has_icon = true;
        InitIcon(SIZE_128x128, icon.url, 128, 128);
      }
    }

    if (!has_icon) {
      // Fall back to the favicon only if the site provides no icons at all.  We
      // assume if a site provides any icons, it wants to override default
      // behavior.
      InitIcon(SIZE_16x16, ConvertSkBitmapToDataURL(fallback_icon), 16, 16);
    }

    shortcut_data_.command_interface = this;
  }
  virtual ~CreateShortcutCommand() { }

  // CPCommandInterface
  virtual void* GetData() { return &shortcut_data_; }
  virtual void OnCommandInvoked(CPError retval) {
    if (retval != CPERR_IO_PENDING) {
      // Older versions of Gears don't send a response, so don't wait for one.
      OnCommandResponse(CPERR_FAILURE);
    }
  }
  virtual void OnCommandResponse(CPError retval) {
    calling_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &CreateShortcutCommand::ReportResults, retval));
  }

 private:
  void ReportResults(CPError retval) {
    // Other code only knows about the original GearsShortcutData.  Pass our
    // GearsShortcutData2 off as one of those - but use the unmodified name.
    // TODO(mpcomplete): this means that Gears will have stored its sanitized
    // filename, but not expose it to us.  We will use the unsanitized version,
    // so our name will potentially differ.  This is relevant because we store
    // some prefs keyed off the webapp name.
    shortcut_data_.name = shortcut_data_.orig_name;
    callback_->Run(shortcut_data_, retval == CPERR_SUCCESS);
    delete this;
  }

  void InitIcon(GearsIconSizes size, const GURL& url, int width, int height) {
    icon_urls_[size] = url.spec();  // keeps the string memory in scope
    shortcut_data_.icons[size].url = icon_urls_[size].c_str();
    shortcut_data_.icons[size].width = width;
    shortcut_data_.icons[size].height = height;
  }

  GearsCreateShortcutData shortcut_data_;
  std::string name_;
  std::string url_;
  std::string description_;
  std::string icon_urls_[NUM_GEARS_ICONS];
  std::string orig_name_;
  scoped_ptr<GearsCreateShortcutCallback> callback_;
  MessageLoop* calling_loop_;
};

// Allows InvokeLater without adding refcounting.  The object is only deleted
// when its last InvokeLater is run anyway.
template<>
void RunnableMethodTraits<CreateShortcutCommand>::RetainCallee(
    CreateShortcutCommand* remover) {
}
template<>
void RunnableMethodTraits<CreateShortcutCommand>::ReleaseCallee(
    CreateShortcutCommand* remover) {
}

void GearsCreateShortcut(
    const webkit_glue::WebApplicationInfo& app_info,
    const std::wstring& fallback_name,
    const GURL& fallback_url,
    const SkBitmap& fallback_icon,
    GearsCreateShortcutCallback* callback) {
  std::wstring name =
      !app_info.title.empty() ? app_info.title : fallback_name;
  std::string orig_name_utf8 = WideToUTF8(name);
  EnsureStringValidPathComponent(name);

  std::string name_utf8 = WideToUTF8(name);
  std::string description_utf8 = WideToUTF8(app_info.description);
  const GURL& url =
      !app_info.app_url.is_empty() ? app_info.app_url : fallback_url;

  CreateShortcutCommand* command =
      new CreateShortcutCommand(name_utf8, orig_name_utf8, url.spec(),
                                description_utf8,
                                app_info.icons, fallback_icon, callback);
  CPHandleCommand(GEARSPLUGINCOMMAND_CREATE_SHORTCUT, command, NULL);
}

// This class holds and manages the data passed to the
// GEARSPLUGINCOMMAND_GET_SHORTCUT_LIST plugin command.  When the command is
// invoked, we proxy the results over to the calling thread.
class QueryShortcutsCommand : public CPCommandInterface {
 public:
  explicit QueryShortcutsCommand(GearsQueryShortcutsCallback* callback) :
      callback_(callback), calling_loop_(MessageLoop::current()) {
    shortcut_list_.shortcuts = NULL;
    shortcut_list_.num_shortcuts = 0;
  }
  virtual ~QueryShortcutsCommand() { }
  virtual void* GetData() { return &shortcut_list_; }
  virtual void OnCommandInvoked(CPError retval) {
    calling_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QueryShortcutsCommand::ReportResults, retval));
  }

 private:
  void ReportResults(CPError retval) {
    callback_->Run(retval == CPERR_SUCCESS ? &shortcut_list_ : NULL);
    FreeGearsShortcutList();
    delete this;
  }

  void FreeGearsShortcutList() {
    for (size_t i = 0; i < shortcut_list_.num_shortcuts; ++i) {
      CPB_Free(const_cast<char*>(shortcut_list_.shortcuts[i].description));
      CPB_Free(const_cast<char*>(shortcut_list_.shortcuts[i].name));
      CPB_Free(const_cast<char*>(shortcut_list_.shortcuts[i].url));
      for (size_t j = 0; j < 4; ++j)
        CPB_Free(const_cast<char*>(shortcut_list_.shortcuts[i].icons[j].url));
    }
    CPB_Free(shortcut_list_.shortcuts);
  }

  GearsShortcutList shortcut_list_;
  scoped_ptr<GearsQueryShortcutsCallback> callback_;
  MessageLoop* calling_loop_;
};

// Allows InvokeLater without adding refcounting.  The object is only deleted
// when its last InvokeLater is run anyway.
template<>
void RunnableMethodTraits<QueryShortcutsCommand>::RetainCallee(
    QueryShortcutsCommand* remover) {
}
template<>
void RunnableMethodTraits<QueryShortcutsCommand>::ReleaseCallee(
    QueryShortcutsCommand* remover) {
}

void GearsQueryShortcuts(GearsQueryShortcutsCallback* callback) {
  CPHandleCommand(GEARSPLUGINCOMMAND_GET_SHORTCUT_LIST,
      new QueryShortcutsCommand(callback),
      NULL);
}

