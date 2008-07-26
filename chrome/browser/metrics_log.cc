// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/metrics_log.h"

#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/md5.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/env_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/base64.h"

#define OPEN_ELEMENT_FOR_SCOPE(name) ScopedElement scoped_element(this, name)

// libxml take xmlChar*, which is unsigned char*
inline const unsigned char* UnsignedChar(const char* input) {
  return reinterpret_cast<const unsigned char*>(input);
}

// static
void MetricsLog::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterListPref(prefs::kStabilityPluginStats);
  local_state->RegisterBooleanPref(prefs::kMetricsIsRecording, true);
}

MetricsLog::MetricsLog(const std::string& client_id, int session_id)
    : start_time_(Time::Now()),
      num_events_(0),
      locked_(false),
      buffer_(NULL),
      writer_(NULL),
      client_id_(client_id),
      session_id_(IntToString(session_id)) {

  buffer_ = xmlBufferCreate();
  DCHECK(buffer_);

  writer_ = xmlNewTextWriterMemory(buffer_, 0);
  DCHECK(writer_);

  int result = xmlTextWriterSetIndent(writer_, 2);
  DCHECK_EQ(0, result);

  StartElement("log");
  WriteAttribute("clientid", client_id_);

  DCHECK_GE(result, 0);
}

MetricsLog::~MetricsLog() {
  if (writer_)
    xmlFreeTextWriter(writer_);

  if (buffer_)
    xmlBufferFree(buffer_);
}

void MetricsLog::CloseLog() {
  DCHECK(!locked_);
  locked_ = true;

  int result = xmlTextWriterEndDocument(writer_);
  DCHECK(result >= 0);

  result = xmlTextWriterFlush(writer_);
  DCHECK(result >= 0);
}

int MetricsLog::GetEncodedLogSize() {
  DCHECK(locked_);
  return buffer_->use;
}

bool MetricsLog::GetEncodedLog(char* buffer, int buffer_size) {
  DCHECK(locked_);
  if (buffer_size < GetEncodedLogSize())
    return false;

  memcpy(buffer, buffer_->content, GetEncodedLogSize());
  return true;
}

int MetricsLog::GetElapsedSeconds() {
  return static_cast<int>((Time::Now() - start_time_).InSeconds());
}

std::string MetricsLog::CreateHash(const std::string& value) {
  MD5Context ctx;
  MD5Init(&ctx);
  MD5Update(&ctx, value.data(), value.length());

  MD5Digest digest;
  MD5Final(&digest, &ctx);

  unsigned char reverse[8];  // UMA only uses first 8 chars of hash.
  DCHECK(arraysize(digest.a) >= arraysize(reverse));
  for (int i = 0; i < arraysize(reverse); ++i)
    reverse[i] = digest.a[arraysize(reverse) - i - 1];
  LOG(INFO) << "Metrics: Hash numeric [" << value << "]=["
    << *reinterpret_cast<const uint64*>(&reverse[0]) << "]";
  return std::string(reinterpret_cast<char*>(digest.a), arraysize(digest.a));
}

std::string MetricsLog::CreateBase64Hash(const std::string& string) {
  std::string encoded_digest;
  if (Base64Encode(CreateHash(string), &encoded_digest)) {
    DLOG(INFO) << "Metrics: Hash [" << encoded_digest << "]=[" << string << "]";
    return encoded_digest;
  } else {
    return std::string();
  }
}

void MetricsLog::RecordUserAction(const wchar_t* key) {
  DCHECK(!locked_);

  std::string command_hash = CreateBase64Hash(WideToUTF8(key));
  if (command_hash.empty()) {
    NOTREACHED() << "Unable generate encoded hash of command: " << key;
    return;
  }

  StartElement("uielement");
  WriteAttribute("action", "command");
  WriteAttribute("targetidhash", command_hash);

  // TODO(jhughes): Properly track windows.
  WriteIntAttribute("window", 0);
  WriteCommonEventAttributes();
  EndElement();

  ++num_events_;
}

