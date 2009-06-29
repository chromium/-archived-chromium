// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mime_util.h"

#include <gtk/gtk.h>
#include <sys/time.h>
#include <time.h>

#include <cstdlib>
#include <list>
#include <map>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/third_party/xdg_mime/xdgmime.h"

namespace {

// Toggles whether we can handle SVG icons. See http://crbug.com/12272
const bool kEnableSVG = false;

class IconTheme;

class MimeUtilConstants {
 public:

  // In seconds, specified by icon theme specs.
  const int kUpdateInterval;

  // Store icon directories and their mtimes.
  std::map<FilePath, int>* icon_dirs_;

  // Store icon formats.
  std::vector<std::string>* icon_formats_;

  // Store loaded icon_theme.
  std::map<std::string, IconTheme*>* icon_themes_;

  static const size_t kDefaultThemeNum = 4;

  // The default theme.
  IconTheme* default_themes_[kDefaultThemeNum];

  time_t last_check_time_;

 private:
  MimeUtilConstants()
      : kUpdateInterval(5),
        icon_dirs_(NULL),
        icon_formats_(NULL),
        icon_themes_(NULL),
        last_check_time_(0) {
    for (size_t i = 0; i < kDefaultThemeNum; ++i)
      default_themes_[i] = NULL;
  }
  ~MimeUtilConstants();

  friend struct DefaultSingletonTraits<MimeUtilConstants>;

  DISALLOW_COPY_AND_ASSIGN(MimeUtilConstants);
};

// IconTheme represents an icon theme as defined by the xdg icon theme spec.
// Example themes on GNOME include 'Human' and 'Mist'.
// Example themes on KDE include 'crystalsvg' and 'kdeclassic'.
class IconTheme {
 public:
  // A theme consists of multiple sub-directories, like '32x32' and 'scalable'.
  class SubDirInfo {
   public:
    // See spec for details.
    enum Type {
      Fixed,
      Scalable,
      Threshold
    };
    SubDirInfo()
        : size(0),
          type(Threshold),
          max_size(0),
          min_size(0),
          threshold(2) {
    }
    size_t size;  // Nominal size of the icons in this directory.
    Type type;  // Type of the icon size.
    size_t max_size;  // Maximum size that the icons can be scaled to.
    size_t min_size;  // Minimum size that the icons can be scaled to.
    size_t threshold;  // Maximum difference from desired size. 2 by default.
  };

  explicit IconTheme(const std::string& name);

  ~IconTheme() {
    delete[] info_array_;
  }

  // Returns the path to an icon with the name |icon_name| and a size of |size|
  // pixels. If the icon does not exist, but |inherits| is true, then look for
  // the icon in the parent theme.
  FilePath GetIconPath(const std::string& icon_name, int size, bool inherits);

  // Load a theme with the name |theme_name| into memory. Returns null if theme
  // is invalid.
  static IconTheme* LoadTheme(const std::string& theme_name);

 private:
  // Returns the path to an icon with the name |icon_name| in |subdir|.
  FilePath GetIconPathUnderSubdir(const std::string& icon_name,
                                  const std::string& subdir);

  // Whether the theme loaded properly.
  bool IsValid() {
    return index_theme_loaded_;
  }

  // Read and parse |file| which is usually named 'index.theme' per theme spec.
  bool LoadIndexTheme(const FilePath& file);

  // Checks to see if the icons in |info| matches |size| (in pixels). Returns
  // 0 if they match, or the size difference in pixels.
  size_t MatchesSize(SubDirInfo* info, size_t size);

  // Yet another function to read a line.
  std::string ReadLine(FILE* fp);

  // Set directories to search for icons to the comma-separated list |dirs|.
  bool SetDirectories(const std::string& dirs);

  bool index_theme_loaded_;  // True if an instance is properly loaded.
  // store the scattered directories of this theme.
  std::list<FilePath> dirs_;

