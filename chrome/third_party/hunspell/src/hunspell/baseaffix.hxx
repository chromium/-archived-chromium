#ifndef _BASEAFF_HXX_
#define _BASEAFF_HXX_

class AffEntry
{
public:

protected:
       char *       appnd;
       char *       strip;
       unsigned char  appndl;
       unsigned char  stripl;
       char         numconds;
       char  opts;
       unsigned short aflag;
       union {
         char   base[SETSIZE];
         struct {
                char  ascii[SETSIZE/2];
                char neg[8];
                char all[8];
                w_char * wchars[8];
                int wlen[8];
         } utf8;
       } conds;
       char *       morphcode;
       unsigned short * contclass;
       short        contclasslen;
};

#endif
