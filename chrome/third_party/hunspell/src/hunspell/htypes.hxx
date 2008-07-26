#ifndef _HTYPES_HXX_
#define _HTYPES_HXX_

#define MAXDELEN    8192

#define ROTATE_LEN   5

#define ROTATE(v,q) \
   (v) = ((v) << (q)) | (((v) >> (32 - q)) & ((1 << (q))-1));
// approx. number  of user defined words
#define USERWORD 1000

struct hentry
{
  short    wlen;
  short    alen;
  /* NOTE:  Removed by mbelshe since this is not used.
   * The english dictionary is 63K in size, so removing this 
   * itty bitty field saves us ~250KB of RAM.
  char     wbeg[2];
  */
  char *   word;
  unsigned short * astr;
  struct   hentry * next;
  struct   hentry * next_homonym;
#ifdef HUNSPELL_EXPERIMENTAL
  char *   description;
#endif
};

#endif
