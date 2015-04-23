/*
 * parser classes for MySpell
 *
 * implemented: text, HTML, TeX
 *
 * Copyright (C) 2002, Laszlo Nemeth
 *
 */

#ifndef _TEXTPARSER_HXX_
#define _TEXTPARSER_HXX_

// set sum of actual and previous lines
#define MAXPREVLINE 4

#define MAXLNLEN        8192 * 4

/*
 * Base Text Parser
 *
 */

class TextParser
{

protected:
  void                init(const char *);
  void                init(unsigned short * wordchars, int len);
  int                 wordcharacters[256]; // for detection of the word boundaries
  char                line[MAXPREVLINE][MAXLNLEN]; // parsed and previous lines
  int                 actual; // actual line
  int                 head;   // head position
  int                 token;  // begin of token
  int                 state;  // state of automata
  int                 utf8;   // UTF-8 character encoding
  int                 next_char(char * line, int * pos);
  unsigned short *    wordchars_utf16;
  int                 wclen;

public:
 
  TextParser();
  TextParser(unsigned short * wordchars, int len);
  TextParser(const char * wc);
  virtual ~TextParser();

  void                put_line(char * line);
  char *              get_line();
  char *              get_prevline(int n);
  virtual char *      next_token();
  int                 change_token(const char * word);
  int                 get_tokenpos();
  int                 is_wordchar(char * w);
  char *              get_latin1(char * s);
  char *              next_char();
  
};

#endif

