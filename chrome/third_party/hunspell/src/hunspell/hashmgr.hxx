#ifndef _HASHMGR_HXX_
#define _HASHMGR_HXX_

#include <cstdio>
#include "htypes.hxx"

#ifdef HUNSPELL_CHROME_CLIENT
#include <string>
#include <map>

#include "base/stl_util-inl.h"
#include "base/string_piece.h"
#include "chrome/third_party/hunspell/google/bdict_reader.h"
#endif

enum flag { FLAG_CHAR, FLAG_LONG, FLAG_NUM, FLAG_UNI };

class HashMgr
{
#ifdef HUNSPELL_CHROME_CLIENT
  // Not owned by this class, owned by the Hunspell object.
  hunspell::BDictReader* bdict_reader;
  std::map<StringPiece, int> custom_word_to_affix_id_map_;
  std::vector<std::string*> pointer_to_strings_;
#endif
  int             tablesize;
  struct hentry * tableptr;
  int             userword;
  flag            flag_mode;
  int             complexprefixes;
  int             utf8;
  char *          ignorechars;
  unsigned short * ignorechars_utf16;
  int             ignorechars_utf16_len;
  int                 numaliasf; // flag vector `compression' with aliases
  unsigned short **   aliasf;
  unsigned short *    aliasflen;
  int                 numaliasm; // morphological desciption `compression' with aliases
  char **             aliasm;


public:
#ifdef HUNSPELL_CHROME_CLIENT
  HashMgr(hunspell::BDictReader* reader);
  
  // Return the hentry corresponding to the given word. Returns NULL if the 
  // word is not there in the cache.
  hentry* GetHentryFromHEntryCache(char* word);

  // Called before we do a new operation. This will empty the cache of pointers
  // to hentries that we have cached. In Chrome, we make these on-demand, but
  // they must live as long as the single spellcheck operation that they're part
  // of since Hunspell will save pointers to various ones as it works.
  //
  // This function allows that cache to be emptied and not grow infinitely.
  void EmptyHentryCache();
#else
  HashMgr(FILE* t_handle, FILE* a_handle);
#endif
  ~HashMgr();

  struct hentry * lookup(const char *) const;
  int hash(const char *) const;
  struct hentry * walk_hashtable(int & col, struct hentry * hp) const;

  int put_word(const char * word, int wl, char * ap);
  int put_word_pattern(const char * word, int wl, const char * pattern);
  int decode_flags(unsigned short ** result, char * flags);
  unsigned short        decode_flag(const char * flag);
  char *                encode_flag(unsigned short flag);
  int is_aliasf();
  int get_aliasf(int index, unsigned short ** fvec);
#ifdef HUNSPELL_EXPERIMENTAL
  int is_aliasm();
  char * get_aliasm(int index);
#endif

  
private:
  int load_tables(FILE* t_handle);
  int add_word(const char * word, int wl, unsigned short * ap, int al, const char * desc);

#ifdef HUNSPELL_CHROME_CLIENT
  int load_config();
  int parse_aliasf(char* line, hunspell::LineIterator* iterator);

  // Converts the list of affix IDs to a linked list of hentry structures. The
  // hentry structures will point to the given word. The returned pointer will
  // be a statically allocated variable that will change for the next call. The
  // |word| buffer must be the same.
  hentry* AffixIDsToHentry(char* word, int* affix_ids, int affix_count) const;

  // See EmptyHentryCache above. Note that each one is actually a linked list
  // followed by the homonym pointer.
  typedef std::map<std::string, hentry*> HEntryCache;
  HEntryCache hentry_cache;

#else
  int load_config(FILE* aff_handle);
  int parse_aliasf(char * line, FILE * af);
#endif

#ifdef HUNSPELL_EXPERIMENTAL
  int parse_aliasm(char * line, FILE * af);
#endif

};

#endif
