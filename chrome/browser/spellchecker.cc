// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "chrome/browser/spellchecker.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/third_party/hunspell/src/hunspell/hunspell.hxx"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/url_request/url_request.h"

using base::TimeTicks;

static const int kMaxSuggestions = 5;  // Max number of dictionary suggestions.

namespace {

static const struct {
  // The language.
  const char* language;

  // The corresponding language and region, used by the dictionaries.
  const char* language_region;
} g_supported_spellchecker_languages[] = {
  {"en-US", "en-US"},
  {"en-GB", "en-GB"},
  {"fr", "fr-FR"},
  {"it", "it-IT"},
  {"de", "de-DE"},
  {"es", "es-ES"},
  {"nl", "nl-NL"},
  {"pt-BR", "pt-BR"},
  {"ru", "ru-RU"},
  {"pl", "pl-PL"},
  // {"th", "th-TH"}, // Not to be included in Spellchecker as per B=1277824
  {"sv", "sv-SE"},
  {"da", "da-DK"},
  {"pt-PT", "pt-PT"},
  {"ro", "ro-RO"},
  // {"hu", "hu-HU"}, // Not to be included in Spellchecker as per B=1277824
  {"he", "he-IL"},
  {"id", "id-ID"},
  {"cs", "cs-CZ"},
  {"el", "el-GR"},
  {"nb", "nb-NO"},
  {"vi", "vi-VN"},
  // {"bg", "bg-BG"}, // Not to be included in Spellchecker as per B=1277824
  {"hr", "hr-HR"},
  {"lt", "lt-LT"},
  {"sk", "sk-SK"},
  {"sl", "sl-SI"},
  {"ca", "ca-ES"},
  {"lv", "lv-LV"},
  // {"uk", "uk-UA"}, // Not to be included in Spellchecker as per B=1277824
  {"hi", "hi-IN"},
  {"et", "et-EE"},
  {"tr", "tr-TR"},
};

}

void SpellChecker::SpellCheckLanguages(Languages* languages) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i)
    languages->push_back(g_supported_spellchecker_languages[i].language);
}

// This function returns the language-region version of language name.
// e.g. returns hi-IN for hi.
SpellChecker::Language SpellChecker::GetSpellCheckLanguageRegion(
    Language input_language) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    Language language(g_supported_spellchecker_languages[i].language);
    if (language ==  input_language)
      return Language(g_supported_spellchecker_languages[i].language_region);
  }

  return input_language;
}


SpellChecker::Language SpellChecker::GetLanguageFromLanguageRegion(
    Language input_language) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    Language language(g_supported_spellchecker_languages[i].language_region);
    if (language ==  input_language)
      return Language(g_supported_spellchecker_languages[i].language);
  }

  return input_language;
}

SpellChecker::Language SpellChecker::GetCorrespondingSpellCheckLanguage(
    const Language& language) {
  // Look for exact match in the Spell Check language list.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    Language spellcheck_language(
        g_supported_spellchecker_languages[i].language);
    if (spellcheck_language == language)
      return language;
  }

  // Look for a match by comparing only language parts. All the 'en-RR'
  // except for 'en-GB' exactly matched in the above loop, will match
  // 'en-US'. This is not ideal because 'en-AU', 'en-ZA', 'en-NZ' had
  // better be matched with 'en-GB'. This does not handle cases like
  // 'az-Latn-AZ' vs 'az-Arab-AZ', either, but we don't use 3-part
  // locale ids with a script code in the middle, yet.
  // TODO(jungshik): Add a better fallback.
  Language language_part(language, 0, language.find('-'));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    Language spellcheck_language(
        g_supported_spellchecker_languages[i].language_region);
    if (spellcheck_language.substr(0, spellcheck_language.find('-')) ==
        language_part)
      return spellcheck_language;
  }

  // No match found - return blank.
  return Language();
}

