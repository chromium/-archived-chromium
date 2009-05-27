/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Custom error reporting functon for the Technique parser, as described in
// the Antlr3 documention for the C Runtime at
// http://www.antlr.org/api/C/using.html
//
// NOTE: These error functions are only designed to work with 8-bit
// token streams.

#include "core/cross/types.h"
#include "compiler/technique/technique_error.h"

namespace o3d {
String* error_string;

void TechniqueSetErrorString(String* e) {
  error_string = e;
}

void TechniqueError(pANTLR3_BASE_RECOGNIZER recognizer,
                    pANTLR3_UINT8 * tokenNames) {
  // If the error string pointer has not been set, bail now.
  if (!error_string) return;

  pANTLR3_PARSER parser;
  pANTLR3_STRING token_text;
  pANTLR3_EXCEPTION exception;
  pANTLR3_COMMON_TOKEN token;
  pANTLR3_BASE_TREE theBaseTree;
  pANTLR3_COMMON_TREE theCommonTree;

  // Retrieve some info for easy reading.
  exception = recognizer->state->exception;
  token_text = NULL;

  pANTLR3_COMMON_TOKEN exception_token =
      static_cast<pANTLR3_COMMON_TOKEN>(exception->token);
  if (exception_token && exception_token->custom) {
    // The filename is retrieved from the "custom" field of the token, which
    // was put there by the lexer.  The lexer gets it from the #line directives
    // in the pre-processed output.
    pANTLR3_STRING filename =
        static_cast<pANTLR3_STRING>(exception_token->custom);
    *error_string += reinterpret_cast<char*>(filename->chars);
  } else {
    if (exception_token && exception_token->type == ANTLR3_TOKEN_EOF) {
      *error_string += "End of input";
    } else {
      *error_string += "Unknown source";
    }
  }

  // Next comes the line number
  char buffer[10];
  base::snprintf(buffer, sizeof(buffer), "%d", exception->line);
  *error_string += "(";
  *error_string += buffer;
  *error_string += ")";

  *error_string += ": Error: ";
  *error_string += reinterpret_cast<char*>(exception->message);

  // Find out what part of the system raised the error.
  switch (recognizer->type) {
    case ANTLR3_TYPE_PARSER: {
      // This is a normal Parser error. Prepare the information we have
      // available.
      parser = (pANTLR3_PARSER)(recognizer->super);
      token = (pANTLR3_COMMON_TOKEN)(exception->token);

      *error_string += ", at offset ";
      base::snprintf(buffer, sizeof(buffer), "%d",
                     exception->charPositionInLine);
      *error_string += buffer;
      if (token != NULL) {
        if (token->type == ANTLR3_TOKEN_EOF) {
          ANTLR3_FPRINTF(stderr, ", at <EOF>");
        } else {
          token_text = token->getText(token);
          if (token_text != NULL) {
            *error_string += " near \"";
            *error_string += reinterpret_cast<const char*>(token_text->chars);
            *error_string += "\"";
          }
        }
      }
      break;
    }
    case  ANTLR3_TYPE_LEXER: {
      *error_string += "lexer error.";
      break;
    }
    case ANTLR3_TYPE_TREE_PARSER: {
      // Tree parsers not supported. Exit.
      DLOG(FATAL) << "Technique error should never see a Tree Parser.";
      return;
    }
    default: {
      // Parser type was not recognised. Exit.
      DLOG(FATAL) << "Technique error called by an unknown Parser type.";
      return;
    }
  }

#if 0
  // Although this function should generally be provided by the
  // implementation, this one should be as helpful as possible for grammar
  // developers and serve as an example of what you can do with each
  // exception type. In general, when you make up your 'real' handler, you
  // should debug the routine with all possible errors you expect which will
  // then let you be as specific as possible about all circumstances.
  //
  // Note that in the general case, errors thrown by tree parsers indicate a
  // problem with the output of the parser or with the tree grammar
  // itself. The job of the parser is to produce a perfect (in traversal
  // terms) syntactically correct tree, so errors at that stage should
  // really be semantic errors that your own code determines and handles in
  // whatever way is appropriate.
  switch (exception->type) {
    case ANTLR3_UNWANTED_TOKEN_EXCEPTION:
      // Indicates that the recognizer was fed a token which seesm to be
      // spurious input. We can detect this when the token that follows
      // this unwanted token would normally be part of the syntactically
      // correct stream. Then we can see that the token we are looking at
      // is just something that should not be there and throw this exception.
      if (tokenNames == NULL) {
        ANTLR3_FPRINTF(stderr, " : Extraneous input...");
      } else {
        if (exception->expecting == ANTLR3_TOKEN_EOF) {
          ANTLR3_FPRINTF(stderr, " : Extraneous input - expected <EOF>\n");
        } else {
          ANTLR3_FPRINTF(stderr,
                         " : Extraneous input - expected %s ...\n",
                         tokenNames[exception->expecting]);
        }
      }
      break;
    case ANTLR3_MISSING_TOKEN_EXCEPTION:
      // Indicates that the recognizer detected that the token we just
      // hit would be valid syntactically if preceeded by a particular
      // token. Perhaps a missing ';' at line end or a missing ',' in an
      // expression list, and such like.
      if (tokenNames == NULL) {
        ANTLR3_FPRINTF(stderr,
                       " : Missing token (%d)...\n",
                       exception->expecting);
      } else {
        if (exception->expecting == ANTLR3_TOKEN_EOF) {
          ANTLR3_FPRINTF(stderr, " : Missing <EOF>\n");
        } else {
          ANTLR3_FPRINTF(stderr,
                         " : Missing %s \n",
                         tokenNames[exception->expecting]);
        }
      }
      break;
    case ANTLR3_RECOGNITION_EXCEPTION:
      // Indicates that the recognizer received a token
      // in the input that was not predicted. This is the basic exception type
      // from which all others are derived. So we assume it was a syntax error.
      // You may get this if there are not more tokens and more are needed
      // to complete a parse for instance.
      ANTLR3_FPRINTF(stderr, " : syntax error...\n");
      break;
    case ANTLR3_MISMATCHED_TOKEN_EXCEPTION:
      // We were expecting to see one thing and got another. This is the
      // most common error if we coudl not detect a missing or unwanted token.
      // Here you can spend your efforts to
      // derive more useful error messages based on the expected
      // token set and the last token and so on. The error following
      // bitmaps do a good job of reducing the set that we were looking
      // for down to something small. Knowing what you are parsing may be
      // able to allow you to be even more specific about an error.
      if (tokenNames == NULL) {
        ANTLR3_FPRINTF(stderr, " : syntax error...\n");
      } else {
        if (exception->expecting == ANTLR3_TOKEN_EOF) {
          ANTLR3_FPRINTF(stderr, " : expected <EOF>\n");
        } else {
          ANTLR3_FPRINTF(stderr,
                         " : expected %s ...\n",
                         tokenNames[exception->expecting]);
        }
      }
      break;
    case ANTLR3_NO_VIABLE_ALT_EXCEPTION:
      // We could not pick any alt decision from the input given
      // so god knows what happened - however when you examine your grammar,
      // you should. It means that at the point where the current token occurred
      // that the DFA indicates nowhere to go from here.
      ANTLR3_FPRINTF(stderr, " : cannot match to any predicted input...\n");
      break;
    case ANTLR3_MISMATCHED_SET_EXCEPTION: {
      ANTLR3_UINT32 count;
      ANTLR3_UINT32 bit;
      ANTLR3_UINT32 size;
      ANTLR3_UINT32 numbits;
      pANTLR3_BITSET errBits;

      // This means we were able to deal with one of a set of
      // possible tokens at this point, but we did not see any
      // member of that set.
      ANTLR3_FPRINTF(stderr, " : unexpected input...\n  expected one of : ");

      // What tokens could we have accepted at this point in the
      // parse?
      count = 0;
      errBits = antlr3BitsetLoad(exception->expectingSet);
      numbits = errBits->numBits(errBits);
      size = errBits->size(errBits);

      if (size > 0) {
        // However many tokens we could have dealt with here, it is usually
        // not useful to print ALL of the set here. I arbitrarily chose 8
        // here, but you should do whatever makes sense for you of course.
        // No token number 0, so look for bit 1 and on.
        for  (bit = 1; bit < numbits && count < 8 && count < size; bit++) {
          // TODO: This doesn't look right - should be asking if the bit is set!
          if (tokenNames[bit]) {
            ANTLR3_FPRINTF(stderr,
                           "%%s", count > 0 ? ", " : "",
                           tokenNames[bit]);
            count++;
          }
        }
        ANTLR3_FPRINTF(stderr, "\n");
      } else {
        ANTLR3_FPRINTF(stderr,
                       "Could not work out which set element was missing.\n");
      }
      break;
    }
    case ANTLR3_EARLY_EXIT_EXCEPTION:
      // We entered a loop requiring a number of token sequences
      // but found a token that ended that sequence earlier than
      // we should have done.
      ANTLR3_FPRINTF(stderr, " : missing elements...\n");
      break;
    default:
      // We don't handle any other exceptions here, but you can
      // if you wish. If we get an exception that hits this point
      // then we are just going to report what we know about the
      // token.
      ANTLR3_FPRINTF(stderr, " : syntax not recognized...\n");
      break;
  }
#endif  // end #if 0

  *error_string += "\n";

  DLOG(INFO) << "parse error:  " << error_string->c_str();
}
}  // namespace o3d
