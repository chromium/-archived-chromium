#include "license.hunspell"
#include "license.myspell"

#ifndef MOZILLA_CLIENT
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>
#else
#include <stdlib.h> 
#include <string.h>
#include <stdio.h> 
#include <ctype.h>
#endif

#include "hashmgr.hxx"
#include "csutil.hxx"
#include "atypes.hxx"

#ifdef MOZILLA_CLIENT
#ifdef __SUNPRO_CC // for SunONE Studio compiler
using namespace std;
#endif
#else
#ifndef W32
using namespace std;
#endif
#endif

// build a hash table from a munched word list
#ifdef HUNSPELL_CHROME_CLIENT
HashMgr::HashMgr(hunspell::BDictReader* reader)
{
  bdict_reader = reader;
#else
HashMgr::HashMgr(FILE* dic_handle, FILE* aff_handle)
{
#endif
  tablesize = 0;
  tableptr = NULL;
  flag_mode = FLAG_CHAR;
  complexprefixes = 0;
  utf8 = 0;
  ignorechars = NULL;
  ignorechars_utf16 = NULL;
  ignorechars_utf16_len = 0;
  numaliasf = 0;
  aliasf = NULL;
  numaliasm = 0;
  aliasm = NULL;
#ifdef HUNSPELL_CHROME_CLIENT
  // No tables to load, just the AF config.
  int ec = load_config();
#else
  load_config(aff_handle);
  int ec = load_tables(dic_handle);
#endif
  if (ec) {
    /* error condition - what should we do here */
    HUNSPELL_WARNING(stderr, "Hash Manager Error : %d\n",ec);
    if (tableptr) {
      free(tableptr);
      tableptr = NULL;
    }
    tablesize = 0;
  }
}


HashMgr::~HashMgr()
{
  if (tableptr) {
    // now pass through hash table freeing up everything
    // go through column by column of the table
    for (int i=0; i < tablesize; i++) {
      struct hentry * pt = &tableptr[i];
      struct hentry * nt = NULL;
      if (pt) {
        if (pt->astr && !aliasf) free(pt->astr);
        if (pt->word) free(pt->word);
#ifdef HUNSPELL_EXPERIMENTAL
        if (pt->description && !aliasm) free(pt->description);
#endif
        pt = pt->next;
      }
      while(pt) {
        nt = pt->next;
        if (pt->astr && !aliasf) free(pt->astr);
        if (pt->word) free(pt->word);
#ifdef HUNSPELL_EXPERIMENTAL
        if (pt->description && !aliasm) free(pt->description);
#endif
        free(pt);
        pt = nt;
      }
    }
    free(tableptr);
    tableptr = NULL;
  }
  tablesize = 0;

  if (aliasf) {
    for (int j = 0; j < (numaliasf); j++) free(aliasf[j]);
    free(aliasf);
    aliasf = NULL;
    if (aliasflen) {
      free(aliasflen);
      aliasflen = NULL;
    }
  }
  if (aliasm) {
    for (int j = 0; j < (numaliasm); j++) free(aliasm[j]);
    free(aliasm);
    aliasm = NULL;
  }  
  
  if (ignorechars) free(ignorechars);
  if (ignorechars_utf16) free(ignorechars_utf16);

#ifdef HUNSPELL_CHROME_CLIENT
  EmptyHentryCache();
  for (std::vector<std::string*>::iterator it = pointer_to_strings_.begin(); 
       it != pointer_to_strings_.end(); ++it) {
    delete *it;
  }
#endif
}

#ifdef HUNSPELL_CHROME_CLIENT
void HashMgr::EmptyHentryCache() {
  // We need to delete each cache entry, and each additional one in the linked
  // list of homonyms.
  for (HEntryCache::iterator i = hentry_cache.begin();
       i != hentry_cache.end(); ++i) {
    hentry* cur = i->second;
    while (cur) {
      hentry* next = cur->next_homonym;
      delete cur;
      cur = next;
    }
  }
  hentry_cache.clear();
}
#endif

// lookup a root word in the hashtable

struct hentry * HashMgr::lookup(const char *word) const
{
#ifdef HUNSPELL_CHROME_CLIENT
  int affix_ids[hunspell::BDict::MAX_AFFIXES_PER_WORD];
  int affix_count = bdict_reader->FindWord(word, affix_ids);
  if (affix_count == 0) { // look for custom added word
    std::map<StringPiece, int>::const_iterator iter = 
      custom_word_to_affix_id_map_.find(word);
    if (iter != custom_word_to_affix_id_map_.end()) {
      affix_count = 1;
      affix_ids[0] = iter->second;
    }
  }

  static const int kMaxWordLen = 128;
  static char word_buf[kMaxWordLen];
  strncpy(word_buf, word, kMaxWordLen);

  return AffixIDsToHentry(word_buf, affix_ids, affix_count);
#else
    struct hentry * dp;
    if (tableptr) {
       dp = &tableptr[hash(word)];
       if (dp->word == NULL) return NULL;
       for (  ;  dp != NULL;  dp = dp->next) {
          if (strcmp(word,dp->word) == 0) return dp;
       }
    }
    return NULL;
#endif
}

// add a word to the hash table (private)

int HashMgr::add_word(const char * word, int wl, unsigned short * aff, int al, const char * desc)
{
#ifndef HUNSPELL_CHROME_CLIENT
    char * st = mystrdup(word);
    if (wl && !st) return 1;
    if (ignorechars != NULL) {
      if (utf8) {
        remove_ignored_chars_utf(st, ignorechars_utf16, ignorechars_utf16_len);
      } else {
        remove_ignored_chars(st, ignorechars);
      }
    }
    if (complexprefixes) {
        if (utf8) reverseword_utf(st); else reverseword(st);
    }
    int i = hash(st);
    struct hentry * dp = &tableptr[i];
    if (dp->word == NULL) {
       dp->wlen = (short) wl;
       dp->alen = (short) al;
       dp->word = st;
       dp->astr = aff;
       dp->next = NULL;
       dp->next_homonym = NULL;
#ifdef HUNSPELL_EXPERIMENTAL
       if (aliasm) {
            dp->description = (desc) ? get_aliasm(atoi(desc)) : mystrdup(desc);
       } else {
            dp->description = mystrdup(desc);
            if (desc && !dp->description) return 1;
            if (dp->description && complexprefixes) {
                if (utf8) reverseword_utf(dp->description); else reverseword(dp->description);
            }
       }
#endif
    } else {
       struct hentry* hp = (struct hentry *) malloc (sizeof(struct hentry));
       if (!hp) return 1;
       hp->wlen = (short) wl;
       hp->alen = (short) al;
       hp->word = st;
       hp->astr = aff;
       hp->next = NULL;      
       hp->next_homonym = NULL;
#ifdef HUNSPELL_EXPERIMENTAL       
       if (aliasm) {
            hp->description = (desc) ? get_aliasm(atoi(desc)) : mystrdup(desc);
       } else {
            hp->description = mystrdup(desc);
            if (desc && !hp->description) return 1;
            if (dp->description && complexprefixes) {
                if (utf8) reverseword_utf(hp->description); else reverseword(hp->description);
            }
       }
#endif
       while (dp->next != NULL) {
         if ((!dp->next_homonym) && (strcmp(hp->word, dp->word) == 0)) dp->next_homonym = hp;
         dp=dp->next;
       }
       if ((!dp->next_homonym) && (strcmp(hp->word, dp->word) == 0)) dp->next_homonym = hp;
       dp->next = hp;
    }
#endif  // HUNSPELL_CHROME_CLIENT
    std::map<StringPiece, int>::iterator iter = 
        custom_word_to_affix_id_map_.find(word);
    if(iter == custom_word_to_affix_id_map_.end()) {  // word needs to be added
      std::string* new_string_word = new std::string(word);
      pointer_to_strings_.push_back(new_string_word);
      StringPiece sp(*(new_string_word));
      custom_word_to_affix_id_map_[sp] = 0; // no affixes for custom words
      return 1;
    }

    return 0;
}     

// add a custom dic. word to the hash table (public)
int HashMgr::put_word(const char * word, int wl, char * aff)
{
    unsigned short * flags;
    int al = 0;
    if (aff) {
        al = decode_flags(&flags, aff);
        flag_qsort(flags, 0, al);
    } else {
        flags = NULL;
    }
    add_word(word, wl, flags, al, NULL);
    return 0;
}

int HashMgr::put_word_pattern(const char * word, int wl, const char * pattern)
{
    unsigned short * flags;
    struct hentry * dp = lookup(pattern);
    if (!dp || !dp->astr) return 1;
    flags = (unsigned short *) malloc (dp->alen * sizeof(short));
    memcpy((void *) flags, (void *) dp->astr, dp->alen * sizeof(short));
    add_word(word, wl, flags, dp->alen, NULL);
    return 0;
}

// walk the hash table entry by entry - null at end
struct hentry * HashMgr::walk_hashtable(int &col, struct hentry * hp) const
{
#ifdef HUNSPELL_CHROME_CLIENT
  // This function creates a new hentry if NULL is passed as hp. It also takes
  // the responsibility of deleting the pointer hp when walk is over.
  
  // This function is only ever called by one place and not nested. We can
  // therefore keep static state between calls and use |col| as a "reset" flag
  // to avoid changing the API. It is set to -1 for the first call.
  static hunspell::WordIterator word_iterator =
      bdict_reader->GetAllWordIterator();
  if (col < 0) {
    col = 1;
    word_iterator = bdict_reader->GetAllWordIterator();
  }

  int affix_ids[hunspell::BDict::MAX_AFFIXES_PER_WORD];
  static const int kMaxWordLen = 128;
  static char word[kMaxWordLen];
  int affix_count = word_iterator.Advance(word, kMaxWordLen, affix_ids);
  if (affix_count == 0) {
    delete hp;
    return NULL;
  }
  short word_len = static_cast<short>(strlen(word));

  // For now, just re-compute the |hp| and return it. No need to create linked
  // lists for the extra affixes. If hp is NULL, create it here.
  if (!hp)
    hp = new hentry;
  hp->word = word;
  hp->wlen = word_len;
  hp->alen = (short)const_cast<HashMgr*>(this)->get_aliasf(affix_ids[0],
                                                           &hp->astr);
  hp->next = NULL;
  hp->next_homonym = NULL;
  
  return hp;
#else
  //reset to start
  if ((col < 0) || (hp == NULL)) {
    col = -1;
    hp = NULL;
  }

  if (hp && hp->next != NULL) {
    hp = hp->next;
  } else {
    col++;
    hp = (col < tablesize) ? &tableptr[col] : NULL;
    // search for next non-blank column entry
    while (hp && (hp->word == NULL)) {
        col ++;
        hp = (col < tablesize) ? &tableptr[col] : NULL;
    }
    if (col < tablesize) return hp;
    hp = NULL;
    col = -1;
  }
  return hp;
#endif
}

// load a munched word list and build a hash table on the fly
int HashMgr::load_tables(FILE* t_handle)
{
#ifndef HUNSPELL_CHROME_CLIENT
  int wl, al;
  char * ap;
  char * dp;
  unsigned short * flags;

  // raw dictionary - munched file
  FILE * rawdict = _fdopen(_dup(_fileno(t_handle)), "r");
  if (rawdict == NULL) return 1;
  fseek(rawdict, 0, SEEK_SET);

  // first read the first line of file to get hash table size */
  char ts[MAXDELEN];
  if (! fgets(ts, MAXDELEN-1,rawdict)) return 2;
  mychomp(ts);
  
  /* remove byte order mark */
  if (strncmp(ts,"\xef\xbb\xbf",3) == 0) {
    memmove(ts, ts+3, strlen(ts+3)+1);
    HUNSPELL_WARNING(stderr, "warning: dic file begins with byte order mark: possible incompatibility with old Hunspell versions\n");
  }
  
  if ((*ts < '1') || (*ts > '9')) HUNSPELL_WARNING(stderr, "error - missing word count in dictionary file\n");
  tablesize = atoi(ts);
  if (!tablesize) return 4; 
  tablesize = tablesize + 5 + USERWORD;
  if ((tablesize %2) == 0) tablesize++;

  // allocate the hash table
  tableptr = (struct hentry *) calloc(tablesize, sizeof(struct hentry));
  if (! tableptr) return 3;
  for (int i=0; i<tablesize; i++) tableptr[i].word = NULL;

  // loop through all words on much list and add to hash
  // table and create word and affix strings

  while (fgets(ts,MAXDELEN-1,rawdict)) {
    mychomp(ts);
    // split each line into word and morphological description
    dp = strchr(ts,'\t');

    if (dp) {
      *dp = '\0';
      dp++;
    } else {
      dp = NULL;
    }

    // split each line into word and affix char strings
    // "\/" signs slash in words (not affix separator)
    // "/" at beginning of the line is word character (not affix separator)
    ap = strchr(ts,'/');
    while (ap) {
        if (ap == ts) {
            ap++;
            continue;
        } else if (*(ap - 1) != '\\') break;
        // replace "\/" with "/"
        for (char * sp = ap - 1; *sp; *sp = *(sp + 1), sp++);
        ap = strchr(ap,'/');
    }

    if (ap) {
      *ap = '\0';
      if (aliasf) {
        int index = atoi(ap + 1);
        al = get_aliasf(index, &flags);
        if (!al) {
            HUNSPELL_WARNING(stderr, "error - bad flag vector alias: %s\n", ts);
            *ap = '\0';
        }
      } else {
        al = decode_flags(&flags, ap + 1);
        flag_qsort(flags, 0, al);
      }
    } else {
      al = 0;
      ap = NULL;
      flags = NULL;
    }

    wl = strlen(ts);

    // add the word and its index
    if (add_word(ts,wl,flags,al,dp)) return 5;

  }
 
  fclose(rawdict);
#endif
  return 0;
}


// the hash function is a simple load and rotate
// algorithm borrowed

int HashMgr::hash(const char * word) const
{
#ifdef HUNSPELL_CHROME_CLIENT
    return 0;
#else
    long  hv = 0;
    for (int i=0; i < 4  &&  *word != 0; i++)
        hv = (hv << 8) | (*word++);
    while (*word != 0) {
      ROTATE(hv,ROTATE_LEN);
      hv ^= (*word++);
    }
    return (unsigned long) hv % tablesize;
#endif
}

int HashMgr::decode_flags(unsigned short ** result, char * flags) {
    int len;
    switch (flag_mode) {
      case FLAG_LONG: { // two-character flags (1x2yZz -> 1x 2y Zz)
        len = strlen(flags);
        if (len%2 == 1) HUNSPELL_WARNING(stderr, "error: length of FLAG_LONG flagvector is odd: %s\n", flags);
        len = len/2;
        *result = (unsigned short *) malloc(len * sizeof(short));
        for (int i = 0; i < len; i++) {
            (*result)[i] = (((unsigned short) flags[i * 2]) << 8) + (unsigned short) flags[i * 2 + 1]; 
        }
        break;
      }
      case FLAG_NUM: { // decimal numbers separated by comma (4521,23,233 -> 4521 23 233)
        len = 1;
        char * src = flags; 
        unsigned short * dest;
        char * p;
        for (p = flags; *p; p++) {
          if (*p == ',') len++;
        }
        *result = (unsigned short *) malloc(len * sizeof(short));
        dest = *result;
        for (p = flags; *p; p++) {
          if (*p == ',') {
            *dest = (unsigned short) atoi(src);
            if (*dest == 0) HUNSPELL_WARNING(stderr, "error: 0 is wrong flag id\n");
            src = p + 1;
            dest++;
          }
        }
        *dest = (unsigned short) atoi(src);
        if (*dest == 0) HUNSPELL_WARNING(stderr, "error: 0 is wrong flag id\n");
        break;
      }    
      case FLAG_UNI: { // UTF-8 characters
        w_char w[MAXDELEN/2];
        len = u8_u16(w, MAXDELEN/2, flags);
        *result = (unsigned short *) malloc(len * sizeof(short));
        memcpy(*result, w, len * sizeof(short));
        break;
      }
      default: { // Ispell's one-character flags (erfg -> e r f g)
        unsigned short * dest;
        len = strlen(flags);
        *result = (unsigned short *) malloc(len * sizeof(short));
        dest = *result;
        for (unsigned char * p = (unsigned char *) flags; *p; p++) {
          *dest = (unsigned short) *p;
          dest++;
        }
      }
    }      
    return len;
}

unsigned short HashMgr::decode_flag(const char * f) {
    unsigned short s = 0;
    switch (flag_mode) {
      case FLAG_LONG:
        s = ((unsigned short) f[0] << 8) + (unsigned short) f[1];
        break;
      case FLAG_NUM:
        s = (unsigned short) atoi(f);
        break;
      case FLAG_UNI:
        u8_u16((w_char *) &s, 1, f);
        break;
      default:
        s = (unsigned short) *((unsigned char *)f);
    }
    if (!s) HUNSPELL_WARNING(stderr, "error: 0 is wrong flag id\n");
    return s;
}

char * HashMgr::encode_flag(unsigned short f) {
    unsigned char ch[10];
    if (f==0) return mystrdup("(NULL)");
    if (flag_mode == FLAG_LONG) {
        ch[0] = (unsigned char) (f >> 8);
        ch[1] = (unsigned char) (f - ((f >> 8) << 8));
        ch[2] = '\0';
    } else if (flag_mode == FLAG_NUM) {
        sprintf((char *) ch, "%d", f);
    } else if (flag_mode == FLAG_UNI) {
        u16_u8((char *) &ch, 10, (w_char *) &f, 1);
    } else {
        ch[0] = (unsigned char) (f);
        ch[1] = '\0';
    }
    return mystrdup((char *) ch);
}

#ifdef HUNSPELL_CHROME_CLIENT
int HashMgr::load_config()
{
  utf8 = 1;  // We always use UTF-8.

  // Read in all the AF lines which tell us the rules for each affix group ID.
  char line[MAXDELEN+1];
  hunspell::LineIterator iterator = bdict_reader->GetAfLineIterator();
  while (iterator.AdvanceAndCopy(line, MAXDELEN)) {
    int rv = parse_aliasf(line, &iterator);
    if (rv)
      return rv;
  }

  // Read in the regular commands from the affix file. We only care about the
  // IGNORE line here. The rest of the commands will be read by the affix
  // manager.
  iterator = bdict_reader->GetOtherLineIterator();
  while (iterator.AdvanceAndCopy(line, MAXDELEN)) {
    // Parse in the ignored characters (for example, Arabic optional
    // diacritics characters.
    if (strncmp(line,"IGNORE",6) == 0) {
      parse_array(line, &ignorechars, &ignorechars_utf16,
                  &ignorechars_utf16_len, "IGNORE", utf8);
      break;  // All done.
    }
  }

  return 0;
}
#else
// read in aff file and set flag mode
int  HashMgr::load_config(FILE* aff_handle)
{
  int firstline = 1;
  
  // io buffers
  char line[MAXDELEN+1];
 
  // open the affix file
  FILE * afflst;
  afflst = _fdopen(_dup(_fileno(aff_handle)), "r");
  if (!afflst) {
    HUNSPELL_WARNING(stderr, "Error - could not open affix description file\n");
    return 1;
  }
  fseek(afflst, 0, SEEK_SET);

    // read in each line ignoring any that do not
    // start with a known line type indicator

    while (fgets(line,MAXDELEN,afflst)) {
        mychomp(line);

       /* remove byte order mark */
       if (firstline) {
         firstline = 0;
         if (strncmp(line,"\xef\xbb\xbf",3) == 0) memmove(line, line+3, strlen(line+3)+1);
       }

        /* parse in the try string */
        if ((strncmp(line,"FLAG",4) == 0) && isspace(line[4])) {
            if (flag_mode != FLAG_CHAR) {
                HUNSPELL_WARNING(stderr, "error: duplicate FLAG parameter\n");
            }
            if (strstr(line, "long")) flag_mode = FLAG_LONG;
            if (strstr(line, "num")) flag_mode = FLAG_NUM;
            if (strstr(line, "UTF-8")) flag_mode = FLAG_UNI;
            if (flag_mode == FLAG_CHAR) {
                HUNSPELL_WARNING(stderr, "error: FLAG need `num', `long' or `UTF-8' parameter: %s\n", line);
            }
        }
        if ((strncmp(line,"SET",3) == 0) && isspace(line[3]) && strstr(line, "UTF-8")) utf8 = 1;

       /* parse in the ignored characters (for example, Arabic optional diacritics characters */
       if (strncmp(line,"IGNORE",6) == 0) {
          if (parse_array(line, &ignorechars, &ignorechars_utf16, &ignorechars_utf16_len, "IGNORE", utf8)) {
             fclose(afflst);
             return 1;
          }
       }

       if ((strncmp(line,"AF",2) == 0) && isspace(line[2])) {
          if (parse_aliasf(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }

#ifdef HUNSPELL_EXPERIMENTAL
       if ((strncmp(line,"AM",2) == 0) && isspace(line[2])) {
          if (parse_aliasm(line, afflst)) {
             fclose(afflst);
             return 1;
          }
       }
#endif
        if (strncmp(line,"COMPLEXPREFIXES",15) == 0) complexprefixes = 1;
        if (((strncmp(line,"SFX",3) == 0) || (strncmp(line,"PFX",3) == 0)) && isspace(line[3])) break;
    }
    fclose(afflst);
    return 0;
}
#endif  // HUNSPELL_CHROME_CLIENT

/* parse in the ALIAS table */
#ifdef HUNSPELL_CHROME_CLIENT
int HashMgr::parse_aliasf(char* line, hunspell::LineIterator* iterator)
{
#else
int  HashMgr::parse_aliasf(char * line, FILE * af)
{
#endif
   if (numaliasf != 0) {
      HUNSPELL_WARNING(stderr, "error: duplicate AF (alias for flag vector) tables used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
                       numaliasf = atoi(piece);
                       if (numaliasf < 1) {
                          numaliasf = 0;
                          aliasf = NULL;
                          aliasflen = NULL;
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in AF table\n");
                          free(piece);
                          return 1;
                       }
                       aliasf = (unsigned short **) malloc(numaliasf * sizeof(unsigned short *));
                       aliasflen = (unsigned short *) malloc(numaliasf * sizeof(short));
                       if (!aliasf || !aliasflen) {
                          numaliasf = 0;
                          if (aliasf) free(aliasf);
                          if (aliasflen) free(aliasflen);
                          aliasf = NULL;
                          aliasflen = NULL;
                          return 1;
                       }
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       free(piece);
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      numaliasf = 0;
      free(aliasf);
      free(aliasflen);
      aliasf = NULL;
      aliasflen = NULL;
      HUNSPELL_WARNING(stderr, "error: missing AF table information\n");
      return 1;
   } 
 
   /* now parse the numaliasf lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numaliasf; j++) {
#ifdef HUNSPELL_CHROME_CLIENT
        if (!iterator->AdvanceAndCopy(nl, MAXDELEN))
          return 1;
#else
        if (!fgets(nl,MAXDELEN,af)) return 1;
#endif
        mychomp(nl);
        tp = nl;
        i = 0;
        aliasf[j] = NULL;
        aliasflen[j] = 0;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece,"AF",2) != 0) {
                                 numaliasf = 0;
                                 free(aliasf);
                                 free(aliasflen);
                                 aliasf = NULL;
                                 aliasflen = NULL;
                                 HUNSPELL_WARNING(stderr, "error: AF table is corrupt\n");
                                 free(piece);
                                 return 1;
                             }
                             break;
                          }
                  case 1: {
                            aliasflen[j] = (unsigned short) decode_flags(&(aliasf[j]), piece);
                            flag_qsort(aliasf[j], 0, aliasflen[j]);
                            break; 
                          }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if (!aliasf[j]) {
             free(aliasf);
             free(aliasflen);
             aliasf = NULL;
             aliasflen = NULL;
             numaliasf = 0;
             HUNSPELL_WARNING(stderr, "error: AF table is corrupt\n");
             return 1;
        }
   }
   return 0;
}

#ifdef HUNSPELL_CHROME_CLIENT
hentry* HashMgr::AffixIDsToHentry(char* word,
                                  int* affix_ids, 
                                  int affix_count) const
{
  if (affix_count == 0)
    return NULL;

  HEntryCache& cache = const_cast<HashMgr*>(this)->hentry_cache;
  std::string std_word(word);
  HEntryCache::iterator found = cache.find(std_word);
  if (found != cache.end()) {
    // We must return an existing hentry for the same word if we've previously
    // handed one out. Hunspell will compare pointers in some cases to see if
    // two words it has found are the same.
    return found->second;
  }

  short word_len = static_cast<short>(strlen(word));

  // We can get a number of prefixes per word. There will normally be only one,
  // but if not, there will be a linked list of "hentry"s for the "homonym"s 
  // for the word.
  struct hentry* first_he = NULL;
  struct hentry* prev_he = NULL;  // For making linked list.
  for (int i = 0; i < affix_count; i++) {
    struct hentry* he = new hentry;
    if (i == 0)
      first_he = he;
    he->word = word;
    he->wlen = word_len;
    he->alen = (short)const_cast<HashMgr*>(this)->get_aliasf(affix_ids[i],
                                                             &he->astr);
    he->next = NULL;
    he->next_homonym = NULL;
    if (prev_he)
      prev_he->next_homonym = he;
    prev_he = he;
  }

  cache[std_word] = first_he;  // Save this word in the cache for later.
  return first_he;
}

#endif

#ifdef HUNSPELL_CHROME_CLIENT
hentry* HashMgr::GetHentryFromHEntryCache(char* word) {
  HEntryCache& cache = const_cast<HashMgr*>(this)->hentry_cache;
  std::string std_word(word);
  HEntryCache::iterator found = cache.find(std_word);
  if (found != cache.end())
    return found->second;
  else
    return NULL;
}

#endif

int HashMgr::is_aliasf() {
    return (aliasf != NULL);
}

int HashMgr::get_aliasf(int index, unsigned short ** fvec) {
    if ((index > 0) && (index <= numaliasf)) {
        *fvec = aliasf[index - 1];
        return aliasflen[index - 1];
    }
    HUNSPELL_WARNING(stderr, "error: bad flag alias index: %d\n", index);
    *fvec = NULL;
    return 0;
}

#ifdef HUNSPELL_EXPERIMENTAL
/* parse morph alias definitions */
int  HashMgr::parse_aliasm(char * line, FILE * af)
{
   if (numaliasm != 0) {
      HUNSPELL_WARNING(stderr, "error: duplicate AM (aliases for morphological descriptions) tables used\n");
      return 1;
   }
   char * tp = line;
   char * piece;
   int i = 0;
   int np = 0;
   piece = mystrsep(&tp, 0);
   while (piece) {
       if (*piece != '\0') {
          switch(i) {
             case 0: { np++; break; }
             case 1: { 
                       numaliasm = atoi(piece);
                       if (numaliasm < 1) {
                          HUNSPELL_WARNING(stderr, "incorrect number of entries in AM table\n");
                          free(piece);
                          return 1;
                       }
                       aliasm = (char **) malloc(numaliasm * sizeof(char *));
                       if (!aliasm) {
                          numaliasm = 0;
                          return 1;
                       }
                       np++;
                       break;
                     }
             default: break;
          }
          i++;
       }
       free(piece);
       piece = mystrsep(&tp, 0);
   }
   if (np != 2) {
      numaliasm = 0;
      free(aliasm);
      aliasm = NULL;
      HUNSPELL_WARNING(stderr, "error: missing AM alias information\n");
      return 1;
   } 

   /* now parse the numaliasm lines to read in the remainder of the table */
   char * nl = line;
   for (int j=0; j < numaliasm; j++) {
        if (!fgets(nl,MAXDELEN,af)) return 1;
        mychomp(nl);
        tp = nl;
        i = 0;
        aliasm[j] = NULL;
        piece = mystrsep(&tp, 0);
        while (piece) {
           if (*piece != '\0') {
               switch(i) {
                  case 0: {
                             if (strncmp(piece,"AM",2) != 0) {
                                 HUNSPELL_WARNING(stderr, "error: AM table is corrupt\n");
                                 free(piece);
                                 numaliasm = 0;
                                 free(aliasm);
                                 aliasm = NULL;
                                 return 1;
                             }
                             break;
                          }
                  case 1: {
                            if (complexprefixes) {
                                if (utf8) reverseword_utf(piece);
                                    else reverseword(piece);
                            }
                            aliasm[j] = mystrdup(piece);
                            break; }
                  default: break;
               }
               i++;
           }
           free(piece);
           piece = mystrsep(&tp, 0);
        }
        if (!aliasm[j]) {
             numaliasm = 0;
             free(aliasm);
             aliasm = NULL;
             HUNSPELL_WARNING(stderr, "error: map table is corrupt\n");
             return 1;
        }
   }
   return 0;
}

int HashMgr::is_aliasm() {
    return (aliasm != NULL);
}

char * HashMgr::get_aliasm(int index) {
    if ((index > 0) && (index <= numaliasm)) return aliasm[index - 1];
    HUNSPELL_WARNING(stderr, "error: bad morph. alias index: %d\n", index);
    return NULL;
}
#endif
