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

#include <io.h>
#include <vector>

#include "chrome/browser/spellchecker.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/win_util.h"
#include "chrome/app/locales/locale_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/url_fetcher.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/win_util.h"
#include "chrome/third_party/hunspell/src/hunspell/hunspell.hxx"
#include "net/url_request/url_request.h"

#include "generated_resources.h"

static const int kMaxSuggestions = 5;  // Max number of dictionary suggestions.

// ############################################################################
// This part of the spellchecker code is used for downloading spellchecking
// dictionary if required. This code is included in this file since dictionary
// is an integral part of spellchecker.

// Design: The spellchecker initializes hunspell_ in the |Initialize()| method.
// This is done using the dictionary file on disk, for example, "en-US.bdic".
// If this file is missing, a |DictionaryDownloadController| object is used to
// download the missing files asynchronously (using URLFetcher) in the file
// thread. Initialization of hunspell_ is held off during this process. After
// the dictionary downloads (or fails to download), corresponding flags are set
// in spellchecker - in the IO thread. Since IO thread goes first during closing
// of browser, a proxy task |UIProxyForIOTask| is created in the UI thread,
// which obtains the IO thread independently and invokes the task in the IO
// thread if it's not NULL. After the flags are cleared, a (final) attempt is
// made to initialize hunspell_. If it fails even then (dictionary could not
// download), no more attempts are made to initialize it.

// TODO(sidchat): Implement options to download dictionary as zip files or
// mini installer

// ############################################################################

// This object downloads the dictionary files asynchronously by first
// fetching it to memory using URL fetcher and then writing it to
// disk using file_util::WriteFile.
class SpellChecker::DictionaryDownloadController
    : public URLFetcher::Delegate,
      public base::RefCountedThreadSafe<DictionaryDownloadController> {
 public:
  DictionaryDownloadController(
      Task* spellchecker_flag_set_task,
      const std::wstring& dic_file_path,
      URLRequestContext* url_request_context,
      MessageLoop* ui_loop)
      : url_request_context_(url_request_context),
        download_server_url_(
            L"http://dl.google.com/chrome/dict/"),
        ui_loop_(ui_loop),
        spellchecker_flag_set_task_(spellchecker_flag_set_task) {
    // Determine dictionary file path and name.
    fetcher_.reset(NULL);
    dic_zip_file_path_ = file_util::GetDirectoryFromPath(dic_file_path);
    file_name_ = file_util::GetFilenameFromPath(dic_file_path);

    name_of_file_to_download_ = l10n_util::ToLower(file_name_);
  }

  // Save the file in memory buffer to the designated dictionary file.
  // returns the number of bytes it could save.
  // Invoke this on the file thread.
  void StartDownload() {
    GURL url(WideToUTF8(download_server_url_ + name_of_file_to_download_));
    fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
    fetcher_->set_request_context(url_request_context_);
    fetcher_->Start();
  }

 private:
  // The file has been downloaded in memory - need to write it down to file.
  bool SaveBufferToFile(const std::string& data) {
    const char *file_char = data.c_str();
    std::wstring file_to_write = dic_zip_file_path_;
    file_util::AppendToPath(&file_to_write, file_name_);
    int save_file = file_util::WriteFile(file_to_write, file_char,
                                         static_cast<int>(data.length()));
    return (save_file > 0 ? true : false);
  }

  // URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data) {
    DCHECK(source);
    fetcher_.reset(NULL);
    bool save_success = false;
    if ((response_code / 100) == 2 ||
        response_code == 401 ||
        response_code == 407) {
      save_success = SaveBufferToFile(data);
    }  // Unsuccessful save is taken care of spellchecker |Initialize|.

    // Set Flag that dictionary is not downloading anymore.
    ui_loop_->PostTask(FROM_HERE, spellchecker_flag_set_task_);
  }

  // factory object to invokelater back to spellchecker in io thread on
  // download completion to change appropriate flags.
  Task* spellchecker_flag_set_task_;

  // URLRequestContext to be used by URLFetcher. This is obtained from profile.
  // The ownership remains with the profile.
  URLRequestContext* url_request_context_;

  // URLFetcher to fetch the file in memory.
  scoped_ptr<URLFetcher> fetcher_;

  // The file path where both the dic files have to be written locally.
  std::wstring dic_zip_file_path_;

  // The name of the file in the server which has to be downloaded.
  std::wstring name_of_file_to_download_;

  // The name of the file which has to be stored locally.
  std::wstring file_name_;

  // The URL of the server from where the file has to be downloaded.
  const std::wstring download_server_url_;

  // this invokes back to io loop when downloading is over.
  MessageLoop* ui_loop_;
  DISALLOW_EVIL_CONSTRUCTORS(DictionaryDownloadController);
};


