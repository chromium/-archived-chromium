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

#ifndef CHROME_BROWSER_SPELLCHECKER_H__
#define CHROME_BROWSER_SPELLCHECKER_H__

#include "chrome/browser/spellcheck_worditerator.h"

#include "base/ref_counted.h"
#include "base/task.h"
#include "unicode/uscript.h"

class Hunspell;
class PrefService;
class Profile;
class MessageLoop;
class URLRequestContext;

// The Browser's Spell Checker. It checks and suggests corrections.
//
// This object is not threadsafe. In normal usage (not unit tests) it lives on
// the I/O thread of the browser. It is threadsafe refcounted so that I/O thread
// and the profile on the main thread (which gives out references to it) can
// keep it. However, all usage of this must be on the I/O thread.
//
// This object should also be deleted on the I/O thread only. It owns a
// reference to URLRequestContext which in turn owns the cache, etc. and must be
// deleted on the I/O thread itself.
class SpellChecker : public base::RefCountedThreadSafe<SpellChecker> {
 public:
  // Creates the spellchecker by reading dictionaries from the given directory,
  // and defaulting to the given language. Both strings must be provided.
  //
  // The request context is used to download dictionaries if they do not exist.
  // This can be NULL if you don't want this (like in tests).
  SpellChecker(const std::wstring& dict_dir,
               const std::wstring& language,
               URLRequestContext* request_context);

  static void RegisterUserPrefs(PrefService* prefs);

  // Only delete on the I/O thread (see above).
  ~SpellChecker();

  // SpellCheck a word.
  // Returns true if spelled correctly, false otherwise.
  // If the spellchecker failed to initialize, always returns true.
  // In addition, finds the suggested words for a given word
  // and puts them into |*optional_suggestions|.
  // If the word is spelled correctly, the vector is empty.
  // If optional_suggestions is NULL, suggested words will not be looked up.
  // Note that Doing suggest lookups can be slow.
  bool SpellCheckWord(const wchar_t* in_word,
                      int in_word_len,
                      int* misspelling_start,
                      int* misspelling_len,
                      std::vector<std::wstring>* optional_suggestions);

 private:
  // Download dictionary files when required.
  class DictionaryDownloadController;

  SpellChecker::SpellChecker();

  // Initializes the Hunspell Dictionary.
  bool Initialize();

  void set_file_is_downloading(bool value);

  // Memory maps the given .bdic file. On success, it will return true and will
  // place the data and lenght into the given out parameters.
  bool MapBdictFile(const unsigned char** data, size_t* length);

  // Returns whether or not the given word is a contraction of valid words
  // (e.g. "word:word").
  bool IsValidContraction(const std::wstring& word);

  // Path to the spellchecker file.
  std::wstring bdict_file_name_;

  // We memory-map the BDict file for spellchecking. These are the handles
  // necessary for that.
  HANDLE bdict_file_;
  HANDLE bdict_mapping_;
  const unsigned char* bdict_mapped_data_;

  // The hunspell dictionary in use.
  Hunspell *hunspell_;

  // Represents character attributes used for filtering out characters which
  // are not supported by this SpellChecker object.
  SpellcheckCharAttribute character_attributes_;

  // Flag indicating whether we've tried to initialize.  If we've already
  // attempted initialiation, we won't retry to avoid failure loops.
  bool tried_to_init_;

#ifndef NDEBUG
  // This object must only be used on the same thread. However, it is normally
  // created on the UI thread. This checks calls to SpellCheckWord and the
  // destructor to make sure we're only ever running on the same thread.
  //
  // This will be NULL if it is not initialized yet (not initialized in the
  // constructor since that's on a different thread.
  MessageLoop* worker_loop_;
#endif

  // Flag indicating whether we've tried to download dictionary files. If we've
  // already attempted download, we won't retry to avoid failure loops.
  bool tried_to_download_;

  // File Thread Message Loop.
  MessageLoop* file_loop_;

  // UI Thread Message Loop - this will be used as a proxy to access io loop.
  MessageLoop* ui_loop_;

  // Used for requests. MAY BE NULL which means don't try to download.
  URLRequestContext* url_request_context_;

  // DictionaryDownloadController object to download dictionary if required.
  scoped_refptr<DictionaryDownloadController> ddc_dic_;

  // Set when the dictionary file is currently downloading.
  bool dic_is_downloading_;

  // Tasks generated in spellchecker in IO thread, to be run back in the IO
  // thread from the UI thread when corresponding file is not downloading.
  scoped_ptr<Task> aff_task_;
  scoped_ptr<Task> dic_task_;

  // Used for generating callbacks to spellchecker, since spellchecker is a
  // non-reference counted object. The callback is generated by generating tasks
  // using NewRunableMethod on these objects.
  ScopedRunnableMethodFactory<SpellChecker> dic_download_state_changer_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(SpellChecker);
};

#endif  // #ifndef CHROME_BROWSER_SPELLCHECKER_H__
