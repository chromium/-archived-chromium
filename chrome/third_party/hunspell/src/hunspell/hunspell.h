#ifndef _MYSPELLMGR_H_
#define _MYSPELLMGR_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Hunhandle Hunhandle;

Hunhandle *Hunspell_create(const char * affpath, const char * dpath);
void Hunspell_destroy(Hunhandle *pHunspell);

/* spell(word) - spellcheck word
 * output: 0 = bad word, not 0 = good word
 */
int Hunspell_spell(Hunhandle *pHunspell, const char *);

char *Hunspell_get_dic_encoding(Hunhandle *pHunspell);

/* suggest(suggestions, word) - search suggestions
 * input: pointer to an array of strings pointer and the (bad) word
 *   array of strings pointer (here *slst) may not be initialized
 * output: number of suggestions in string array, and suggestions in
 *   a newly allocated array of strings (*slts will be NULL when number
 *   of suggestion equals 0.)
 */
int Hunspell_suggest(Hunhandle *pHunspell, char*** slst, const char * word);

#ifdef __cplusplus
}
#endif

#endif
