#ifndef _AFFIXMGR_HXX_
#define _AFFIXMGR_HXX_

#ifdef MOZILLA_CLIENT
#ifdef __SUNPRO_CC // for SunONE Studio compiler
using namespace std;
#endif
#include <stdio.h>
#else
#include <cstdio>
#endif

#include "atypes.hxx"
#include "baseaffix.hxx"
#include "hashmgr.hxx"

// check flag duplication
#define dupSFX        (1 << 0)
#define dupPFX        (1 << 1)

#ifdef HUNSPELL_CHROME_CLIENT

#include <vector>

// This class provides an implementation of the contclasses array in AffixMgr
// that is normally a large static array. We should almost never need more than
// 256 elements, so this class only allocates that much to start off with. If
// elements higher than that are actually used, we'll automatically expand.
class ContClasses {
 public:
  ContClasses() {
    // Pre-allocate a buffer so that typically, we'll never have to resize.
    EnsureSizeIs(256);
  }

  char& operator[](size_t index) {
    EnsureSizeIs(index + 1);
    return data[index];
  }

  void EnsureSizeIs(size_t new_size) {
    if (data.size() >= new_size)
      return;  // Nothing to do.

    size_t old_size = data.size();
    data.resize(new_size);
    memset(&data[old_size], 0, new_size - old_size);
  }

  std::vector<char> data;
};

#endif  // HUNSPELL_CHROME_CLIENT

class AffixMgr
{

  AffEntry *          pStart[SETSIZE];
  AffEntry *          sStart[SETSIZE];
#ifdef HUNSPELL_CHROME_CLIENT
  // It doesn't appear that these pointers are used beyond SETSIZE.
  AffEntry *          pFlag[SETSIZE];
  AffEntry *          sFlag[SETSIZE];
#else
  AffEntry *          pFlag[CONTSIZE];
  AffEntry *          sFlag[CONTSIZE];
#endif
  HashMgr *           pHMgr;
  char *              trystring;
  char *              encoding;
  struct cs_info *    csconv;
  int                 utf8;
  int                 complexprefixes;
  FLAG                compoundflag;  
  FLAG                compoundbegin;
  FLAG                compoundmiddle;
  FLAG                compoundend;
  FLAG                compoundroot;
  FLAG                compoundforbidflag;
  FLAG                compoundpermitflag;
  int                 checkcompounddup;
  int                 checkcompoundrep;
  int                 checkcompoundcase;
  int                 checkcompoundtriple;
  FLAG                forbiddenword;
  FLAG                nosuggest;
  FLAG                pseudoroot;
  int                 cpdmin;
  int                 numrep;
  replentry *         reptable;
  int                 nummap;
  mapentry *          maptable;
  int                 numbreak;
  char **             breaktable;
  int                 numcheckcpd;
  replentry *         checkcpdtable;
  int                 numdefcpd;
  flagentry *         defcpdtable;
  int                 maxngramsugs;
  int                 nosplitsugs;
  int                 sugswithdots;
  int                 cpdwordmax;
  int                 cpdmaxsyllable;
  char *              cpdvowels;
  w_char *            cpdvowels_utf16;
  int                 cpdvowels_utf16_len;
  char *              cpdsyllablenum;
  const char *        pfxappnd; // BUG: not stateless
  const char *        sfxappnd; // BUG: not stateless
  FLAG                sfxflag;  // BUG: not stateless
  char *              derived;  // BUG: not stateless
  AffEntry *          sfx;      // BUG: not stateless
  AffEntry *          pfx;      // BUG: not stateless
  int                 checknum;
  char *              wordchars;
  unsigned short *    wordchars_utf16;
  int                 wordchars_utf16_len;
  char *              ignorechars;
  unsigned short *    ignorechars_utf16;
  int                 ignorechars_utf16_len;
  char *              version;
  char *              lang;
  int                 langnum;
  FLAG                lemma_present;
  FLAG                circumfix;
  FLAG                onlyincompound;
  FLAG                keepcase;
  int                 checksharps;

  int                 havecontclass; // boolean variable
#ifdef HUNSPELL_CHROME_CLIENT
  ContClasses         contclasses;
#else
  char                contclasses[CONTSIZE]; // flags of possible continuing classes (twofold affix)
#endif
  flag                flag_mode;
  
public:
#ifdef HUNSPELL_CHROME_CLIENT
  AffixMgr(hunspell::BDictReader* reader, HashMgr* ptr);
#else
  AffixMgr(FILE* aff_handle, HashMgr * ptr);
#endif

  ~AffixMgr();
  struct hentry *     affix_check(const char * word, int len,
            const unsigned short needflag = (unsigned short) 0, char in_compound = IN_CPD_NOT);
  struct hentry *     prefix_check(const char * word, int len,
            char in_compound, const FLAG needflag = FLAG_NULL);
  inline int isSubset(const char * s1, const char * s2);
  struct hentry *     prefix_check_twosfx(const char * word, int len,
            char in_compound, const FLAG needflag = FLAG_NULL);
  inline int isRevSubset(const char * s1, const char * end_of_s2, int len);
  struct hentry *     suffix_check(const char * word, int len, int sfxopts, AffEntry* ppfx,
                        char ** wlst, int maxSug, int * ns, const FLAG cclass = FLAG_NULL,
                        const FLAG needflag = FLAG_NULL, char in_compound = IN_CPD_NOT);
  struct hentry *     suffix_check_twosfx(const char * word, int len,
            int sfxopts, AffEntry* ppfx, const FLAG needflag = FLAG_NULL);

