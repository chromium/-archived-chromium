// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_updater.h"

#include "base/logging.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/libxml_utils.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "libxml/tree.h"

const char* ExtensionUpdater::kExpectedGupdateProtocol = "2.0";
const char* ExtensionUpdater::kExpectedGupdateXmlns =
    "http://www.google.com/update2/response";

// For sanity checking on update frequency - enforced in release mode only.
static const int kMinUpdateFrequencySeconds = 60 * 60;  // 1 hour
static const int kMaxUpdateFrequencySeconds = 60 * 60 * 24 * 7;  // 7 days

// A utility class to do file handling on the file I/O thread.
class ExtensionUpdaterFileHandler
    : public base::RefCountedThreadSafe<ExtensionUpdaterFileHandler> {
 public:
  ExtensionUpdaterFileHandler(MessageLoop* updater_loop,
                              MessageLoop* file_io_loop)
      : updater_loop_(updater_loop), file_io_loop_(file_io_loop) {}

  // Writes crx file data into a tempfile, and calls back the updater.
  void WriteTempFile(const std::string& extension_id, const std::string& data,
                     scoped_refptr<ExtensionUpdater> updater) {
    // Make sure we're running in the right thread.
    DCHECK(MessageLoop::current() == file_io_loop_);

    FilePath path;
    if (!file_util::CreateTemporaryFileName(&path)) {
      LOG(WARNING) << "Failed to create temporary file path";
      return;
    }
    if (file_util::WriteFile(path, data.c_str(), data.length()) !=
        static_cast<int>(data.length())) {
      // TODO(asargent) - It would be nice to back off updating alltogether if
      // the disk is full. (http://crbug.com/12763).
      LOG(ERROR) << "Failed to write temporary file";
      file_util::Delete(path, false);
      return;
    }

    // The ExtensionUpdater is now responsible for cleaning up the temp file
    // from disk.
    updater_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        updater.get(), &ExtensionUpdater::OnCRXFileWritten, extension_id,
        path));
  }

  void DeleteFile(const FilePath& path) {
    DCHECK(MessageLoop::current() == file_io_loop_);
    if (!file_util::Delete(path, false)) {
      LOG(WARNING) << "Failed to delete temp file " << path.value();
    }
  }

 private:
  // The MessageLoop we use to call back the ExtensionUpdater.
  MessageLoop* updater_loop_;

  // The MessageLoop we should be operating on for file operations.
  MessageLoop* file_io_loop_;
};


ExtensionUpdater::ExtensionUpdater(ExtensionUpdateService* service,
                                   int frequency_seconds,
                                   MessageLoop* file_io_loop)
    : service_(service), frequency_seconds_(frequency_seconds),
      file_io_loop_(file_io_loop),
      file_handler_(new ExtensionUpdaterFileHandler(MessageLoop::current(),
                                                    file_io_loop_)) {
  Init();
}

void ExtensionUpdater::Init() {
  // Unless we're in a unit test, expect that the file_io_loop_ is on the
  // browser file thread.
  if (g_browser_process->file_thread() != NULL) {
    DCHECK(file_io_loop_ == g_browser_process->file_thread()->message_loop());
  }

  DCHECK_GE(frequency_seconds_, 5);
  DCHECK(frequency_seconds_ <= kMaxUpdateFrequencySeconds);
#ifdef NDEBUG
  // In Release mode we enforce that update checks don't happen too often.
  frequency_seconds_ = std::max(frequency_seconds_, kMinUpdateFrequencySeconds);
#endif
  frequency_seconds_ = std::min(frequency_seconds_, kMaxUpdateFrequencySeconds);
}

ExtensionUpdater::~ExtensionUpdater() {}

void ExtensionUpdater::Start() {
  // TODO(asargent) Make sure update schedules work across browser restarts by
  // reading/writing update frequency in profile/settings. *But*, make sure to
  // wait a reasonable amount of time after browser startup to do the first
  // check during a given run of the browser. Also remember to update the header
  // comments when this is implemented. (http://crbug.com/12545).
  timer_.Start(base::TimeDelta::FromSeconds(frequency_seconds_), this,
      &ExtensionUpdater::TimerFired);
}