void MetricsLog::RecordLoadEvent(int window_id,
                                 const GURL& url,
                                 PageTransition::Type origin,
                                 int session_index,
                                 TimeDelta load_time) {
  DCHECK(!locked_);

  StartElement("document");
  WriteAttribute("action", "load");
  WriteIntAttribute("docid", session_index);
  WriteIntAttribute("window", window_id);
  WriteAttribute("loadtime", Int64ToString(load_time.InMilliseconds()));

  std::string origin_string;

  switch (PageTransition::StripQualifier(origin)) {
    // TODO(jhughes): Some of these mappings aren't right... we need to add
    // some values to the server's enum.
    case PageTransition::LINK:
    case PageTransition::MANUAL_SUBFRAME:
      origin_string = "link";
      break;

    case PageTransition::TYPED:
      origin_string = "typed";
      break;

    case PageTransition::AUTO_BOOKMARK:
      origin_string = "bookmark";
      break;

    case PageTransition::AUTO_SUBFRAME:
    case PageTransition::RELOAD:
      origin_string = "refresh";
      break;

    case PageTransition::GENERATED:
      origin_string = "global-history";
      break;

    case PageTransition::START_PAGE:
      origin_string = "start-page";
      break;

    case PageTransition::FORM_SUBMIT:
      origin_string = "form-submit";
      break;

    default:
      NOTREACHED() << "Received an unknown page transition type: " <<
                       PageTransition::StripQualifier(origin);
  }
  if (!origin_string.empty())
    WriteAttribute("origin", origin_string);

  WriteCommonEventAttributes();
  EndElement();

  ++num_events_;
}

// static
const char* MetricsLog::WindowEventTypeToString(WindowEventType type) {
  switch (type) {
    case WINDOW_CREATE:
      return "create";

    case WINDOW_OPEN:
      return "open";

    case WINDOW_CLOSE:
      return "close";

    case WINDOW_DESTROY:
      return "destroy";

    default:
      NOTREACHED();
      return "unknown";
  }
}

void MetricsLog::RecordWindowEvent(WindowEventType type,
                                   int window_id,
                                   int parent_id) {
  DCHECK(!locked_);

  StartElement("window");
  WriteAttribute("action", WindowEventTypeToString(type));
  WriteAttribute("windowid", IntToString(window_id));
  if (parent_id >= 0)
    WriteAttribute("parent", IntToString(parent_id));
  WriteCommonEventAttributes();
  EndElement();

  ++num_events_;
}

std::string MetricsLog::GetCurrentTimeString() {
  return Uint64ToString(Time::Now().ToTimeT());
}

// These are the attributes that are common to every event.
void MetricsLog::WriteCommonEventAttributes() {
  WriteAttribute("session", session_id_);
  WriteAttribute("time", GetCurrentTimeString());
}

void MetricsLog::WriteAttribute(const std::string& name,
                                const std::string& value) {
  DCHECK(!locked_);
  DCHECK(!name.empty());

  int result = xmlTextWriterWriteAttribute(writer_,
                                           UnsignedChar(name.c_str()),
                                           UnsignedChar(value.c_str()));
  DCHECK_GE(result, 0);
}

void MetricsLog::WriteIntAttribute(const std::string& name, int value) {
  WriteAttribute(name, IntToString(value));
}

void MetricsLog::WriteInt64Attribute(const std::string& name, int64 value) {
  WriteAttribute(name, Int64ToString(value));
}

void MetricsLog::StartElement(const char* name) {
  DCHECK(!locked_);
  DCHECK(name);

  int result = xmlTextWriterStartElement(writer_, UnsignedChar(name));
  DCHECK_GE(result, 0);
}

void MetricsLog::EndElement() {
  DCHECK(!locked_);

  int result = xmlTextWriterEndElement(writer_);
  DCHECK_GE(result, 0);
}

std::string MetricsLog::GetVersionString() const {
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info.get()) {
    std::string version = WideToUTF8(version_info->product_version());
    if (!version_info->is_official_build())
      version.append("-devel");
    return version;
  } else {
    NOTREACHED() << "Unable to retrieve version string.";
  }

  return std::string();
}