  char * affix_check_morph(const char * word, int len,
                    const FLAG needflag = FLAG_NULL, char in_compound = IN_CPD_NOT);
  char * prefix_check_morph(const char * word, int len,
                    char in_compound, const FLAG needflag = FLAG_NULL);
  char * suffix_check_morph (const char * word, int len, int sfxopts, AffEntry * ppfx,
            const FLAG cclass = FLAG_NULL, const FLAG needflag = FLAG_NULL, char in_compound = IN_CPD_NOT);

  char * prefix_check_twosfx_morph(const char * word, int len,
            char in_compound, const FLAG needflag = FLAG_NULL);
  char * suffix_check_twosfx_morph(const char * word, int len,
            int sfxopts, AffEntry * ppfx, const FLAG needflag = FLAG_NULL);

  int                 expand_rootword(struct guessword * wlst, int maxn, const char * ts,
                        int wl, const unsigned short * ap, unsigned short al, char * bad, int);

  short               get_syllable (const char * word, int wlen);
  int                 cpdrep_check(const char * word, int len);
  int                 cpdpat_check(const char * word, int len);
  int                 defcpd_check(hentry *** words, short wnum, hentry * rv, hentry ** rwords, char all);
  int                 cpdcase_check(const char * word, int len);
  inline int                 candidate_check(const char * word, int len);
  struct hentry *     compound_check(const char * word, int len,
                              short wordnum, short numsyllable, short maxwordnum, short wnum, hentry ** words,
                              char hu_mov_rule, int * cmpdstemnum, int * cmpdstem, char is_sug);

  int compound_check_morph(const char * word, int len,
                              short wordnum, short numsyllable, short maxwordnum, short wnum, hentry ** words,
                              char hu_mov_rule, char ** result, char * partresult);

  struct hentry *     lookup(const char * word);
  int                 get_numrep();
  struct replentry *  get_reptable();
  int                 get_nummap();
  struct mapentry *   get_maptable();
  int                 get_numbreak();
  char **             get_breaktable();
  char *              get_encoding();
  int                 get_langnum();
  char *              get_try_string();
  const char *        get_wordchars();
  unsigned short *    get_wordchars_utf16(int * len);
  char *              get_ignore();
  unsigned short *    get_ignore_utf16(int * len);
  int                 get_compound();
  FLAG                get_compoundflag();
  FLAG                get_compoundbegin();
  FLAG                get_forbiddenword();
  FLAG                get_nosuggest();
//  FLAG                get_circumfix();
  FLAG                get_pseudoroot();
  FLAG                get_onlyincompound();
  FLAG                get_compoundroot();
  FLAG                get_lemma_present();
  int                 get_checknum();
  char *              get_possible_root();
  const char *        get_prefix();
  const char *        get_suffix();
  const char *        get_derived();
  const char *        get_version();
  const int           have_contclass();
  int                 get_utf8();
  int                 get_complexprefixes();
  char *              get_suffixed(char );
  int                 get_maxngramsugs();
  int                 get_nosplitsugs();
  int                 get_sugswithdots(void);
  FLAG                get_keepcase(void);
  int                 get_checksharps(void);

private:
#ifdef HUNSPELL_CHROME_CLIENT
  // Not owned by us, owned by the Hunspell object.
  hunspell::BDictReader* bdict_reader;

  int  parse_file();
#else
  int  parse_file(FILE* aff_handle);
#endif
//  int  parse_string(char * line, char ** out, const char * name);
  int  parse_flag(char * line, unsigned short * out, const char * name);
  int  parse_num(char * line, int * out, const char * name);
//  int  parse_array(char * line, char ** out, unsigned short ** out_utf16,
//            int * out_utf16_len, const char * name);
  int  parse_cpdsyllable(char * linfe);
#ifdef HUNSPELL_CHROME_CLIENT
  // We just change the FILE* to be an iterator.
  int  parse_maptable(char * line, hunspell::LineIterator* iterator);
  int  parse_checkcpdtable(char * line, hunspell::LineIterator* iterator);
  int  parse_breaktable(char * line, hunspell::LineIterator* iterator);
  int  parse_defcpdtable(char * line, hunspell::LineIterator* iterator);
  int  parse_affix(char * line, const char at, hunspell::LineIterator* iterator);
#else
  int  parse_reptable(char * line, FILE * af);
  int  parse_maptable(char * line, FILE * af);
  int  parse_breaktable(char * line, FILE * af);
  int  parse_checkcpdtable(char * line, FILE * af);
  int  parse_defcpdtable(char * line, FILE * af);
  int  parse_affix(char * line, const char at, FILE * af, char * dupflags);
#endif

  int encodeit(struct affentry * ptr, char * cs);
  int build_pfxtree(AffEntry* pfxptr);
  int build_sfxtree(AffEntry* sfxptr);
  int process_pfx_order();
  int process_sfx_order();
  AffEntry * process_pfx_in_order(AffEntry * ptr, AffEntry * nptr);
  AffEntry * process_sfx_in_order(AffEntry * ptr, AffEntry * nptr);
  int process_pfx_tree_to_list();
  int process_sfx_tree_to_list();
  int redundant_condition(char, char * strip, int stripl, const char * cond, char *);
};

#endif