  // store the subdirs of this theme and array index of |info_array_|.
  std::map<std::string, int> subdirs_;
  SubDirInfo* info_array_;  // List of sub-directories.
  std::string inherits_;  // Name of the theme this one inherits from.
};

IconTheme::IconTheme(const std::string& name)
  : index_theme_loaded_(false),
    info_array_(NULL) {
  // Iterate on all icon directories to find directories of the specified
  // theme and load the first encountered index.theme.
  std::map<FilePath, int>::iterator iter;
  FilePath theme_path;
  std::map<FilePath, int>* icon_dirs =
      Singleton<MimeUtilConstants>::get()->icon_dirs_;
  for (iter = icon_dirs->begin(); iter != icon_dirs->end(); ++iter) {
    theme_path = iter->first.Append(name);
    if (!file_util::DirectoryExists(theme_path))
      continue;
    FilePath theme_index = theme_path.Append("index.theme");
    if (!index_theme_loaded_ && file_util::PathExists(theme_index)) {
      if (!LoadIndexTheme(theme_index))
        return;
      index_theme_loaded_ = true;
    }
    dirs_.push_back(theme_path);
  }
}

FilePath IconTheme::GetIconPath(const std::string& icon_name, int size,
                                bool inherits) {
  std::map<std::string, int>::iterator subdir_iter;
  FilePath icon_path;

  for (subdir_iter = subdirs_.begin();
       subdir_iter != subdirs_.end();
       ++subdir_iter) {
    SubDirInfo* info = &info_array_[subdir_iter->second];
    if (MatchesSize(info, size) == 0) {
      icon_path = GetIconPathUnderSubdir(icon_name, subdir_iter->first);
      if (!icon_path.empty())
        return icon_path;
    }
  }
  // Now looking for the mostly matched.
  int min_delta_seen = 9999;

  for (subdir_iter = subdirs_.begin();
       subdir_iter != subdirs_.end();
       ++subdir_iter) {
    SubDirInfo* info = &info_array_[subdir_iter->second];
    int delta = abs(MatchesSize(info, size));
    if (delta < min_delta_seen) {
      FilePath path = GetIconPathUnderSubdir(icon_name, subdir_iter->first);
      if (!path.empty()) {
        min_delta_seen = delta;
        icon_path = path;
      }
    }
  }

  if (!icon_path.empty() || !inherits || inherits_ == "")
    return icon_path;

  IconTheme* theme = LoadTheme(inherits_);
  if (theme)
    return theme->GetIconPath(icon_name, size, inherits);
  else
    return FilePath();
}

IconTheme* IconTheme::LoadTheme(const std::string& theme_name) {
  scoped_ptr<IconTheme> theme;
  std::map<std::string, IconTheme*>* icon_themes =
      Singleton<MimeUtilConstants>::get()->icon_themes_;
  if (icon_themes->find(theme_name) != icon_themes->end()) {
    theme.reset((*icon_themes)[theme_name]);
  } else {
    theme.reset(new IconTheme(theme_name));
    if (!theme->IsValid())
      theme.reset();
    (*icon_themes)[theme_name] = theme.get();
  }
  return theme.release();
}

FilePath IconTheme::GetIconPathUnderSubdir(const std::string& icon_name,
                                           const std::string& subdir) {
  FilePath icon_path;
  std::list<FilePath>::iterator dir_iter;
  std::vector<std::string>* icon_formats =
      Singleton<MimeUtilConstants>::get()->icon_formats_;
  for (dir_iter = dirs_.begin(); dir_iter != dirs_.end(); ++dir_iter) {
    for (size_t i = 0; i < icon_formats->size(); ++i) {
      icon_path = dir_iter->Append(subdir);
      icon_path = icon_path.Append(icon_name + (*icon_formats)[i]);
      if (file_util::PathExists(icon_path))
        return icon_path;
    }
  }
  return FilePath();
}

bool IconTheme::LoadIndexTheme(const FilePath& file) {
  FILE* fp = file_util::OpenFile(file, "r");
  SubDirInfo* current_info = NULL;
  if (!fp)
    return false;

  // Read entries.
  while (!feof(fp) && !ferror(fp)) {
    std::string buf = ReadLine(fp);
    if (buf == "")
      break;

    std::string entry;
    TrimWhitespaceASCII(buf, TRIM_ALL, &entry);
    if (entry.length() == 0 || entry[0] == '#') {
      // Blank line or Comment.
      continue;
    } else if (entry[0] == '[' && info_array_) {
      current_info = NULL;
      std::string subdir = entry.substr(1, entry.length() - 2);
      if (subdirs_.find(subdir) != subdirs_.end())
        current_info = &info_array_[subdirs_[subdir]];
    }

    std::string key, value;
    std::vector<std::string> r;
    SplitStringDontTrim(entry, '=', &r);
    if (r.size() < 2)
      continue;

    TrimWhitespaceASCII(r[0], TRIM_ALL, &key);
    for (size_t i = 1; i < r.size(); i++)
      value.append(r[i]);
    TrimWhitespaceASCII(value, TRIM_ALL, &value);

    if (current_info) {
      if (key == "Size") {
        current_info->size = atoi(value.c_str());
      } else if (key == "Type") {
        if (value == "Fixed")
          current_info->type = SubDirInfo::Fixed;
        else if (value == "Scalable")
          current_info->type = SubDirInfo::Scalable;
        else if (value == "Threshold")
          current_info->type = SubDirInfo::Threshold;
      } else if (key == "MaxSize") {
        current_info->max_size = atoi(value.c_str());
      } else if (key == "MinSize") {
        current_info->min_size = atoi(value.c_str());
      } else if (key == "Threshold") {
        current_info->threshold = atoi(value.c_str());
      }
    } else {
      if (key.compare("Directories") == 0 && !info_array_) {
        if (!SetDirectories(value)) break;
      } else if (key.compare("Inherits") == 0) {
        if (value != "hicolor")
          inherits_ = value;
      }
    }
  }

  file_util::CloseFile(fp);
  return info_array_ != NULL;
}

size_t IconTheme::MatchesSize(SubDirInfo* info, size_t size) {
  if (info->type == SubDirInfo::Fixed) {
    return size - info->size;
  } else if (info->type == SubDirInfo::Scalable) {
    if (size >= info->min_size && size <= info->max_size) {
      return 0;
    } else {
      return abs(size - info->min_size) < abs(size - info->max_size) ?
          (size - info->min_size) : (size - info->max_size);
    }
  } else {
    if (size >= info->size - info->threshold &&
        size <= info->size + info->threshold) {
      return 0;
    } else {
      return abs(size - info->size - info->threshold) <
          abs(size - info->size + info->threshold)
          ? size - info->size - info->threshold
          : size - info->size + info->threshold;
    }
  }
}

std::string IconTheme::ReadLine(FILE* fp) {
  if (!fp)
    return "";

  std::string result = "";
  const size_t kBufferSize = 100;
  char buffer[kBufferSize];
  while ((fgets(buffer, kBufferSize - 1, fp)) != NULL) {
    result += buffer;
    size_t len = result.length();
    if (len == 0)
      break;
    char end = result[len - 1];
    if (end == '\n' || end == '\0')
      break;
  }

  return result;
}

bool IconTheme::SetDirectories(const std::string& dirs) {
  int num = 0;
  std::string::size_type pos = 0, epos;
  std::string dir;
  while ((epos = dirs.find(',', pos)) != std::string::npos) {
    TrimWhitespaceASCII(dirs.substr(pos, epos - pos), TRIM_ALL, &dir);
    if (dir.length() == 0) {
      LOG(WARNING) << "Invalid index.theme: blank subdir";
      return false;
    }
    subdirs_[dir] = num++;
    pos = epos + 1;
  }
  TrimWhitespaceASCII(dirs.substr(pos), TRIM_ALL, &dir);
  if (dir.length() == 0) {
    LOG(WARNING) << "Invalid index.theme: blank subdir";
    return false;
  }
  subdirs_[dir] = num++;
  info_array_ = new SubDirInfo[num];
  return true;
}

// Make sure |dir| exists and add it to the list of icon directories.
void TryAddIconDir(const FilePath& dir) {
  if (!file_util::DirectoryExists(dir))
    return;
  (*Singleton<MimeUtilConstants>::get()->icon_dirs_)[dir] = 0;
}

// For a xdg directory |dir|, add the appropriate icon sub-directories.
void AddXDGDataDir(const FilePath& dir) {
  if (!file_util::DirectoryExists(dir))
    return;
  TryAddIconDir(dir.Append("icons"));
  TryAddIconDir(dir.Append("pixmaps"));
}

// Enable or disable SVG support.
void EnableSvgIcon(bool enable) {
  std::vector<std::string>* icon_formats =
      Singleton<MimeUtilConstants>::get()->icon_formats_;
  icon_formats->clear();
  icon_formats->push_back(".png");
  if (enable) {
    icon_formats->push_back(".svg");
    icon_formats->push_back(".svgz");
  }
  icon_formats->push_back(".xpm");
}

// Add all the xdg icon directories.
void InitIconDir() {
  Singleton<MimeUtilConstants>::get()->icon_dirs_->clear();
  const char* home = getenv("HOME");
  if (home) {
      FilePath legacy_data_dir(home);
      legacy_data_dir = legacy_data_dir.AppendASCII(".icons");
      if (file_util::DirectoryExists(legacy_data_dir))
        TryAddIconDir(legacy_data_dir);
  }
  const char* env = getenv("XDG_DATA_HOME");
  if (env) {
    AddXDGDataDir(FilePath(env));
  } else if (home) {
    FilePath local_data_dir(home);
    local_data_dir = local_data_dir.AppendASCII(".local");
    local_data_dir = local_data_dir.AppendASCII("share");
    AddXDGDataDir(local_data_dir);
  }

  env = getenv("XDG_DATA_DIRS");
  if (!env) {
    AddXDGDataDir(FilePath("/usr/local/share"));
    AddXDGDataDir(FilePath("/usr/share"));
  } else {
    std::string xdg_data_dirs = env;
    std::string::size_type pos = 0, epos;
    while ((epos = xdg_data_dirs.find(':', pos)) != std::string::npos) {
      AddXDGDataDir(FilePath(xdg_data_dirs.substr(pos, epos - pos)));
      pos = epos + 1;
    }
    AddXDGDataDir(FilePath(xdg_data_dirs.substr(pos)));
  }
}

// Per xdg theme spec, we should check the icon directories every so often for
// newly added icons. This isn't quite right.
void EnsureUpdated() {
  struct timeval t;
  gettimeofday(&t, NULL);
  time_t now = t.tv_sec;
  MimeUtilConstants* constants = Singleton<MimeUtilConstants>::get();

  if (constants->last_check_time_ == 0) {
    constants->icon_dirs_ = new std::map<FilePath, int>;
    constants->icon_themes_ = new std::map<std::string, IconTheme*>;
    constants->icon_formats_ = new std::vector<std::string>;
    EnableSvgIcon(kEnableSVG);
    InitIconDir();
    constants->last_check_time_ = now;
  } else {
    // TODO(thestig): something changed. start over. Upstream fix to Google
    // Gadgets for Linux.
    if (now > constants->last_check_time_ + constants->kUpdateInterval) {
    }
  }
}

// Find a fallback icon if we cannot find it in the default theme.
FilePath LookupFallbackIcon(const std::string& icon_name) {
  FilePath icon;
  MimeUtilConstants* constants = Singleton<MimeUtilConstants>::get();
  std::map<FilePath, int>::iterator iter;
  std::map<FilePath, int>* icon_dirs = constants->icon_dirs_;
  std::vector<std::string>* icon_formats = constants->icon_formats_;
  for (iter = icon_dirs->begin(); iter != icon_dirs->end(); ++iter) {
    for (size_t i = 0; i < icon_formats->size(); ++i) {
      icon = iter->first.Append(icon_name + (*icon_formats)[i]);
      if (file_util::PathExists(icon))
        return icon;
    }
  }
  return FilePath();
}

// Initialize the list of default themes.
void InitDefaultThemes() {
  IconTheme** default_themes =
      Singleton<MimeUtilConstants>::get()->default_themes_;

  char* env = getenv("KDE_FULL_SESSION");
  if (env) {
    // KDE
    std::string kde_default_theme;
    std::string kde_fallback_theme;

    // TODO(thestig): Figure out how to get the current icon theme on KDE.
    // Setting stored in ~/.kde/share/config/kdeglobals under Icons -> Theme.
    default_themes[0] = NULL;

    // Try some reasonable defaults for KDE.
    env = getenv("KDE_SESSION_VERSION");
    if (!env || env[0] != '4') {
      // KDE 3
      kde_default_theme = "default.kde";
      kde_fallback_theme = "crystalsvg";
    } else {
      // KDE 4
      kde_default_theme = "default.kde4";
      kde_fallback_theme = "oxygen";
    }
    default_themes[1] = IconTheme::LoadTheme(kde_default_theme);
    default_themes[2] = IconTheme::LoadTheme(kde_fallback_theme);
  } else {
    // Assume it's Gnome and use GTK to figure out the theme.
    gchar* gtk_theme_name;
    g_object_get(gtk_settings_get_default(),
                 "gtk-icon-theme-name",
                 &gtk_theme_name, NULL);
    default_themes[1] = IconTheme::LoadTheme(gtk_theme_name);
    default_themes[2] = IconTheme::LoadTheme("gnome");
  }
  // hicolor needs to be last per icon theme spec.
  default_themes[3] = IconTheme::LoadTheme("hicolor");

  for (size_t i = 0; i < MimeUtilConstants::kDefaultThemeNum; i++) {
    if (default_themes[i] == NULL)
      continue;
    // NULL out duplicate pointers.
    for (size_t j = i + 1; j < MimeUtilConstants::kDefaultThemeNum; j++) {
      if (default_themes[j] == default_themes[i])
        default_themes[j] = NULL;
    }
  }
}

// Try to find an icon with the name |icon_name| that's |size| pixels.
FilePath LookupIconInDefaultTheme(const std::string& icon_name, int size) {
  EnsureUpdated();
  MimeUtilConstants* constants = Singleton<MimeUtilConstants>::get();
  std::map<std::string, IconTheme*>* icon_themes = constants->icon_themes_;
  if (icon_themes->size() == 0)
    InitDefaultThemes();

  FilePath icon_path;
  IconTheme** default_themes = constants->default_themes_;
  for (size_t i = 0; i < MimeUtilConstants::kDefaultThemeNum; i++) {
    if (default_themes[i]) {
      icon_path = default_themes[i]->GetIconPath(icon_name, size, true);
      if (!icon_path.empty())
        return icon_path;
    }
  }
  return LookupFallbackIcon(icon_name);
}

MimeUtilConstants::~MimeUtilConstants() {
  delete icon_dirs_;
  delete icon_formats_;
  delete icon_themes_;
  for (size_t i = 0; i < kDefaultThemeNum; i++)
    delete default_themes_[i];
}

}  // namespace