std::string MetricsLog::GetInstallDate() const {
  PrefService* pref = g_browser_process->local_state();
  if (pref) {
    return WideToUTF8(pref->GetString(prefs::kMetricsClientIDTimestamp));
  } else {
    NOTREACHED();
    return "0";
  }
}

void MetricsLog::WriteStabilityElement() {
  DCHECK(!locked_);

  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);

  // Get stability attributes out of Local State, zeroing out stored values.
  // NOTE: This could lead to some data loss if this report isn't successfully
  //       sent, but that's true for all the metrics.

  StartElement("stability");

  WriteIntAttribute("launchcount",
                    pref->GetInteger(prefs::kStabilityLaunchCount));
  pref->SetInteger(prefs::kStabilityLaunchCount, 0);
  WriteIntAttribute("crashcount",
                    pref->GetInteger(prefs::kStabilityCrashCount));
  pref->SetInteger(prefs::kStabilityCrashCount, 0);
  WriteIntAttribute("pageloadcount",
                    pref->GetInteger(prefs::kStabilityPageLoadCount));
  pref->SetInteger(prefs::kStabilityPageLoadCount, 0);
  WriteIntAttribute("renderercrashcount",
                    pref->GetInteger(prefs::kStabilityRendererCrashCount));
  pref->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  WriteIntAttribute("rendererhangcount",
                    pref->GetInteger(prefs::kStabilityRendererHangCount));
  pref->SetInteger(prefs::kStabilityRendererHangCount, 0);

  // Uptime is stored as a string, since there's no int64 in Value/JSON.
  WriteAttribute("uptimesec",
                 WideToUTF8(pref->GetString(prefs::kStabilityUptimeSec)));
  pref->SetString(prefs::kStabilityUptimeSec, L"0");

  // Now log plugin stability info
  const ListValue* plugin_stats_list = pref->GetList(
      prefs::kStabilityPluginStats);
  if (plugin_stats_list) {
    StartElement("plugins");
    for (ListValue::const_iterator iter = plugin_stats_list->begin();
         iter != plugin_stats_list->end(); ++iter) {
      if (!(*iter)->IsType(Value::TYPE_DICTIONARY)) {
        NOTREACHED();
        continue;
      }
      DictionaryValue* plugin_dict = static_cast<DictionaryValue*>(*iter);

      std::wstring plugin_path;
      plugin_dict->GetString(prefs::kStabilityPluginPath, &plugin_path);
      plugin_path = file_util::GetFilenameFromPath(plugin_path);
      if (plugin_path.empty()) {
        NOTREACHED();
        continue;
      }

      StartElement("pluginstability");
      WriteAttribute("filename", CreateBase64Hash(WideToUTF8(plugin_path)));

      int launches = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginLaunches, &launches);
      WriteIntAttribute("launchcount", launches);

      int instances = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginInstances, &instances);
      WriteIntAttribute("instancecount", instances);

      int crashes = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginCrashes, &crashes);
      WriteIntAttribute("crashcount", crashes);
      EndElement();
    }

    pref->ClearPref(prefs::kStabilityPluginStats);
    EndElement();
  }

  EndElement();
}

void MetricsLog::WritePluginList(
         const std::vector<WebPluginInfo>& plugin_list) {
  DCHECK(!locked_);

  StartElement("plugins");

  for (std::vector<WebPluginInfo>::const_iterator iter = plugin_list.begin();
       iter != plugin_list.end(); ++iter) {
    StartElement("plugin");

    // Plugin name and filename are hashed for the privacy of those
    // testing unreleased new extensions.
    WriteAttribute("name", CreateBase64Hash(WideToUTF8((*iter).name)));
    std::wstring filename = file_util::GetFilenameFromPath((*iter).file);
    WriteAttribute("filename", CreateBase64Hash(WideToUTF8(filename)));

    WriteAttribute("version", WideToUTF8((*iter).version));

    EndElement();
  }

  EndElement();
}