void ExtensionUpdater::Stop() {
  timer_.Stop();
  manifest_fetcher_.reset();
  extension_fetcher_.reset();
  manifests_pending_.clear();
  extensions_pending_.clear();
}

void ExtensionUpdater::OnURLFetchComplete(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {

  if (source == manifest_fetcher_.get()) {
    OnManifestFetchComplete(url, status, response_code, data);
  } else if (source == extension_fetcher_.get()) {
    OnCRXFetchComplete(url, status, response_code, data);
  } else {
    NOTREACHED();
  }
}

void ExtensionUpdater::OnManifestFetchComplete(const GURL& url,
                                               const URLRequestStatus& status,
                                               int response_code,
                                               const std::string& data) {
  // We want to try parsing the manifest, and if it indicates updates are
  // available, we want to fire off requests to fetch those updates.
  if (status.status() == URLRequestStatus::SUCCESS && response_code == 200) {
    ScopedVector<ParseResult> parsed;
    // TODO(asargent) - We should do the xml parsing in a sandboxed process.
    // (http://crbug.com/12677).
    if (Parse(data, &parsed.get())) {
      std::vector<int> updates = DetermineUpdates(parsed.get());
      for (size_t i = 0; i < updates.size(); i++) {
        ParseResult* update = parsed[updates[i]];
        FetchUpdatedExtension(update->extension_id, update->crx_url);
      }
    }
  } else {
    // TODO(asargent) Do exponential backoff here. (http://crbug.com/12546).
    LOG(INFO) << "Failed to fetch manifst '" << url.possibly_invalid_spec() <<
        "' response code:" << response_code;
  }
  manifest_fetcher_.reset();

  // If we have any pending manifest requests, fire off the next one.
  if (!manifests_pending_.empty()) {
    GURL url = manifests_pending_.front();
    manifests_pending_.pop_front();
    StartUpdateCheck(url);
  }
}

void ExtensionUpdater::OnCRXFetchComplete(const GURL& url,
                                          const URLRequestStatus& status,
                                          int response_code,
                                          const std::string& data) {
  if (url != current_extension_fetch_.url) {
    LOG(ERROR) << "Called with unexpected url:'" << url.spec()
               << "' expected:'" << current_extension_fetch_.url.spec() << "'";
    NOTREACHED();
  } else if (status.status() == URLRequestStatus::SUCCESS &&
             response_code == 200) {
    // Successfully fetched - now write crx to a file so we can have the
    // ExtensionsService install it.
    file_io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      file_handler_.get(), &ExtensionUpdaterFileHandler::WriteTempFile,
      current_extension_fetch_.id, data, this));
  } else {
    // TODO(asargent) do things like exponential backoff, handling
    // 503 Service Unavailable / Retry-After headers, etc. here.
    // (http://crbug.com/12546).
    LOG(INFO) << "Failed to fetch extension '" <<
        url.possibly_invalid_spec() << "' response code:" << response_code;
  }
  extension_fetcher_.reset();
  current_extension_fetch_ = ExtensionFetch();

  // If there are any pending downloads left, start one.
  if (extensions_pending_.size() > 0) {
    ExtensionFetch next = extensions_pending_.front();
    extensions_pending_.pop_front();
    FetchUpdatedExtension(next.id, next.url);
  }
}

void ExtensionUpdater::OnCRXFileWritten(const std::string& id,
                                        const FilePath& path) {
  // TODO(asargent) - Instead of calling InstallExtension here, we should have
  // an UpdateExtension method in ExtensionsService and rely on it to check
  // that the extension is still installed, and still an older version than
  // what we're handing it.
  // (http://crbug.com/12764).
  ExtensionInstallCallback* callback =
      NewCallback(this, &ExtensionUpdater::OnExtensionInstallFinished);
  service_->UpdateExtension(id, path, false, callback);
}