// This is a helper class which acts as a proxy for invoking a task from the
// file loop back to the IO loop. Invoking a task from file loop to the IO
// loop directly is not safe as during browser shutdown, the IO loop tears
// down before the file loop. To avoid a crash, this object is invoked in the
// UI loop from the file loop, from where it gets the IO thread directly from
// g_browser_process and invokes the given task in the IO loop if it is not
// NULL. This object also takes ownership of the given task.
class UIProxyForIOTask : public Task {
 public:
  explicit UIProxyForIOTask(Task* spellchecker_flag_set_task)
      : spellchecker_flag_set_task_(spellchecker_flag_set_task) {
  }

 private:
  void Run() {
    // This has been invoked in the UI thread.
    Thread* io_thread = g_browser_process->io_thread();
    if (io_thread) {  // io_thread has not been torn down yet.
      MessageLoop* io_loop = io_thread->message_loop();
      if (io_loop) {
        scoped_ptr<Task> this_task(spellchecker_flag_set_task_);
        io_loop->PostTask(FROM_HERE, this_task.release());
      }
    }
  }

  Task* spellchecker_flag_set_task_;
  DISALLOW_EVIL_CONSTRUCTORS(UIProxyForIOTask);
};

void SpellChecker::set_file_is_downloading(bool value) {
  dic_is_downloading_ = value;
}

// ################################################################
// This part of the code is used for spell checking.
// ################################################################

// static
void SpellChecker::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterLocalizedStringPref(prefs::kSpellCheckDictionary,
      IDS_SPELLCHECK_DICTIONARY);
}

SpellChecker::SpellChecker(const std::wstring& dict_dir,
                           const std::wstring& language,
                           URLRequestContext* request_context)
    : bdict_file_name_(dict_dir),
      bdict_file_(NULL),
      bdict_mapping_(NULL),
      bdict_mapped_data_(NULL),
      hunspell_(NULL),
      tried_to_init_(false),
#ifndef NDEBUG
      worker_loop_(NULL),
#endif
      tried_to_download_(false),
      url_request_context_(request_context),
      file_loop_(NULL),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      dic_download_state_changer_factory_(this),
      dic_is_downloading_(false) {
  // Remember UI loop to later use this as a proxy to get IO loop.
  ui_loop_ = MessageLoop::current();

  // Reset dictionary flag setting task to NULL.
  dic_task_.reset(NULL);

  // Get File Loop - hunspell gets initialized here.
  Thread* file_thread = g_browser_process->file_thread();
  if (file_thread)
    file_loop_ = file_thread->message_loop();

  // Get the path to the spellcheck file.
  file_util::AppendToPath(&bdict_file_name_, language + L".bdic");

  // Use this dictionary language as the default one of the
  // SpecllcheckCharAttribute object.
  character_attributes_.SetDefaultLanguage(language);
}

SpellChecker::~SpellChecker() {
#ifndef NDEBUG
  // This must be deleted on the I/O thread (see the header). This is the same
  // thread thatSpellCheckWord is called on, so we verify that they were all the
  // same thread.
  if (worker_loop_)
    DCHECK(MessageLoop::current() == worker_loop_);
#endif

  delete hunspell_;

  if (bdict_mapped_data_)
    UnmapViewOfFile(bdict_mapped_data_);
  if (bdict_mapping_)
    CloseHandle(bdict_mapping_);
  if (bdict_file_)
    CloseHandle(bdict_file_);
}

// Initialize SpellChecker. In this method, if the dicitonary is not present
// in the local disk, it is fetched asynchronously.
// TODO(sidchat): After dictionary is downloaded, initialize hunspell in
// file loop - this is currently being done in the io loop.
// Bug: http://b/issue?id=1123096
bool SpellChecker::Initialize() {
  // Return false if the dictionary files are downloading.
  if (dic_is_downloading_)
    return false;

  // Return false if tried to init and failed - don't try multiple times in
  // this session.
  if (tried_to_init_)
    return hunspell_ != NULL ? true : false;

  StatsScope<StatsCounterTimer> timer(chrome::Counters::spellcheck_init());

  bool dic_exists = file_util::PathExists(bdict_file_name_);
  if (!dic_exists) {
    if (file_loop_ && !tried_to_download_ && url_request_context_) {
      dic_task_.reset(dic_download_state_changer_factory_.NewRunnableMethod(
          &SpellChecker::set_file_is_downloading, false));
      ddc_dic_ = new DictionaryDownloadController(dic_task_.release(),
                                                  bdict_file_name_,
                                                  url_request_context_,
                                                  ui_loop_);
      set_file_is_downloading(true);
      file_loop_->PostTask(FROM_HERE, NewRunnableMethod(
          ddc_dic_.get(),
          &DictionaryDownloadController::StartDownload));
    }
  }

  if (!dic_exists && !tried_to_download_) {
    tried_to_download_ = true;
    return false;
  }

  // Control has come so far - both files probably exist.
  TimeTicks begin_time = TimeTicks::Now();

  const unsigned char* bdict_data;
  size_t bdict_length;
  if (MapBdictFile(&bdict_data, &bdict_length))
    hunspell_ = new Hunspell(bdict_data, bdict_length);

  TimeTicks end_time = TimeTicks::Now();
  DHISTOGRAM_TIMES(L"Spellcheck.InitTime", end_time - begin_time);

  tried_to_init_ = true;
  return false;
}