void MetricsLog::RecordEnvironment(
         const std::vector<WebPluginInfo>& plugin_list,
         const DictionaryValue* profile_metrics) {
  DCHECK(!locked_);

  PrefService* pref = g_browser_process->local_state();

  StartElement("profile");
  WriteCommonEventAttributes();

  StartElement("install");
  WriteAttribute("installdate", GetInstallDate());
  WriteIntAttribute("buildid", 0);  // means that we're using appversion instead
  WriteAttribute("appversion", GetVersionString());
  EndElement();

  WritePluginList(plugin_list);

  WriteStabilityElement();

  {
    OPEN_ELEMENT_FOR_SCOPE("cpu");
    WriteAttribute("arch", env_util::GetCPUArchitecture());
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("security");
    WriteIntAttribute("rendereronsboxdesktop",
                      pref->GetInteger(prefs::kSecurityRendererOnSboxDesktop));
    pref->SetInteger(prefs::kSecurityRendererOnSboxDesktop, 0);

    WriteIntAttribute("rendererondefaultdesktop",
        pref->GetInteger(prefs::kSecurityRendererOnDefaultDesktop));
    pref->SetInteger(prefs::kSecurityRendererOnDefaultDesktop, 0);
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("memory");
    WriteIntAttribute("mb", env_util::GetPhysicalMemoryMB());
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("os");
    WriteAttribute("name",
                   env_util::GetOperatingSystemName());
    WriteAttribute("version",
                   env_util::GetOperatingSystemVersion());
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("display");
    int width = 0;
    int height = 0;
    env_util::GetPrimaryDisplayDimensions(&width, &height);
    WriteIntAttribute("xsize", width);
    WriteIntAttribute("ysize", height);
    WriteIntAttribute("screens", env_util::GetDisplayCount());
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("bookmarks");
    int num_bookmarks_on_bookmark_bar =
        pref->GetInteger(prefs::kNumBookmarksOnBookmarkBar);
    int num_folders_on_bookmark_bar =
        pref->GetInteger(prefs::kNumFoldersOnBookmarkBar);
    int num_bookmarks_in_other_bookmarks_folder =
        pref->GetInteger(prefs::kNumBookmarksInOtherBookmarkFolder);
    int num_folders_in_other_bookmarks_folder =
        pref->GetInteger(prefs::kNumFoldersInOtherBookmarkFolder);
    {
      OPEN_ELEMENT_FOR_SCOPE("bookmarklocation");
      WriteAttribute("name", "full-tree");
      WriteIntAttribute("foldercount",
          num_folders_on_bookmark_bar + num_folders_in_other_bookmarks_folder);
      WriteIntAttribute("itemcount",
          num_bookmarks_on_bookmark_bar +
          num_bookmarks_in_other_bookmarks_folder);
    }
    {
      OPEN_ELEMENT_FOR_SCOPE("bookmarklocation");
      WriteAttribute("name", "toolbar");
      WriteIntAttribute("foldercount", num_folders_on_bookmark_bar);
      WriteIntAttribute("itemcount", num_bookmarks_on_bookmark_bar);
    }
  }

  {
    OPEN_ELEMENT_FOR_SCOPE("keywords");
    WriteIntAttribute("count", pref->GetInteger(prefs::kNumKeywords));
  }

  if (profile_metrics)
    WriteAllProfilesMetrics(*profile_metrics);

  EndElement();  // profile
}

void MetricsLog::WriteAllProfilesMetrics(
    const DictionaryValue& all_profiles_metrics) {
  const std::wstring profile_prefix(prefs::kProfilePrefix);
  for (DictionaryValue::key_iterator i = all_profiles_metrics.begin_keys();
       i != all_profiles_metrics.end_keys(); ++i) {
    const std::wstring& key_name = *i;
    if (key_name.compare(0, profile_prefix.size(), profile_prefix) == 0) {
      DictionaryValue* profile;
      if (all_profiles_metrics.GetDictionary(key_name, &profile))
        WriteProfileMetrics(key_name.substr(profile_prefix.size()), *profile);
    }
  }
}

