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

#ifndef BASE_STRING_TOKENIZER_H__
#define BASE_STRING_TOKENIZER_H__

#include <string>

// StringTokenizerT is a simple string tokenizer class.  It works like an
// iterator that with each step (see the Advance method) updates members that
// refer to the next token in the input string.  The user may optionally
// configure the tokenizer to return delimiters.
//
//
// EXAMPLE 1:
//
//   StringTokenizer t("this is a test", " ");
//   while (t.GetNext()) {
//     printf("%s\n", t.token().c_str());
//   }
//
// Output:
//
//   this
//   is
//   a
//   test
//
//
// EXAMPLE 2:
//
//   StringTokenizer t("no-cache=\"foo, bar\", private", ", ");
//   t.set_quote_chars("\"");
//   while (t.GetNext()) {
//     printf("%s\n", t.token().c_str());
//   }
//
// Output:
//
//   no-cache="foo, bar"
//   private
//
//
// EXAMPLE 3:
//
//   bool next_is_option = false, next_is_value = false;
//   std::string input = "text/html; charset=UTF-8; foo=bar";
//   StringTokenizer t(input, "; =");
//   t.set_options(StringTokenizer::RETURN_DELIMS);
//   while (t.GetNext()) {
//     if (t.token_is_delim()) {
//       switch (*t.token_begin()) {
//         case ';':
//           next_is_option = true;
//           break;
//         case '=':
//           next_is_value = true;
//           break;
//       }
//     } else {
//       const char* label;
//       if (next_is_option) {
//         label = "option-name";
//         next_is_option = false;
//       } else if (next_is_value) {
//         label = "option-value";
//         next_is_value = false;
//       } else {
//         label = "mime-type";
//       }
//       printf("%s: %s\n", label, t.token().c_str());
//     }
//   }
//
//
template <class str>
class StringTokenizerT {
 public:
  typedef typename str::const_iterator const_iterator;
  typedef typename str::value_type char_type;

  // Options that may be pass to set_options()
  enum {
    // Specifies the delimiters should be returned as tokens
    RETURN_DELIMS = 1 << 0,
  };

  StringTokenizerT(const str& string,
                   const str& delims) {
    Init(string.begin(), string.end(), delims);
  }

  StringTokenizerT(const_iterator string_begin,
                   const_iterator string_end,
                   const str& delims) {
    Init(string_begin, string_end, delims);
  }

  // Set the options for this tokenizer.  By default, this is 0.
  void set_options(int options) { options_ = options; }

  // Set the characters to regard as quotes.  By default, this is empty.  When
  // a quote char is encountered, the tokenizer will switch into a mode where
  // it ignores delimiters that it finds.  It switches out of this mode once it
  // finds another instance of the quote char.  If a backslash is encountered
  // within a quoted string, then the next character is skipped.
  void set_quote_chars(const std::string& quotes) { quotes_ = quotes; }

  // Call this method to advance the tokenizer to the next delimiter.  This
  // returns false if the tokenizer is complete.  This method must be called
  // before calling any of the token* methods.
  bool GetNext() {
    AdvanceState state;
    token_is_delim_ = false;
    for (;;) {
      token_begin_ = token_end_;
      if (token_end_ == end_)
        return false;
      ++token_end_;
      if (AdvanceOne(&state, *token_begin_))
        break;
      if (options_ & RETURN_DELIMS) {
        token_is_delim_ = true;
        return true;
      }
      // else skip over delim
    }
    while (token_end_ != end_ && AdvanceOne(&state, *token_end_))
      ++token_end_;
    return true;
  }

  // Returns true if token is a delimiter.  When the tokenizer is constructed
  // with the RETURN_DELIMS option, this method can be used to check if the
  // returned token is actually a delimiter.
  bool token_is_delim() const { return token_is_delim_; }

  // If GetNext() returned true, then these methods may be used to read the
  // value of the token.
  const_iterator token_begin() const { return token_begin_; }
  const_iterator token_end() const { return token_end_; }
  str token() const { return str(token_begin_, token_end_); }

 private:
  void Init(const_iterator string_begin,
            const_iterator string_end,
            const str& delims) {
    token_end_ = string_begin;
    end_ = string_end;
    delims_ = delims;
    options_ = 0;
  }

  bool IsDelim(char_type c) const {
    return delims_.find(c) != str::npos;
  }

  bool IsQuote(char_type c) const {
    return quotes_.find(c) != str::npos;
  }

  struct AdvanceState {
    bool in_quote;
    bool in_escape;
    char_type quote_char;
    AdvanceState() : in_quote(false), in_escape(false) {}
  };

  // Returns true if a delimiter was not hit.
  bool AdvanceOne(AdvanceState* state, char_type c) {
    if (state->in_quote) {
      if (state->in_escape) {
        state->in_escape = false;
      } else if (c == '\\') {
        state->in_escape = true;
      } else if (c == state->quote_char) {
        state->in_quote = false;
      }
    } else {
      if (IsDelim(c))
        return false;
      state->in_quote = IsQuote(state->quote_char = c);
    }
    return true;
  }

  const_iterator token_begin_;
  const_iterator token_end_;
  const_iterator end_;
  str delims_;
  str quotes_;
  int options_;
  bool token_is_delim_;
};

typedef StringTokenizerT<std::string> StringTokenizer;
typedef StringTokenizerT<std::wstring> WStringTokenizer;

#endif  // BASE_STRING_TOKENIZER_H__