int SpellChecker::GetSpellCheckLanguagesToDisplayInContextMenu(
    Profile* profile,
    DisplayLanguages* display_languages) {
  StringPrefMember accept_languages_pref;
  StringPrefMember dictionary_language_pref;
  accept_languages_pref.Init(prefs::kAcceptLanguages, profile->GetPrefs(),
                             NULL);
  dictionary_language_pref.Init(prefs::kSpellCheckDictionary,
                                profile->GetPrefs(), NULL);
  std::wstring dictionary_language = dictionary_language_pref.GetValue();

  // The current dictionary language should be there.
  display_languages->push_back(dictionary_language);

  // Now scan through the list of accept languages, and find possible mappings
  // from this list to the existing list of spell check languages.
  Languages accept_languages;
  std::set<Language> unique_languages;
  SplitString(WideToASCII(accept_languages_pref.GetValue()), ',',
              &accept_languages);
  for (Languages::const_iterator i = accept_languages.begin();
       i != accept_languages.end(); ++i) {
    Language language(GetCorrespondingSpellCheckLanguage(*i));
    if (!language.empty())
      unique_languages.insert(language);
  }

  for (std::set<Language>::const_iterator i = unique_languages.begin();
       i != unique_languages.end(); ++i)
    display_languages->push_back(ASCIIToWide(*i));

  // Sort using locale specific sorter.
  l10n_util::SortStrings(g_browser_process->GetApplicationLocale(),
                         display_languages);

  for (size_t i = 0; i < display_languages->size(); ++i) {
    if ((*display_languages)[i] == dictionary_language)
      return i;
  }
  return -1;
}

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
    base::Thread* io_thread = g_browser_process->io_thread();
    if (io_thread) {  // io_thread has not been torn down yet.
      MessageLoop* io_loop = io_thread->message_loop();
      if (io_loop) {
        io_loop->PostTask(FROM_HERE, spellchecker_flag_set_task_);
        spellchecker_flag_set_task_ = NULL;
      }
    }
  }

  Task* spellchecker_flag_set_task_;
  DISALLOW_COPY_AND_ASSIGN(UIProxyForIOTask);
};

// ############################################################################
// This part of the spellchecker code is used for downloading spellchecking
// dictionary if required. This code is included in this file since dictionary
// is an integral part of spellchecker.

// Design: The spellchecker initializes hunspell_ in the Initialize() method.
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
      const FilePath& dic_file_path,
      URLRequestContext* url_request_context,
      MessageLoop* ui_loop)
      : spellchecker_flag_set_task_(spellchecker_flag_set_task),
        url_request_context_(url_request_context),
        ui_loop_(ui_loop) {
    // Determine dictionary file path and name.
    dic_zip_file_path_ = dic_file_path.DirName();
    file_name_ = dic_file_path.BaseName();
  }

  // Save the file in memory buffer to the designated dictionary file.
  // returns the number of bytes it could save.
  // Invoke this on the file thread.
  void StartDownload() {
    static const char kDownloadServerUrl[] =
        "http://cache.pack.google.com/edgedl/chrome/dict/";

    GURL url = GURL(std::string(kDownloadServerUrl) + WideToUTF8(
                        l10n_util::ToLower(file_name_.ToWStringHack())));
    fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
    fetcher_->set_request_context(url_request_context_);
    fetcher_->Start();
  }

 private:
  // The file has been downloaded in memory - need to write it down to file.
  bool SaveBufferToFile(const std::string& data) {
    FilePath file_to_write = dic_zip_file_path_.Append(file_name_);
    int num_bytes = data.length();
    return file_util::WriteFile(file_to_write, data.data(), num_bytes) ==
        num_bytes;
  }

  // URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data) {
    DCHECK(source);
    bool save_success = false;
    if ((response_code / 100) == 2 ||
        response_code == 401 ||
        response_code == 407) {
      save_success = SaveBufferToFile(data);
    }  // Unsuccessful save is taken care of in SpellChecker::Initialize().

    // Set Flag that dictionary is not downloading anymore.
    ui_loop_->PostTask(FROM_HERE,
                       new UIProxyForIOTask(spellchecker_flag_set_task_));
    fetcher_.reset(NULL);
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
  FilePath dic_zip_file_path_;

  // The name of the file which has to be stored locally.
  FilePath file_name_;

  // this invokes back to io loop when downloading is over.
  MessageLoop* ui_loop_;
  DISALLOW_COPY_AND_ASSIGN(DictionaryDownloadController);
};

void SpellChecker::set_file_is_downloading(bool value) {
  dic_is_downloading_ = value;
}

// ################################################################
// This part of the code is used for spell checking.
// ################################################################