void ExtensionUpdater::OnExtensionInstallFinished(const FilePath& path,
                                                  Extension* extension) {
  // Have the file_handler_ delete the temp file on the file I/O thread.
  file_io_loop_->PostTask(FROM_HERE, NewRunnableMethod(
    file_handler_.get(), &ExtensionUpdaterFileHandler::DeleteFile, path));
}

static const char* kURLEncodedEquals = "%3D";  // '='
static const char* kURLEncodedAnd = "%26";  // '&'

void ExtensionUpdater::TimerFired() {
  // Generate a set of update urls for loaded extensions.
  std::set<GURL> urls;
  const ExtensionList* extensions = service_->extensions();
  for (ExtensionList::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    Extension* extension = (*iter);
    const GURL& update_url = extension->update_url();
    if (update_url.is_empty() || extension->id().empty()) {
      continue;
    }

    DCHECK(update_url.is_valid());
    DCHECK(!update_url.has_ref());

    // Append extension information to the url.
    std::string full_url_string = update_url.spec();
    full_url_string.append(update_url.has_query() ? "&" : "?");
    full_url_string.append("x=");

    full_url_string.append("id");
    full_url_string.append(kURLEncodedEquals);
    full_url_string.append(extension->id());

    const Version* version = extension->version();
    DCHECK(version);
    full_url_string.append("v");
    full_url_string.append(kURLEncodedEquals);
    full_url_string.append(version->GetString());
    full_url_string.append(kURLEncodedAnd);

    GURL full_url(full_url_string);
    if (!full_url.is_valid()) {
      LOG(ERROR) << "invalid url: " << full_url.possibly_invalid_spec();
      NOTREACHED();
    } else {
      urls.insert(full_url);
    }
  }

  // Now do an update check for each url we found.
  for (std::set<GURL>::iterator iter = urls.begin(); iter != urls.end();
       ++iter) {
    // StartUpdateCheck makes sure the url isn't already downloading or
    // scheduled, so we don't need to check before calling it.
    StartUpdateCheck(*iter);
  }
  timer_.Reset();
}

static void ManifestParseError(const char* details, ...) {
  va_list args;
  va_start(args, details);
  std::string message("Extension update manifest parse error: ");
  StringAppendV(&message, details, args);
  ExtensionErrorReporter::GetInstance()->ReportError(message, false);
}

// Checks whether a given node's name matches |expected_name| and
// |expected_namespace|.
static bool TagNameEquals(const xmlNode* node, const char* expected_name,
                          const xmlNs* expected_namespace) {
  if (node->ns != expected_namespace) {
    return false;
  }
  return 0 == strcmp(expected_name, reinterpret_cast<const char*>(node->name));
}

// Returns child nodes of |root| with name |name| in namespace |xml_namespace|.
static std::vector<xmlNode*> GetChildren(xmlNode* root, xmlNs* xml_namespace,
                                         const char* name) {
  std::vector<xmlNode*> result;
  for (xmlNode* child = root->children; child != NULL; child = child->next) {
    if (!TagNameEquals(child, name, xml_namespace)) {
      continue;
    }
    result.push_back(child);
  }
  return result;
}

// Returns the value of a named attribute, or the empty string.
static std::string GetAttribute(xmlNode* node, const char* attribute_name) {
  const xmlChar* name = reinterpret_cast<const xmlChar*>(attribute_name);
  for (xmlAttr* attr = node->properties; attr != NULL; attr = attr->next) {
    if (!xmlStrcmp(attr->name, name) && attr->children &&
        attr->children->content) {
      return std::string(reinterpret_cast<const char*>(
          attr->children->content));
    }
  }
  return std::string();
}

// This is used for the xml parser to report errors. This assumes the context
// is a pointer to a std::string where the error message should be appended.
static void XmlErrorFunc(void *context, const char *message, ...) {
  va_list args;
  va_start(args, message);
  std::string* error = static_cast<std::string*>(context);
  StringAppendV(error, message, args);
}