bool SpellChecker::MapBdictFile(const unsigned char** data, size_t* length) {
  bdict_file_ = CreateFile(bdict_file_name_.c_str(), GENERIC_READ,
      FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (bdict_file_ == INVALID_HANDLE_VALUE)
    return false;
  DWORD size = GetFileSize(bdict_file_, NULL);

  bdict_mapping_ = CreateFileMapping(bdict_file_, NULL, PAGE_READONLY, 0,
                                     size, NULL);
  if (!bdict_mapping_) {
    CloseHandle(bdict_file_);
    bdict_file_ = NULL;
    return false;
  }

  bdict_mapped_data_ = reinterpret_cast<const unsigned char*>(
      MapViewOfFile(bdict_mapping_, FILE_MAP_READ, 0, 0, size));
  if (!data) {
    CloseHandle(bdict_mapping_);
    bdict_mapping_ = NULL;
    CloseHandle(bdict_file_);
    bdict_file_ = NULL;
    return false;
  }

  *data = bdict_mapped_data_;
  *length = size;
  return true;
}

// Returns whether or not the given string is a valid contraction.
// This function is a fall-back when the SpellcheckWordIterator class
// returns a concatenated word which is not in the selected dictionary
// (e.g. "in'n'out") but each word is valid.
bool SpellChecker::IsValidContraction(const std::wstring& contraction) {
  SpellcheckWordIterator word_iterator;
  std::wstring word;
  int word_start;
  int word_length;
  word_iterator.Initialize(&character_attributes_, contraction.c_str(),
                           contraction.length(), false);
  while (word_iterator.GetNextWord(&word, &word_start, &word_length)) {
    if (!hunspell_->spell(WideToUTF8(word).c_str()))
      return false;
  }
  return true;
}

bool SpellChecker::SpellCheckWord(
    const wchar_t* in_word,
    int in_word_len,
    int* misspelling_start,
    int* misspelling_len,
    std::vector<std::wstring>* optional_suggestions) {
  DCHECK(in_word_len >= 0);
  DCHECK(misspelling_start && misspelling_len) << "Out vars must be given.";

#ifndef NDEBUG
  // This must always be called on the same thread (normally the I/O thread).
  if (worker_loop_)
    DCHECK(MessageLoop::current() == worker_loop_);
  else
    worker_loop_ = MessageLoop::current();
#endif

  Initialize();

  StatsScope<StatsRate> timer(chrome::Counters::spellcheck_lookup());

  *misspelling_start = 0;
  *misspelling_len = 0;
  if (in_word_len == 0)
    return true;  // no input means always spelled correctly

  if (!hunspell_)
    return true;  // unable to spellcheck, return word is OK

  SpellcheckWordIterator word_iterator;
  std::wstring word;
  int word_start;
  int word_length;
  word_iterator.Initialize(&character_attributes_, in_word, in_word_len,
                           true);
  while (word_iterator.GetNextWord(&word, &word_start, &word_length)) {
    // Found a word (or a contraction) that hunspell can check its spelling.
    std::string encoded_word = WideToUTF8(word);

    TimeTicks begin_time = TimeTicks::Now();
    bool word_ok = !!hunspell_->spell(encoded_word.c_str());
    DHISTOGRAM_TIMES(L"Spellcheck.CheckTime", TimeTicks::Now() - begin_time);

    if (word_ok)
      continue;

    // If the given word is a concatenated word of two or more valid words
    // (e.g. "hello:hello"), we should treat it as a valid word.
    if (IsValidContraction(word))
      continue;

    *misspelling_start = word_start;
    *misspelling_len = word_length;

    // Get the list of suggested words.
    if (optional_suggestions) {
      char **suggestions;
      TimeTicks begin_time = TimeTicks::Now();
      int number_of_suggestions = hunspell_->suggest(&suggestions,
                                                     encoded_word.c_str());
      DHISTOGRAM_TIMES(L"Spellcheck.SuggestTime",
                       TimeTicks::Now() - begin_time);

      // Populate the vector of WideStrings.
      for (int i = 0; i < number_of_suggestions; i++) {
        if (i < kMaxSuggestions)
          optional_suggestions->push_back(UTF8ToWide(suggestions[i]));
        free(suggestions[i]);
      }
      free(suggestions);
    }
    return false;
  }
  return true;
}