void MetricsLog::WriteProfileMetrics(const std::wstring& profileidhash,
                                     const DictionaryValue& profile_metrics) {
  OPEN_ELEMENT_FOR_SCOPE("userprofile");
  WriteAttribute("profileidhash", WideToUTF8(profileidhash));
  for (DictionaryValue::key_iterator i = profile_metrics.begin_keys();
       i != profile_metrics.end_keys(); ++i) {
    Value* value;
    if (profile_metrics.Get(*i, &value)) {
      DCHECK(*i != L"id");
      switch (value->GetType()) {
        case Value::TYPE_STRING: {
          std::wstring string_value;
          if (value->GetAsString(&string_value)) {
            OPEN_ELEMENT_FOR_SCOPE("profileparam");
            WriteAttribute("name", WideToUTF8(*i));
            WriteAttribute("value", WideToUTF8(string_value));
          }
          break;
        }

        case Value::TYPE_BOOLEAN: {
          bool bool_value;
          if (value->GetAsBoolean(&bool_value)) {
            OPEN_ELEMENT_FOR_SCOPE("profileparam");
            WriteAttribute("name", WideToUTF8(*i));
            WriteIntAttribute("value", bool_value ? 1 : 0);
          }
          break;
        }

        case Value::TYPE_INTEGER: {
          int int_value;
          if (value->GetAsInteger(&int_value)) {
            OPEN_ELEMENT_FOR_SCOPE("profileparam");
            WriteAttribute("name", WideToUTF8(*i));
            WriteIntAttribute("value", int_value);
          }
          break;
        }

        default:
          NOTREACHED();
          break;
      }
    }
  }
}

void MetricsLog::RecordOmniboxOpenedURL(const AutocompleteLog& log) {
  DCHECK(!locked_);

  StartElement("uielement");
  WriteAttribute("action", "autocomplete");
  WriteAttribute("targetidhash", "");
  // TODO(kochi): Properly track windows.
  WriteIntAttribute("window", 0);
  WriteCommonEventAttributes();

  StartElement("autocomplete");

  WriteIntAttribute("typedlength", static_cast<int>(log.text.length()));
  WriteIntAttribute("completedlength",
                    static_cast<int>(log.inline_autocompleted_length));
  WriteIntAttribute("selectedindex", static_cast<int>(log.selected_index));

  for (AutocompleteResult::const_iterator i(log.result.begin());
       i != log.result.end(); ++i) {
    StartElement("autocompleteitem");
    if (i->provider)
      WriteAttribute("provider", i->provider->name());
    WriteIntAttribute("relevance", i->relevance);
    WriteIntAttribute("isstarred", i->starred ? 1 : 0);
    EndElement();  // autocompleteitem
  }
  EndElement();  // autocomplete
  EndElement();  // uielement

  ++num_events_;
}

// TODO(JAR): A The following should really be part of the histogram class.
// Internal state is being needlessly exposed, and it would be hard to reuse
// this code. If we moved this into the Histogram class, then we could use
// the same infrastructure for logging StatsCounters, RatesCounters, etc.
void MetricsLog::RecordHistogramDelta(const Histogram& histogram,
                                      const Histogram::SampleSet& snapshot) {
  DCHECK(!locked_);
  DCHECK(0 != snapshot.TotalCount());
  snapshot.CheckSize(histogram);

  // We will ignore the MAX_INT/infinite value in the last element of range[].

  OPEN_ELEMENT_FOR_SCOPE("histogram");

  WriteAttribute("name", CreateBase64Hash(histogram.histogram_name()));

  WriteInt64Attribute("sum", snapshot.sum());
  WriteInt64Attribute("sumsquares", snapshot.square_sum());

  for (size_t i = 0; i < histogram.bucket_count(); i++) {
    if (snapshot.counts(i)) {
      OPEN_ELEMENT_FOR_SCOPE("histogrambucket");
      WriteIntAttribute("min", histogram.ranges(i));
      WriteIntAttribute("max", histogram.ranges(i + 1));
      WriteIntAttribute("count", snapshot.counts(i));
    }
  }
}