// Utility class for cleaning up xml parser state when leaving a scope.
class ScopedXmlParserCleanup {
 public:
  ScopedXmlParserCleanup() : document_(NULL) {}
  ~ScopedXmlParserCleanup() {
    if (document_)
      xmlFreeDoc(document_);
    xmlCleanupParser();
  }
  void set_document(xmlDocPtr document) {
    document_ = document;
  }

 private:
  xmlDocPtr document_;
};

// Returns a pointer to the xmlNs on |node| with the |expected_href|, or
// NULL if there isn't one with that href.
static xmlNs* GetNamespace(xmlNode* node, const char* expected_href) {
  const xmlChar* href = reinterpret_cast<const xmlChar*>(expected_href);
  for (xmlNs* ns = node->ns; ns != NULL; ns = ns->next) {
    if (ns->href && !xmlStrcmp(ns->href, href)) {
      return ns;
    }
  }
  return NULL;
}

// This is a separate sub-class so that we can have access to the private
// ParseResult struct, but avoid making the .h file include the xml api headers.
class ExtensionUpdater::ParseHelper {
 public:
  // Helper function for ExtensionUpdater::Parse that reads in values for a
  // single <app> tag. It returns a boolean indicating success or failure.
  static bool ParseSingleAppTag(xmlNode* app_node, xmlNs* xml_namespace,
                                ParseResult* result) {
    // Read the extension id.
    result->extension_id = GetAttribute(app_node, "appid");
    if (result->extension_id.length() == 0) {
      ManifestParseError("Missing appid on app node");
      return false;
    }

    // Get the updatecheck node.
    std::vector<xmlNode*> updates = GetChildren(app_node, xml_namespace,
                                                "updatecheck");
    if (updates.size() > 1) {
      ManifestParseError("Too many updatecheck tags on app (expecting only 1)");
      return false;
    }
    if (updates.size() == 0) {
      ManifestParseError("Missing updatecheck on app");
      return false;
    }
    xmlNode *updatecheck = updates[0];

    // Find the url to the crx file.
    result->crx_url = GURL(GetAttribute(updatecheck, "codebase"));
    if (!result->crx_url.is_valid()) {
      ManifestParseError("Invalid codebase url");
      return false;
    }

    // Get the version.
    std::string tmp = GetAttribute(updatecheck, "version");
    if (tmp.length() == 0) {
      ManifestParseError("Missing version for updatecheck");
      return false;
    }
    result->version.reset(Version::GetVersionFromString(tmp));
    if (!result->version.get()) {
      ManifestParseError("Invalid version");
      return false;
    }

    // Get the minimum browser version (not required).
    tmp = GetAttribute(updatecheck, "prodversionmin");
    if (tmp.length()) {
      result->browser_min_version.reset(Version::GetVersionFromString(tmp));
      if (!result->browser_min_version.get()) {
        ManifestParseError("Invalid prodversionmin");
        return false;
      }
    }
    return true;
  }
};

bool ExtensionUpdater::Parse(const std::string& manifest_xml,
                                ParseResultList* results) {
  std::string xml_errors;
  ScopedXmlErrorFunc error_func(&xml_errors, &XmlErrorFunc);
  ScopedXmlParserCleanup xml_cleanup;

  xmlDocPtr document = xmlParseDoc(
      reinterpret_cast<const xmlChar*>(manifest_xml.c_str()));
  if (!document) {
    ManifestParseError(xml_errors.c_str());
    return false;
  }
  xml_cleanup.set_document(document);

  xmlNode *root = xmlDocGetRootElement(document);
  if (!root) {
    ManifestParseError("Missing root node");
    return false;
  }

  // Look for the required namespace declaration.
  xmlNs* gupdate_ns = GetNamespace(root, kExpectedGupdateXmlns);
  if (!gupdate_ns) {
    ManifestParseError("Missing or incorrect xmlns on gupdate tag");
    return false;
  }

  if (!TagNameEquals(root, "gupdate", gupdate_ns)) {
    ManifestParseError("Missing gupdate tag");
    return false;
  }

  // Check for the gupdate "protocol" attribute.
  if (GetAttribute(root, "protocol") != kExpectedGupdateProtocol) {
    ManifestParseError("Missing/incorrect protocol on gupdate tag "
        "(expected '%s')", kExpectedGupdateProtocol);
    return false;
  }

  // Parse each of the <app> tags.
  ScopedVector<ParseResult> tmp_results;
  std::vector<xmlNode*> apps = GetChildren(root, gupdate_ns, "app");
  for (unsigned int i = 0; i < apps.size(); i++) {
    ParseResult* current = new ParseResult();
    tmp_results.push_back(current);
    if (!ParseHelper::ParseSingleAppTag(apps[i], gupdate_ns, current)) {
      return false;
    }
  }
  results->insert(results->end(), tmp_results.begin(), tmp_results.end());
  tmp_results.get().clear();

  return true;
}