FilePath SpellChecker::GetVersionedFileName(const Language& input_language,
                                            const FilePath& dict_dir) {
  // The default dictionary version is 1-2. These versions have been augmented
  // with additional words found by the translation team.
  static const char kDefaultVersionString[] = "-1-2";

  // The following dictionaries have either not been augmented with additional
  // words (version 1-1) or have new words, as well as an upgraded dictionary
  // as of Feb 2009 (version 1-3).
  static const struct {
    // The language input.
    const char* language;

    // The corresponding version.
    const char* version;
  } special_version_string[] = {
    {"en-GB", "-1-1"},
    {"es-ES", "-1-1"},
    {"nl-NL", "-1-1"},
    {"ru-RU", "-1-1"},
    {"sv-SE", "-1-1"},
    {"he-IL", "-1-1"},
    {"el-GR", "-1-1"},
    {"hi-IN", "-1-1"},
    {"tr-TR", "-1-1"},
    {"et-EE", "-1-1"},
    {"fr-FR", "-1-3"},
    {"pl-PL", "-1-3"}
  };

  // Generate the bdict file name using default version string or special
  // version string, depending on the language.
  std::string language = GetSpellCheckLanguageRegion(input_language);
  std::string versioned_bdict_file_name(language + kDefaultVersionString +
                                        ".bdic");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(special_version_string); ++i) {
    if (language == special_version_string[i].language) {
      versioned_bdict_file_name =
          language + special_version_string[i].version + ".bdic";
      break;
    }
  }

  return dict_dir.AppendASCII(versioned_bdict_file_name);
}

SpellChecker::SpellChecker(const FilePath& dict_dir,
                           const Language& language,
                           URLRequestContext* request_context,
                           const FilePath& custom_dictionary_file_name)
    : custom_dictionary_file_name_(custom_dictionary_file_name),
      tried_to_init_(false),
#ifndef NDEBUG
      worker_loop_(NULL),
#endif
      tried_to_download_(false),
      file_loop_(NULL),
      url_request_context_(request_context),
      dic_is_downloading_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          dic_download_state_changer_factory_(this)) {
  // Remember UI loop to later use this as a proxy to get IO loop.
  ui_loop_ = MessageLoop::current();

  // Get File Loop - hunspell gets initialized here.
  base::Thread* file_thread = g_browser_process->file_thread();
  if (file_thread)
    file_loop_ = file_thread->message_loop();

  // Get the path to the spellcheck file.
  bdict_file_name_ = GetVersionedFileName(language, dict_dir);

  // Get the path to the custom dictionary file.
  if (custom_dictionary_file_name_.empty()) {
    FilePath personal_file_directory;
    PathService::Get(chrome::DIR_USER_DATA, &personal_file_directory);
    custom_dictionary_file_name_ =
        personal_file_directory.Append(chrome::kCustomDictionaryFileName);
  }

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
    return hunspell_.get() != NULL;

  StatsScope<StatsCounterTimer> timer(chrome::Counters::spellcheck_init());

  bool dic_exists = file_util::PathExists(bdict_file_name_);
  if (!dic_exists) {
    if (file_loop_ && !tried_to_download_ && url_request_context_) {
      Task* dic_task = dic_download_state_changer_factory_.NewRunnableMethod(
          &SpellChecker::set_file_is_downloading, false);
      ddc_dic_ = new DictionaryDownloadController(dic_task, bdict_file_name_,
          url_request_context_, ui_loop_);
      set_file_is_downloading(true);
      file_loop_->PostTask(FROM_HERE, NewRunnableMethod(ddc_dic_.get(),
          &DictionaryDownloadController::StartDownload));
    }
  }

  if (!dic_exists && !tried_to_download_) {
    tried_to_download_ = true;
    return false;
  }

  // Control has come so far - both files probably exist.
  TimeTicks begin_time = TimeTicks::Now();
  bdict_file_.reset(new file_util::MemoryMappedFile());
  if (bdict_file_->Initialize(bdict_file_name_)) {
    hunspell_.reset(new Hunspell(bdict_file_->data(), bdict_file_->length()));
    AddCustomWordsToHunspell();
  }
  DHISTOGRAM_TIMES("Spellcheck.InitTime", TimeTicks::Now() - begin_time);

  tried_to_init_ = true;
  return false;
}