namespace mime_util {

std::string GetFileMimeType(const FilePath& filepath) {
  return xdg_mime_get_mime_type_from_file_name(filepath.value().c_str());
}

std::string GetDataMimeType(const std::string& data) {
  return xdg_mime_get_mime_type_for_data(data.data(), data.length(), NULL);
}

FilePath GetMimeIcon(const std::string& mime_type, size_t size) {
  std::vector<std::string> icon_names;
  std::string icon_name;
  FilePath icon_file;

  const char* icon = xdg_mime_get_icon(mime_type.c_str());
  icon_name = std::string(icon ? icon : "");
  if (icon_name.length())
    icon_names.push_back(icon_name);

  // For text/plain, try text-plain.
  icon_name = mime_type;
  for (size_t i = icon_name.find('/', 0); i != std::string::npos;
       i = icon_name.find('/', i + 1)) {
    icon_name[i] = '-';
  }
  icon_names.push_back(icon_name);
  // Also try gnome-mime-text-plain.
  icon_names.push_back("gnome-mime-" + icon_name);

  // Try "deb" for "application/x-deb" in KDE 3.
  icon_name = mime_type.substr(mime_type.find("/x-") + 3);
  icon_names.push_back(icon_name);

  // Try generic name like text-x-generic.
  icon_name = mime_type.substr(0, mime_type.find('/')) + "-x-generic";
  icon_names.push_back(icon_name);

  // Last resort
  icon_names.push_back("unknown");

  for (size_t i = 0; i < icon_names.size(); i++) {
    if (icon_names[i][0] == '/') {
      icon_file = FilePath(icon_names[i]);
      if (file_util::PathExists(icon_file))
        return icon_file;
    } else {
      icon_file = LookupIconInDefaultTheme(icon_names[i], size);
      if (!icon_file.empty())
        return icon_file;
    }
  }
  return FilePath();
}

}  // namespace mime_util