std::vector<int> ExtensionUpdater::DetermineUpdates(
    const ParseResultList& possible_updates) {

  std::vector<int> result;

  // This will only get set if one of possible_updates specifies
  // browser_min_version.
  scoped_ptr<Version> browser_version;

  for (size_t i = 0; i < possible_updates.size(); i++) {
    ParseResult* update = possible_updates[i];

    Extension* extension = service_->GetExtensionById(update->extension_id);
    if (!extension)
      continue;

    // If the update version is the same or older than what's already installed,
    // we don't want it.
    if (update->version.get()->CompareTo(*(extension->version())) <= 0)
      continue;

    // If the update specifies a browser minimum version, do we qualify?
    if (update->browser_min_version.get()) {
      // First determine the browser version if we haven't already.
      if (!browser_version.get()) {
        scoped_ptr<FileVersionInfo> version_info(
            FileVersionInfo::CreateFileVersionInfoForCurrentModule());
        if (version_info.get()) {
          browser_version.reset(Version::GetVersionFromString(
              version_info->product_version()));
        }
      }
      if (browser_version.get() &&
          update->browser_min_version->CompareTo(*browser_version.get()) > 0) {
        // TODO(asargent) - We may want this to show up in the extensions UI
        // eventually. (http://crbug.com/12547).
        LOG(WARNING) << "Updated version of extension " << extension->id() <<
            " available, but requires chrome version " <<
            update->browser_min_version->GetString();
        continue;
      }
    }
    result.push_back(i);
  }
  return result;
}

void ExtensionUpdater::StartUpdateCheck(const GURL& url) {
  if (std::find(manifests_pending_.begin(), manifests_pending_.end(), url) !=
      manifests_pending_.end()) {
    return;  // already scheduled
  }

  if (manifest_fetcher_.get() != NULL) {
    if (manifest_fetcher_->url() != url) {
      manifests_pending_.push_back(url);
    }
  } else {
    manifest_fetcher_.reset(
        URLFetcher::Create(kManifestFetcherId, url, URLFetcher::GET, this));
    manifest_fetcher_->set_request_context(Profile::GetDefaultRequestContext());
    manifest_fetcher_->Start();
  }
}

void ExtensionUpdater::FetchUpdatedExtension(const std::string& id, GURL url) {
  for (std::deque<ExtensionFetch>::const_iterator iter =
           extensions_pending_.begin();
       iter != extensions_pending_.end(); ++iter) {
    if (iter->id == id || iter->url == url) {
      return;  // already scheduled
    }
  }

  if (extension_fetcher_.get() != NULL) {
    if (extension_fetcher_->url() != url) {
      extensions_pending_.push_back(ExtensionFetch(id, url));
    }
  } else {
    extension_fetcher_.reset(
        URLFetcher::Create(kExtensionFetcherId, url, URLFetcher::GET, this));
    extension_fetcher_->set_request_context(
        Profile::GetDefaultRequestContext());
    extension_fetcher_->Start();
    current_extension_fetch_ = ExtensionFetch(id, url);
  }
}