void SpellChecker::AddCustomWordsToHunspell() {
  // Add custom words to Hunspell.
  // This should be done in File Loop, but since Hunspell is in this IO Loop,
  // this too has to be initialized here.
  // TODO(sidchat): Work out a way to initialize Hunspell in the File Loop.
  std::string contents;
  file_util::ReadFileToString(custom_dictionary_file_name_, &contents);
  std::vector<std::string> list_of_words;
  SplitString(contents, '\n', &list_of_words);
  if (hunspell_.get()) {
    for (std::vector<std::string>::iterator it = list_of_words.begin();
         it != list_of_words.end(); ++it) {
      hunspell_->put_word(it->c_str());
    }
  }
}

// Returns whether or not the given string is a valid contraction.
// This function is a fall-back when the SpellcheckWordIterator class
// returns a concatenated word which is not in the selected dictionary
// (e.g. "in'n'out") but each word is valid.
bool SpellChecker::IsValidContraction(const string16& contraction) {
  SpellcheckWordIterator word_iterator;
  word_iterator.Initialize(&character_attributes_, contraction.c_str(),
                           contraction.length(), false);

  string16 word;
  int word_start;
  int word_length;
  while (word_iterator.GetNextWord(&word, &word_start, &word_length)) {
    if (!hunspell_->spell(UTF16ToUTF8(word).c_str()))
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

  if (!hunspell_.get())
    return true;  // unable to spellcheck, return word is OK

  SpellcheckWordIterator word_iterator;
  string16 word;
  string16 in_word_utf16;
  WideToUTF16(in_word, in_word_len, &in_word_utf16);
  int word_start;
  int word_length;
  word_iterator.Initialize(&character_attributes_, in_word_utf16.c_str(),
                           in_word_len, true);
  while (word_iterator.GetNextWord(&word, &word_start, &word_length)) {
    // Found a word (or a contraction) that hunspell can check its spelling.
    std::string encoded_word = UTF16ToUTF8(word);

    {
      TimeTicks begin_time = TimeTicks::Now();
      bool word_ok = !!hunspell_->spell(encoded_word.c_str());
      DHISTOGRAM_TIMES("Spellcheck.CheckTime", TimeTicks::Now() - begin_time);
      if (word_ok)
        continue;
    }

    // If the given word is a concatenated word of two or more valid words
    // (e.g. "hello:hello"), we should treat it as a valid word.
    if (IsValidContraction(word))
      continue;

    *misspelling_start = word_start;
    *misspelling_len = word_length;

    // Get the list of suggested words.
    if (optional_suggestions) {
      char** suggestions;
      TimeTicks begin_time = TimeTicks::Now();
      int number_of_suggestions = hunspell_->suggest(&suggestions,
                                                     encoded_word.c_str());
      DHISTOGRAM_TIMES("Spellcheck.SuggestTime",
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

// This task is called in the file loop to write the new word to the custom
// dictionary in disc.
class AddWordToCustomDictionaryTask : public Task {
 public:
  AddWordToCustomDictionaryTask(const FilePath& file_name,
                                const std::wstring& word)
      : file_name_(file_name),
        word_(WideToUTF8(word)) {
  }

 private:
  void Run() {
    // Add the word with a new line. Note that, although this would mean an
    // extra line after the list of words, this is potentially harmless and
    // faster, compared to verifying everytime whether to append a new line
    // or not.
    word_ += "\n";
    FILE* f = file_util::OpenFile(file_name_, "a+");
    if (f != NULL)
      fputs(word_.c_str(), f);
    file_util::CloseFile(f);
  }

  FilePath file_name_;
  std::string word_;
};

void SpellChecker::AddWord(const std::wstring& word) {
  // Check if the |hunspell_| has been initialized at all.
  Initialize();

  // Add the word to hunspell.
  std::string word_to_add = WideToUTF8(word);
  if (!word_to_add.empty())
    hunspell_->put_word(word_to_add.c_str());

  // Now add the word to the custom dictionary file.
  Task* write_word_task =
      new AddWordToCustomDictionaryTask(custom_dictionary_file_name_, word);
  if (file_loop_)
    file_loop_->PostTask(FROM_HERE, write_word_task);
  else
    write_word_task->Run();
}
