/**
 * uri.c: set of generic URI related routines 
 *
 * Reference: RFCs 2396, 2732 and 2373
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXML
#include "libxml.h"

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/uri.h>
#include <libxml/globals.h>
#include <libxml/xmlerror.h>

/************************************************************************
 *									*
 *		Macros to differentiate various character type		*
 *			directly extracted from RFC 2396		*
 *									*
 ************************************************************************/

/*
 * alpha    = lowalpha | upalpha
 */
#define IS_ALPHA(x) (IS_LOWALPHA(x) || IS_UPALPHA(x))


/*
 * lowalpha = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" | "j" |
 *            "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" | "s" | "t" |
 *            "u" | "v" | "w" | "x" | "y" | "z"
 */

#define IS_LOWALPHA(x) (((x) >= 'a') && ((x) <= 'z'))

/*
 * upalpha = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" | "J" |
 *           "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" | "S" | "T" |
 *           "U" | "V" | "W" | "X" | "Y" | "Z"
 */
#define IS_UPALPHA(x) (((x) >= 'A') && ((x) <= 'Z'))

#ifdef IS_DIGIT
#undef IS_DIGIT
#endif
/*
 * digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
 */
#define IS_DIGIT(x) (((x) >= '0') && ((x) <= '9'))

/*
 * alphanum = alpha | digit
 */

#define IS_ALPHANUM(x) (IS_ALPHA(x) || IS_DIGIT(x))

/*
 * hex = digit | "A" | "B" | "C" | "D" | "E" | "F" |
 *               "a" | "b" | "c" | "d" | "e" | "f"
 */

#define IS_HEX(x) ((IS_DIGIT(x)) || (((x) >= 'a') && ((x) <= 'f')) || \
	    (((x) >= 'A') && ((x) <= 'F')))

/*
 * mark = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
 */

#define IS_MARK(x) (((x) == '-') || ((x) == '_') || ((x) == '.') ||	\
    ((x) == '!') || ((x) == '~') || ((x) == '*') || ((x) == '\'') ||	\
    ((x) == '(') || ((x) == ')'))


/*
 * reserved = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | "," |
 * 	      "[" | "]"
 */

#define IS_RESERVED(x) (((x) == ';') || ((x) == '/') || ((x) == '?') ||	\
        ((x) == ':') || ((x) == '@') || ((x) == '&') || ((x) == '=') ||	\
	((x) == '+') || ((x) == '$') || ((x) == ',') || ((x) == '[') || \
	((x) == ']'))

/*
 * unreserved = alphanum | mark
 */

#define IS_UNRESERVED(x) (IS_ALPHANUM(x) || IS_MARK(x))

/*
 * escaped = "%" hex hex
 */

#define IS_ESCAPED(p) ((*(p) == '%') && (IS_HEX((p)[1])) &&		\
	    (IS_HEX((p)[2])))

/*
 * uric_no_slash = unreserved | escaped | ";" | "?" | ":" | "@" |
 *                        "&" | "=" | "+" | "$" | ","
 */
#define IS_URIC_NO_SLASH(p) ((IS_UNRESERVED(*(p))) || (IS_ESCAPED(p)) ||\
	        ((*(p) == ';')) || ((*(p) == '?')) || ((*(p) == ':')) ||\
	        ((*(p) == '@')) || ((*(p) == '&')) || ((*(p) == '=')) ||\
	        ((*(p) == '+')) || ((*(p) == '$')) || ((*(p) == ',')))

/*
 * pchar = unreserved | escaped | ":" | "@" | "&" | "=" | "+" | "$" | ","
 */
#define IS_PCHAR(p) ((IS_UNRESERVED(*(p))) || (IS_ESCAPED(p)) ||	\
	        ((*(p) == ':')) || ((*(p) == '@')) || ((*(p) == '&')) ||\
	        ((*(p) == '=')) || ((*(p) == '+')) || ((*(p) == '$')) ||\
	        ((*(p) == ',')))

/*
 * rel_segment   = 1*( unreserved | escaped |
 *                 ";" | "@" | "&" | "=" | "+" | "$" | "," )
 */

#define IS_SEGMENT(p) ((IS_UNRESERVED(*(p))) || (IS_ESCAPED(p)) ||	\
          ((*(p) == ';')) || ((*(p) == '@')) || ((*(p) == '&')) ||	\
	  ((*(p) == '=')) || ((*(p) == '+')) || ((*(p) == '$')) ||	\
	  ((*(p) == ',')))

/*
 * scheme = alpha *( alpha | digit | "+" | "-" | "." )
 */

#define IS_SCHEME(x) ((IS_ALPHA(x)) || (IS_DIGIT(x)) ||			\
	              ((x) == '+') || ((x) == '-') || ((x) == '.'))

/*
 * reg_name = 1*( unreserved | escaped | "$" | "," |
 *                ";" | ":" | "@" | "&" | "=" | "+" )
 */

#define IS_REG_NAME(p) ((IS_UNRESERVED(*(p))) || (IS_ESCAPED(p)) ||	\
       ((*(p) == '$')) || ((*(p) == ',')) || ((*(p) == ';')) ||		\
       ((*(p) == ':')) || ((*(p) == '@')) || ((*(p) == '&')) ||		\
       ((*(p) == '=')) || ((*(p) == '+')))

/*
 * userinfo = *( unreserved | escaped | ";" | ":" | "&" | "=" |
 *                      "+" | "$" | "," )
 */
#define IS_USERINFO(p) ((IS_UNRESERVED(*(p))) || (IS_ESCAPED(p)) ||	\
       ((*(p) == ';')) || ((*(p) == ':')) || ((*(p) == '&')) ||		\
       ((*(p) == '=')) || ((*(p) == '+')) || ((*(p) == '$')) ||		\
       ((*(p) == ',')))

/*
 * uric = reserved | unreserved | escaped
 */

#define IS_URIC(p) ((IS_UNRESERVED(*(p))) || (IS_ESCAPED(p)) ||		\
	            (IS_RESERVED(*(p))))

/*                                                                              
* unwise = "{" | "}" | "|" | "\" | "^" | "`"
*/                                                                             

#define IS_UNWISE(p)                                                    \
      (((*(p) == '{')) || ((*(p) == '}')) || ((*(p) == '|')) ||         \
       ((*(p) == '\\')) || ((*(p) == '^')) || ((*(p) == '[')) ||        \
       ((*(p) == ']')) || ((*(p) == '`')))  

/*
 * Skip to next pointer char, handle escaped sequences
 */

#define NEXT(p) ((*p == '%')? p += 3 : p++)

/*
 * Productions from the spec.
 *
 *    authority     = server | reg_name
 *    reg_name      = 1*( unreserved | escaped | "$" | "," |
 *                        ";" | ":" | "@" | "&" | "=" | "+" )
 *
 * path          = [ abs_path | opaque_part ]
 */

#define STRNDUP(s, n) (char *) xmlStrndup((const xmlChar *)(s), (n))

/************************************************************************
 *									*
 *			Generic URI structure functions			*
 *									*
 ************************************************************************/

/**
 * xmlCreateURI:
 *
 * Simply creates an empty xmlURI
 *
 * Returns the new structure or NULL in case of error
 */
xmlURIPtr
xmlCreateURI(void) {
    xmlURIPtr ret;

    ret = (xmlURIPtr) xmlMalloc(sizeof(xmlURI));
    if (ret == NULL) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlCreateURI: out of memory\n");
	return(NULL);
    }
    memset(ret, 0, sizeof(xmlURI));
    return(ret);
}

/**
 * xmlSaveUri:
 * @uri:  pointer to an xmlURI
 *
 * Save the URI as an escaped string
 *
 * Returns a new string (to be deallocated by caller)
 */
xmlChar *
xmlSaveUri(xmlURIPtr uri) {
    xmlChar *ret = NULL;
    const char *p;
    int len;
    int max;

    if (uri == NULL) return(NULL);


    max = 80;
    ret = (xmlChar *) xmlMallocAtomic((max + 1) * sizeof(xmlChar));
    if (ret == NULL) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlSaveUri: out of memory\n");
	return(NULL);
    }
    len = 0;

    if (uri->scheme != NULL) {
	p = uri->scheme;
	while (*p != 0) {
	    if (len >= max) {
		max *= 2;
		ret = (xmlChar *) xmlRealloc(ret, (max + 1) * sizeof(xmlChar));
		if (ret == NULL) {
		    xmlGenericError(xmlGenericErrorContext,
			    "xmlSaveUri: out of memory\n");
		    return(NULL);
		}
	    }
	    ret[len++] = *p++;
	}
	if (len >= max) {
	    max *= 2;
	    ret = (xmlChar *) xmlRealloc(ret, (max + 1) * sizeof(xmlChar));
	    if (ret == NULL) {
		xmlGenericError(xmlGenericErrorContext,
			"xmlSaveUri: out of memory\n");
		return(NULL);
	    }
	}
	ret[len++] = ':';
    }
    if (uri->opaque != NULL) {
	p = uri->opaque;
	while (*p != 0) {
	    if (len + 3 >= max) {
		max *= 2;
		ret = (xmlChar *) xmlRealloc(ret, (max + 1) * sizeof(xmlChar));
		if (ret == NULL) {
		    xmlGenericError(xmlGenericErrorContext,
			    "xmlSaveUri: out of memory\n");
		    return(NULL);
		}
	    }
	    if (IS_RESERVED(*(p)) || IS_UNRESERVED(*(p)))
		ret[len++] = *p++;
	    else {
		int val = *(unsigned char *)p++;
		int hi = val / 0x10, lo = val % 0x10;
		ret[len++] = '%';
		ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		ret[len++] = lo + (lo > 9? 'A'-10 : '0');
	    }
	}
    } else {
	if (uri->server != NULL) {
	    if (len + 3 >= max) {
		max *= 2;
		ret = (xmlChar *) xmlRealloc(ret, (max + 1) * sizeof(xmlChar));
		if (ret == NULL) {
		    xmlGenericError(xmlGenericErrorContext,
			    "xmlSaveUri: out of memory\n");
		    return(NULL);
		}
	    }
	    ret[len++] = '/';
	    ret[len++] = '/';
	    if (uri->user != NULL) {
		p = uri->user;
		while (*p != 0) {
		    if (len + 3 >= max) {
			max *= 2;
			ret = (xmlChar *) xmlRealloc(ret,
				(max + 1) * sizeof(xmlChar));
			if (ret == NULL) {
			    xmlGenericError(xmlGenericErrorContext,
				    "xmlSaveUri: out of memory\n");
			    return(NULL);
			}
		    }
		    if ((IS_UNRESERVED(*(p))) ||
			((*(p) == ';')) || ((*(p) == ':')) ||
			((*(p) == '&')) || ((*(p) == '=')) ||
			((*(p) == '+')) || ((*(p) == '$')) ||
			((*(p) == ',')))
			ret[len++] = *p++;
		    else {
			int val = *(unsigned char *)p++;
			int hi = val / 0x10, lo = val % 0x10;
			ret[len++] = '%';
			ret[len++] = hi + (hi > 9? 'A'-10 : '0');
			ret[len++] = lo + (lo > 9? 'A'-10 : '0');
		    }
		}
		if (len + 3 >= max) {
		    max *= 2;
		    ret = (xmlChar *) xmlRealloc(ret,
			    (max + 1) * sizeof(xmlChar));
		    if (ret == NULL) {
			xmlGenericError(xmlGenericErrorContext,
				"xmlSaveUri: out of memory\n");
			return(NULL);
		    }
		}
		ret[len++] = '@';
	    }
	    p = uri->server;
	    while (*p != 0) {
		if (len >= max) {
		    max *= 2;
		    ret = (xmlChar *) xmlRealloc(ret,
			    (max + 1) * sizeof(xmlChar));
		    if (ret == NULL) {
			xmlGenericError(xmlGenericErrorContext,
				"xmlSaveUri: out of memory\n");
			return(NULL);
		    }
		}
		ret[len++] = *p++;
	    }
	    if (uri->port > 0) {
		if (len + 10 >= max) {
		    max *= 2;
		    ret = (xmlChar *) xmlRealloc(ret,
			    (max + 1) * sizeof(xmlChar));
		    if (ret == NULL) {
			xmlGenericError(xmlGenericErrorContext,
				"xmlSaveUri: out of memory\n");
			return(NULL);
		    }
		}
		len += snprintf((char *) &ret[len], max - len, ":%d", uri->port);
	    }
	} else if (uri->authority != NULL) {
	    if (len + 3 >= max) {
		max *= 2;
		ret = (xmlChar *) xmlRealloc(ret,
			(max + 1) * sizeof(xmlChar));
		if (ret == NULL) {
		    xmlGenericError(xmlGenericErrorContext,
			    "xmlSaveUri: out of memory\n");
		    return(NULL);
		}
	    }
	    ret[len++] = '/';
	    ret[len++] = '/';
	    p = uri->authority;
	    while (*p != 0) {
		if (len + 3 >= max) {
		    max *= 2;
		    ret = (xmlChar *) xmlRealloc(ret,
			    (max + 1) * sizeof(xmlChar));
		    if (ret == NULL) {
			xmlGenericError(xmlGenericErrorContext,
				"xmlSaveUri: out of memory\n");
			return(NULL);
		    }
		}
		if ((IS_UNRESERVED(*(p))) ||
                    ((*(p) == '$')) || ((*(p) == ',')) || ((*(p) == ';')) ||
                    ((*(p) == ':')) || ((*(p) == '@')) || ((*(p) == '&')) ||
                    ((*(p) == '=')) || ((*(p) == '+')))
		    ret[len++] = *p++;
		else {
		    int val = *(unsigned char *)p++;
		    int hi = val / 0x10, lo = val % 0x10;
		    ret[len++] = '%';
		    ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		    ret[len++] = lo + (lo > 9? 'A'-10 : '0');
		}
	    }
	} else if (uri->scheme != NULL) {
	    if (len + 3 >= max) {
		max *= 2;
		ret = (xmlChar *) xmlRealloc(ret,
			(max + 1) * sizeof(xmlChar));
		if (ret == NULL) {
		    xmlGenericError(xmlGenericErrorContext,
			    "xmlSaveUri: out of memory\n");
		    return(NULL);
		}
	    }
	    ret[len++] = '/';
	    ret[len++] = '/';
	}
	if (uri->path != NULL) {
	    p = uri->path;
	    /*
	     * the colon in file:///d: should not be escaped or
	     * Windows accesses fail later.
	     */
	    if ((uri->scheme != NULL) &&
		(p[0] == '/') &&
		(((p[1] >= 'a') && (p[1] <= 'z')) ||
		 ((p[1] >= 'A') && (p[1] <= 'Z'))) &&
		(p[2] == ':') &&
	        (xmlStrEqual(uri->scheme, BAD_CAST "file"))) {
		if (len + 3 >= max) {
		    max *= 2;
		    ret = (xmlChar *) xmlRealloc(ret,
			    (max + 1) * sizeof(xmlChar));
		    if (ret == NULL) {
			xmlGenericError(xmlGenericErrorContext,
				"xmlSaveUri: out of memory\n");
			return(NULL);
		    }
		}
		ret[len++] = *p++;
		ret[len++] = *p++;
		ret[len++] = *p++;
	    }
	    while (*p != 0) {
		if (len + 3 >= max) {
		    max *= 2;
		    ret = (xmlChar *) xmlRealloc(ret,
			    (max + 1) * sizeof(xmlChar));
		    if (ret == NULL) {
			xmlGenericError(xmlGenericErrorContext,
				"xmlSaveUri: out of memory\n");
			return(NULL);
		    }
		}
		if ((IS_UNRESERVED(*(p))) || ((*(p) == '/')) ||
                    ((*(p) == ';')) || ((*(p) == '@')) || ((*(p) == '&')) ||
	            ((*(p) == '=')) || ((*(p) == '+')) || ((*(p) == '$')) ||
	            ((*(p) == ',')))
		    ret[len++] = *p++;
		else {
		    int val = *(unsigned char *)p++;
		    int hi = val / 0x10, lo = val % 0x10;
		    ret[len++] = '%';
		    ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		    ret[len++] = lo + (lo > 9? 'A'-10 : '0');
		}
	    }
	}
	if (uri->query_raw != NULL) {
	    if (len + 1 >= max) {
		max *= 2;
		ret = (xmlChar *) xmlRealloc(ret,
			(max + 1) * sizeof(xmlChar));
		if (ret == NULL) {
		    xmlGenericError(xmlGenericErrorContext,
			    "xmlSaveUri: out of memory\n");
		    return(NULL);
		}
	    }
	    ret[len++] = '?';
	    p = uri->query_raw;
	    while (*p != 0) {
		if (len + 1 >= max) {
		    max *= 2;
		    ret = (xmlChar *) xmlRealloc(ret,
			    (max + 1) * sizeof(xmlChar));
		    if (ret == NULL) {
			xmlGenericError(xmlGenericErrorContext,
				"xmlSaveUri: out of memory\n");
			return(NULL);
		    }
		}
		ret[len++] = *p++;
	    }
	} else if (uri->query != NULL) {
	    if (len + 3 >= max) {
		max *= 2;
		ret = (xmlChar *) xmlRealloc(ret,
			(max + 1) * sizeof(xmlChar));
		if (ret == NULL) {
		    xmlGenericError(xmlGenericErrorContext,
			    "xmlSaveUri: out of memory\n");
		    return(NULL);
		}
	    }
	    ret[len++] = '?';
	    p = uri->query;
	    while (*p != 0) {
		if (len + 3 >= max) {
		    max *= 2;
		    ret = (xmlChar *) xmlRealloc(ret,
			    (max + 1) * sizeof(xmlChar));
		    if (ret == NULL) {
			xmlGenericError(xmlGenericErrorContext,
				"xmlSaveUri: out of memory\n");
			return(NULL);
		    }
		}
		if ((IS_UNRESERVED(*(p))) || (IS_RESERVED(*(p)))) 
		    ret[len++] = *p++;
		else {
		    int val = *(unsigned char *)p++;
		    int hi = val / 0x10, lo = val % 0x10;
		    ret[len++] = '%';
		    ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		    ret[len++] = lo + (lo > 9? 'A'-10 : '0');
		}
	    }
	}
    }
    if (uri->fragment != NULL) {
	if (len + 3 >= max) {
	    max *= 2;
	    ret = (xmlChar *) xmlRealloc(ret,
		    (max + 1) * sizeof(xmlChar));
	    if (ret == NULL) {
		xmlGenericError(xmlGenericErrorContext,
			"xmlSaveUri: out of memory\n");
		return(NULL);
	    }
	}
	ret[len++] = '#';
	p = uri->fragment;
	while (*p != 0) {
	    if (len + 3 >= max) {
		max *= 2;
		ret = (xmlChar *) xmlRealloc(ret,
			(max + 1) * sizeof(xmlChar));
		if (ret == NULL) {
		    xmlGenericError(xmlGenericErrorContext,
			    "xmlSaveUri: out of memory\n");
		    return(NULL);
		}
	    }
	    if ((IS_UNRESERVED(*(p))) || (IS_RESERVED(*(p)))) 
		ret[len++] = *p++;
	    else {
		int val = *(unsigned char *)p++;
		int hi = val / 0x10, lo = val % 0x10;
		ret[len++] = '%';
		ret[len++] = hi + (hi > 9? 'A'-10 : '0');
		ret[len++] = lo + (lo > 9? 'A'-10 : '0');
	    }
	}
    }
    if (len >= max) {
	max *= 2;
	ret = (xmlChar *) xmlRealloc(ret, (max + 1) * sizeof(xmlChar));
	if (ret == NULL) {
	    xmlGenericError(xmlGenericErrorContext,
		    "xmlSaveUri: out of memory\n");
	    return(NULL);
	}
    }
    ret[len++] = 0;
    return(ret);
}

/**
 * xmlPrintURI:
 * @stream:  a FILE* for the output
 * @uri:  pointer to an xmlURI
 *
 * Prints the URI in the stream @stream.
 */
void
xmlPrintURI(FILE *stream, xmlURIPtr uri) {
    xmlChar *out;

    out = xmlSaveUri(uri);
    if (out != NULL) {
	fprintf(stream, "%s", (char *) out);
	xmlFree(out);
    }
}

/**
 * xmlCleanURI:
 * @uri:  pointer to an xmlURI
 *
 * Make sure the xmlURI struct is free of content
 */
static void
xmlCleanURI(xmlURIPtr uri) {
    if (uri == NULL) return;

    if (uri->scheme != NULL) xmlFree(uri->scheme);
    uri->scheme = NULL;
    if (uri->server != NULL) xmlFree(uri->server);
    uri->server = NULL;
    if (uri->user != NULL) xmlFree(uri->user);
    uri->user = NULL;
    if (uri->path != NULL) xmlFree(uri->path);
    uri->path = NULL;
    if (uri->fragment != NULL) xmlFree(uri->fragment);
    uri->fragment = NULL;
    if (uri->opaque != NULL) xmlFree(uri->opaque);
    uri->opaque = NULL;
    if (uri->authority != NULL) xmlFree(uri->authority);
    uri->authority = NULL;
    if (uri->query != NULL) xmlFree(uri->query);
    uri->query = NULL;
    if (uri->query_raw != NULL) xmlFree(uri->query_raw);
    uri->query_raw = NULL;
}

/**
 * xmlFreeURI:
 * @uri:  pointer to an xmlURI
 *
 * Free up the xmlURI struct
 */
void
xmlFreeURI(xmlURIPtr uri) {
    if (uri == NULL) return;

    if (uri->scheme != NULL) xmlFree(uri->scheme);
    if (uri->server != NULL) xmlFree(uri->server);
    if (uri->user != NULL) xmlFree(uri->user);
    if (uri->path != NULL) xmlFree(uri->path);
    if (uri->fragment != NULL) xmlFree(uri->fragment);
    if (uri->opaque != NULL) xmlFree(uri->opaque);
    if (uri->authority != NULL) xmlFree(uri->authority);
    if (uri->query != NULL) xmlFree(uri->query);
    if (uri->query_raw != NULL) xmlFree(uri->query_raw);
    xmlFree(uri);
}

/************************************************************************
 *									*
 *			Helper functions				*
 *									*
 ************************************************************************/

/**
 * xmlNormalizeURIPath:
 * @path:  pointer to the path string
 *
 * Applies the 5 normalization steps to a path string--that is, RFC 2396
 * Section 5.2, steps 6.c through 6.g.
 *
 * Normalization occurs directly on the string, no new allocation is done
 *
 * Returns 0 or an error code
 */
int
xmlNormalizeURIPath(char *path) {
    char *cur, *out;

    if (path == NULL)
	return(-1);

    /* Skip all initial "/" chars.  We want to get to the beginning of the
     * first non-empty segment.
     */
    cur = path;
    while (cur[0] == '/')
      ++cur;
    if (cur[0] == '\0')
      return(0);

    /* Keep everything we've seen so far.  */
    out = cur;

    /*
     * Analyze each segment in sequence for cases (c) and (d).
     */
    while (cur[0] != '\0') {
	/*
	 * c) All occurrences of "./", where "." is a complete path segment,
	 *    are removed from the buffer string.
	 */
	if ((cur[0] == '.') && (cur[1] == '/')) {
	    cur += 2;
	    /* '//' normalization should be done at this point too */
	    while (cur[0] == '/')
		cur++;
	    continue;
	}

	/*
	 * d) If the buffer string ends with "." as a complete path segment,
	 *    that "." is removed.
	 */
	if ((cur[0] == '.') && (cur[1] == '\0'))
	    break;

	/* Otherwise keep the segment.  */
	while (cur[0] != '/') {
            if (cur[0] == '\0')
              goto done_cd;
	    (out++)[0] = (cur++)[0];
	}
	/* nomalize // */
	while ((cur[0] == '/') && (cur[1] == '/'))
	    cur++;

        (out++)[0] = (cur++)[0];
    }
 done_cd:
    out[0] = '\0';

    /* Reset to the beginning of the first segment for the next sequence.  */
    cur = path;
    while (cur[0] == '/')
      ++cur;
    if (cur[0] == '\0')
	return(0);

    /*
     * Analyze each segment in sequence for cases (e) and (f).
     *
     * e) All occurrences of "<segment>/../", where <segment> is a
     *    complete path segment not equal to "..", are removed from the
     *    buffer string.  Removal of these path segments is performed
     *    iteratively, removing the leftmost matching pattern on each
     *    iteration, until no matching pattern remains.
     *
     * f) If the buffer string ends with "<segment>/..", where <segment>
     *    is a complete path segment not equal to "..", that
     *    "<segment>/.." is removed.
     *
     * To satisfy the "iterative" clause in (e), we need to collapse the
     * string every time we find something that needs to be removed.  Thus,
     * we don't need to keep two pointers into the string: we only need a
     * "current position" pointer.
     */
    while (1) {
        char *segp, *tmp;

        /* At the beginning of each iteration of this loop, "cur" points to
         * the first character of the segment we want to examine.
         */

        /* Find the end of the current segment.  */
        segp = cur;
        while ((segp[0] != '/') && (segp[0] != '\0'))
          ++segp;

        /* If this is the last segment, we're done (we need at least two
         * segments to meet the criteria for the (e) and (f) cases).
         */
        if (segp[0] == '\0')
          break;

        /* If the first segment is "..", or if the next segment _isn't_ "..",
         * keep this segment and try the next one.
         */
        ++segp;
        if (((cur[0] == '.') && (cur[1] == '.') && (segp == cur+3))
            || ((segp[0] != '.') || (segp[1] != '.')
                || ((segp[2] != '/') && (segp[2] != '\0')))) {
          cur = segp;
          continue;
        }

        /* If we get here, remove this segment and the next one and back up
         * to the previous segment (if there is one), to implement the
         * "iteratively" clause.  It's pretty much impossible to back up
         * while maintaining two pointers into the buffer, so just compact
         * the whole buffer now.
         */

        /* If this is the end of the buffer, we're done.  */
        if (segp[2] == '\0') {
          cur[0] = '\0';
          break;
        }
        /* Valgrind complained, strcpy(cur, segp + 3); */
	/* string will overlap, do not use strcpy */
	tmp = cur;
	segp += 3;
	while ((*tmp++ = *segp++) != 0);

        /* If there are no previous segments, then keep going from here.  */
        segp = cur;
        while ((segp > path) && ((--segp)[0] == '/'))
          ;
        if (segp == path)
          continue;

        /* "segp" is pointing to the end of a previous segment; find it's
         * start.  We need to back up to the previous segment and start
         * over with that to handle things like "foo/bar/../..".  If we
         * don't do this, then on the first pass we'll remove the "bar/..",
         * but be pointing at the second ".." so we won't realize we can also
         * remove the "foo/..".
         */
        cur = segp;
        while ((cur > path) && (cur[-1] != '/'))
          --cur;
    }
    out[0] = '\0';

    /*
     * g) If the resulting buffer string still begins with one or more
     *    complete path segments of "..", then the reference is
     *    considered to be in error. Implementations may handle this
     *    error by retaining these components in the resolved path (i.e.,
     *    treating them as part of the final URI), by removing them from
     *    the resolved path (i.e., discarding relative levels above the
     *    root), or by avoiding traversal of the reference.
     *
     * We discard them from the final path.
     */
    if (path[0] == '/') {
      cur = path;
      while ((cur[0] == '/') && (cur[1] == '.') && (cur[2] == '.')
             && ((cur[3] == '/') || (cur[3] == '\0')))
	cur += 3;

      if (cur != path) {
	out = path;
	while (cur[0] != '\0')
          (out++)[0] = (cur++)[0];
	out[0] = 0;
      }
    }

    return(0);
}

static int is_hex(char c) {
    if (((c >= '0') && (c <= '9')) ||
        ((c >= 'a') && (c <= 'f')) ||
        ((c >= 'A') && (c <= 'F')))
	return(1);
    return(0);
}

/**
 * xmlURIUnescapeString:
 * @str:  the string to unescape
 * @len:   the length in bytes to unescape (or <= 0 to indicate full string)
 * @target:  optional destination buffer
 *
 * Unescaping routine, but does not check that the string is an URI. The
 * output is a direct unsigned char translation of %XX values (no encoding)
 * Note that the length of the result can only be smaller or same size as
 * the input string.
 *
 * Returns a copy of the string, but unescaped, will return NULL only in case
 * of error
 */
char *
xmlURIUnescapeString(const char *str, int len, char *target) {
    char *ret, *out;
    const char *in;

    if (str == NULL)
	return(NULL);
    if (len <= 0) len = strlen(str);
    if (len < 0) return(NULL);

    if (target == NULL) {
	ret = (char *) xmlMallocAtomic(len + 1);
	if (ret == NULL) {
	    xmlGenericError(xmlGenericErrorContext,
		    "xmlURIUnescapeString: out of memory\n");
	    return(NULL);
	}
    } else
	ret = target;
    in = str;
    out = ret;
    while(len > 0) {
	if ((len > 2) && (*in == '%') && (is_hex(in[1])) && (is_hex(in[2]))) {
	    in++;
	    if ((*in >= '0') && (*in <= '9')) 
	        *out = (*in - '0');
	    else if ((*in >= 'a') && (*in <= 'f'))
	        *out = (*in - 'a') + 10;
	    else if ((*in >= 'A') && (*in <= 'F'))
	        *out = (*in - 'A') + 10;
	    in++;
	    if ((*in >= '0') && (*in <= '9')) 
	        *out = *out * 16 + (*in - '0');
	    else if ((*in >= 'a') && (*in <= 'f'))
	        *out = *out * 16 + (*in - 'a') + 10;
	    else if ((*in >= 'A') && (*in <= 'F'))
	        *out = *out * 16 + (*in - 'A') + 10;
	    in++;
	    len -= 3;
	    out++;
	} else {
	    *out++ = *in++;
	    len--;
	}
    }
    *out = 0;
    return(ret);
}

/**
 * xmlURIEscapeStr:
 * @str:  string to escape
 * @list: exception list string of chars not to escape
 *
 * This routine escapes a string to hex, ignoring reserved characters (a-z)
 * and the characters in the exception list.
 *
 * Returns a new escaped string or NULL in case of error.
 */
xmlChar *
xmlURIEscapeStr(const xmlChar *str, const xmlChar *list) {
    xmlChar *ret, ch;
    const xmlChar *in;

    unsigned int len, out;

    if (str == NULL)
	return(NULL);
    if (str[0] == 0)
	return(xmlStrdup(str));
    len = xmlStrlen(str);
    if (!(len > 0)) return(NULL);

    len += 20;
    ret = (xmlChar *) xmlMallocAtomic(len);
    if (ret == NULL) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlURIEscapeStr: out of memory\n");
	return(NULL);
    }
    in = (const xmlChar *) str;
    out = 0;
    while(*in != 0) {
	if (len - out <= 3) {
	    len += 20;
	    ret = (xmlChar *) xmlRealloc(ret, len);
	    if (ret == NULL) {
		xmlGenericError(xmlGenericErrorContext,
			"xmlURIEscapeStr: out of memory\n");
		return(NULL);
	    }
	}

	ch = *in;

	if ((ch != '@') && (!IS_UNRESERVED(ch)) && (!xmlStrchr(list, ch))) {
	    unsigned char val;
	    ret[out++] = '%';
	    val = ch >> 4;
	    if (val <= 9)
		ret[out++] = '0' + val;
	    else
		ret[out++] = 'A' + val - 0xA;
	    val = ch & 0xF;
	    if (val <= 9)
		ret[out++] = '0' + val;
	    else
		ret[out++] = 'A' + val - 0xA;
	    in++;
	} else {
	    ret[out++] = *in++;
	}

    }
    ret[out] = 0;
    return(ret);
}

/**
 * xmlURIEscape:
 * @str:  the string of the URI to escape
 *
 * Escaping routine, does not do validity checks !
 * It will try to escape the chars needing this, but this is heuristic
 * based it's impossible to be sure.
 *
 * Returns an copy of the string, but escaped
 *
 * 25 May 2001
 * Uses xmlParseURI and xmlURIEscapeStr to try to escape correctly
 * according to RFC2396.
 *   - Carl Douglas
 */
xmlChar *
xmlURIEscape(const xmlChar * str)
{
    xmlChar *ret, *segment = NULL;
    xmlURIPtr uri;
    int ret2;

#define NULLCHK(p) if(!p) { \
                   xmlGenericError(xmlGenericErrorContext, \
                        "xmlURIEscape: out of memory\n"); \
                   return NULL; }

    if (str == NULL)
        return (NULL);

    uri = xmlCreateURI();
    if (uri != NULL) {
	/*
	 * Allow escaping errors in the unescaped form
	 */
        uri->cleanup = 1;
        ret2 = xmlParseURIReference(uri, (const char *)str);
        if (ret2) {
            xmlFreeURI(uri);
            return (NULL);
        }
    }

    if (!uri)
        return NULL;

    ret = NULL;

    if (uri->scheme) {
        segment = xmlURIEscapeStr(BAD_CAST uri->scheme, BAD_CAST "+-.");
        NULLCHK(segment)
        ret = xmlStrcat(ret, segment);
        ret = xmlStrcat(ret, BAD_CAST ":");
        xmlFree(segment);
    }

    if (uri->authority) {
        segment =
            xmlURIEscapeStr(BAD_CAST uri->authority, BAD_CAST "/?;:@");
        NULLCHK(segment)
        ret = xmlStrcat(ret, BAD_CAST "//");
        ret = xmlStrcat(ret, segment);
        xmlFree(segment);
    }

    if (uri->user) {
        segment = xmlURIEscapeStr(BAD_CAST uri->user, BAD_CAST ";:&=+$,");
        NULLCHK(segment)
		ret = xmlStrcat(ret,BAD_CAST "//");	
        ret = xmlStrcat(ret, segment);
        ret = xmlStrcat(ret, BAD_CAST "@");
        xmlFree(segment);
    }

    if (uri->server) {
        segment = xmlURIEscapeStr(BAD_CAST uri->server, BAD_CAST "/?;:@");
        NULLCHK(segment)
		if (uri->user == NULL)
       		ret = xmlStrcat(ret, BAD_CAST "//");
        ret = xmlStrcat(ret, segment);
        xmlFree(segment);
    }

    if (uri->port) {
        xmlChar port[10];

        snprintf((char *) port, 10, "%d", uri->port);
        ret = xmlStrcat(ret, BAD_CAST ":");
        ret = xmlStrcat(ret, port);
    }

    if (uri->path) {
        segment =
            xmlURIEscapeStr(BAD_CAST uri->path, BAD_CAST ":@&=+$,/?;");
        NULLCHK(segment)
        ret = xmlStrcat(ret, segment);
        xmlFree(segment);
    }

    if (uri->query_raw) {
        ret = xmlStrcat(ret, BAD_CAST "?");
        ret = xmlStrcat(ret, BAD_CAST uri->query_raw);
    }
    else if (uri->query) {
        segment =
            xmlURIEscapeStr(BAD_CAST uri->query, BAD_CAST ";/?:@&=+,$");
        NULLCHK(segment)
        ret = xmlStrcat(ret, BAD_CAST "?");
        ret = xmlStrcat(ret, segment);
        xmlFree(segment);
    }

    if (uri->opaque) {
        segment = xmlURIEscapeStr(BAD_CAST uri->opaque, BAD_CAST "");
        NULLCHK(segment)
        ret = xmlStrcat(ret, segment);
        xmlFree(segment);
    }

    if (uri->fragment) {
        segment = xmlURIEscapeStr(BAD_CAST uri->fragment, BAD_CAST "#");
        NULLCHK(segment)
        ret = xmlStrcat(ret, BAD_CAST "#");
        ret = xmlStrcat(ret, segment);
        xmlFree(segment);
    }

    xmlFreeURI(uri);
#undef NULLCHK

    return (ret);
}

/************************************************************************
 *									*
 *			Escaped URI parsing				*
 *									*
 ************************************************************************/

/**
 * xmlParseURIFragment:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse an URI fragment string and fills in the appropriate fields
 * of the @uri structure.
 * 
 * fragment = *uric
 *
 * Returns 0 or the error code
 */
static int
xmlParseURIFragment(xmlURIPtr uri, const char **str)
{
    const char *cur;
    
    if (str == NULL)
        return (-1);

    cur = *str;

    while (IS_URIC(cur) || IS_UNWISE(cur))
        NEXT(cur);
    if (uri != NULL) {
        if (uri->fragment != NULL)
            xmlFree(uri->fragment);
	if (uri->cleanup & 2)
	    uri->fragment = STRNDUP(*str, cur - *str);
	else
	    uri->fragment = xmlURIUnescapeString(*str, cur - *str, NULL);
    }
    *str = cur;
    return (0);
}

/**
 * xmlParseURIQuery:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse the query part of an URI
 * 
 * query = *uric
 *
 * Returns 0 or the error code
 */
static int
xmlParseURIQuery(xmlURIPtr uri, const char **str)
{
    const char *cur;

    if (str == NULL)
        return (-1);

    cur = *str;

    while ((IS_URIC(cur)) ||
           ((uri != NULL) && (uri->cleanup & 1) && (IS_UNWISE(cur))))
        NEXT(cur);
    if (uri != NULL) {
        if (uri->query != NULL)
            xmlFree(uri->query);
	if (uri->cleanup & 2)
	    uri->query = STRNDUP(*str, cur - *str);
	else
	    uri->query = xmlURIUnescapeString(*str, cur - *str, NULL);

	/* Save the raw bytes of the query as well.
	 * See: http://mail.gnome.org/archives/xml/2007-April/thread.html#00114
	 */
	if (uri->query_raw != NULL)
	    xmlFree (uri->query_raw);
	uri->query_raw = STRNDUP (*str, cur - *str);
    }
    *str = cur;
    return (0);
}

/**
 * xmlParseURIScheme:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse an URI scheme
 * 
 * scheme = alpha *( alpha | digit | "+" | "-" | "." )
 *
 * Returns 0 or the error code
 */
static int
xmlParseURIScheme(xmlURIPtr uri, const char **str) {
    const char *cur;

    if (str == NULL)
	return(-1);
    
    cur = *str;
    if (!IS_ALPHA(*cur))
	return(2);
    cur++;
    while (IS_SCHEME(*cur)) cur++;
    if (uri != NULL) {
	if (uri->scheme != NULL) xmlFree(uri->scheme);
	uri->scheme = STRNDUP(*str, cur - *str);
    }
    *str = cur;
    return(0);
}

/**
 * xmlParseURIOpaquePart:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse an URI opaque part
 * 
 * opaque_part = uric_no_slash *uric
 *
 * Returns 0 or the error code
 */
static int
xmlParseURIOpaquePart(xmlURIPtr uri, const char **str)
{
    const char *cur;

    if (str == NULL)
        return (-1);

    cur = *str;
    if (!((IS_URIC_NO_SLASH(cur)) ||
          ((uri != NULL) && (uri->cleanup & 1) && (IS_UNWISE(cur))))) {
        return (3);
    }
    NEXT(cur);
    while ((IS_URIC(cur)) ||
           ((uri != NULL) && (uri->cleanup & 1) && (IS_UNWISE(cur))))
        NEXT(cur);
    if (uri != NULL) {
        if (uri->opaque != NULL)
            xmlFree(uri->opaque);
	if (uri->cleanup & 2)
	    uri->opaque = STRNDUP(*str, cur - *str);
	else
	    uri->opaque = xmlURIUnescapeString(*str, cur - *str, NULL);
    }
    *str = cur;
    return (0);
}

/**
 * xmlParseURIServer:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse a server subpart of an URI, it's a finer grain analysis
 * of the authority part.
 * 
 * server        = [ [ userinfo "@" ] hostport ]
 * userinfo      = *( unreserved | escaped |
 *                       ";" | ":" | "&" | "=" | "+" | "$" | "," )
 * hostport      = host [ ":" port ]
 * host          = hostname | IPv4address | IPv6reference
 * hostname      = *( domainlabel "." ) toplabel [ "." ]
 * domainlabel   = alphanum | alphanum *( alphanum | "-" ) alphanum
 * toplabel      = alpha | alpha *( alphanum | "-" ) alphanum
 * IPv6reference = "[" IPv6address "]"
 * IPv6address   = hexpart [ ":" IPv4address ]
 * IPv4address   = 1*3digit "." 1*3digit "." 1*3digit "." 1*3digit
 * hexpart       = hexseq | hexseq "::" [ hexseq ]| "::" [ hexseq ]
 * hexseq        = hex4 *( ":" hex4)
 * hex4          = 1*4hexdig
 * port          = *digit
 *
 * Returns 0 or the error code
 */
static int
xmlParseURIServer(xmlURIPtr uri, const char **str) {
    const char *cur;
    const char *host, *tmp;
    const int IPV4max = 4;
    const int IPV6max = 8;
    int oct;

    if (str == NULL)
	return(-1);
    
    cur = *str;

    /*
     * is there a userinfo ?
     */
    while (IS_USERINFO(cur)) NEXT(cur);
    if (*cur == '@') {
	if (uri != NULL) {
	    if (uri->user != NULL) xmlFree(uri->user);
	    if (uri->cleanup & 2)
		uri->user = STRNDUP(*str, cur - *str);
	    else
		uri->user = xmlURIUnescapeString(*str, cur - *str, NULL);
	}
	cur++;
    } else {
	if (uri != NULL) {
	    if (uri->user != NULL) xmlFree(uri->user);
	    uri->user = NULL;
	}
        cur = *str;
    }
    /*
     * This can be empty in the case where there is no server
     */
    host = cur;
    if (*cur == '/') {
	if (uri != NULL) {
	    if (uri->authority != NULL) xmlFree(uri->authority);
	    uri->authority = NULL;
	    if (uri->server != NULL) xmlFree(uri->server);
	    uri->server = NULL;
	    uri->port = 0;
	}
	return(0);
    }
    /*
     * host part of hostport can denote an IPV4 address, an IPV6 address
     * or an unresolved name. Check the IP first, its easier to detect
     * errors if wrong one.
     * An IPV6 address must start with a '[' and end with a ']'.
     */
    if (*cur == '[') {
	int compress=0;
	cur++;
	for (oct = 0; oct < IPV6max; ++oct) {
	    if (*cur == ':') {
		if (compress)
		    return(3);	/* multiple compression attempted */
		if (!oct) { 	/* initial char is compression */
		    if (*++cur != ':')
			return(3);
		}
		compress = 1;	/* set compression-encountered flag */
		cur++;		/* skip over the second ':' */
		continue;
	    }
	    while(IS_HEX(*cur)) cur++;
	    if (oct == (IPV6max-1))
		continue;
	    if (*cur != ':')
		break;
	    cur++;
	}
	if ((!compress) && (oct != IPV6max))
	    return(3);
	if (*cur != ']')
	    return(3);
	if (uri != NULL) {
	    if (uri->server != NULL) xmlFree(uri->server);
	    uri->server = (char *)xmlStrndup((xmlChar *)host+1,
			(cur-host)-1);
	}
	cur++;
    } else {
	/*
	 * Not IPV6, maybe IPV4
	 */
	for (oct = 0; oct < IPV4max; ++oct) {
            if (*cur == '.') 
                return(3); /* e.g. http://.xml/ or http://18.29..30/ */
            while(IS_DIGIT(*cur)) cur++;
            if (oct == (IPV4max-1))
                continue;
            if (*cur != '.')
	        break;
            cur++;
	}
    }
    if ((host[0] != '[') && (oct < IPV4max || (*cur == '.' && cur++) ||
			     IS_ALPHA(*cur))) {
        /* maybe host_name */
        if (!IS_ALPHANUM(*cur))
            return(4); /* e.g. http://xml.$oft */
        do {
            do ++cur; while (IS_ALPHANUM(*cur));
            if (*cur == '-') {
	        --cur;
                if (*cur == '.')
                    return(5); /* e.g. http://xml.-soft */
	        ++cur;
		continue;
            }
    	    if (*cur == '.') {
	        --cur;
                if (*cur == '-')
                    return(6); /* e.g. http://xml-.soft */
                if (*cur == '.')
                    return(7); /* e.g. http://xml..soft */
	        ++cur;
		continue;
            }
	    break;
        } while (1);
        tmp = cur;
        if (tmp[-1] == '.')
            --tmp; /* e.g. http://xml.$Oft/ */
        do --tmp; while (tmp >= host && IS_ALPHANUM(*tmp));
        if ((++tmp == host || tmp[-1] == '.') && !IS_ALPHA(*tmp))
            return(8); /* e.g. http://xmlsOft.0rg/ */
    }
    if (uri != NULL) {
	if (uri->authority != NULL) xmlFree(uri->authority);
	uri->authority = NULL;
	if (host[0] != '[') {	/* it's not an IPV6 addr */
	    if (uri->server != NULL) xmlFree(uri->server);
	    if (uri->cleanup & 2)
		uri->server = STRNDUP(host, cur - host);
	    else
		uri->server = xmlURIUnescapeString(host, cur - host, NULL);
	}
    }
    /*
     * finish by checking for a port presence.
     */
    if (*cur == ':') {
        cur++;
	if (IS_DIGIT(*cur)) {
	    if (uri != NULL)
	        uri->port = 0;
	    while (IS_DIGIT(*cur)) {
	        if (uri != NULL)
		    uri->port = uri->port * 10 + (*cur - '0');
		cur++;
	    }
	}
    }
    *str = cur;
    return(0);
}	

/**
 * xmlParseURIRelSegment:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse an URI relative segment
 * 
 * rel_segment = 1*( unreserved | escaped | ";" | "@" | "&" | "=" |
 *                          "+" | "$" | "," )
 *
 * Returns 0 or the error code
 */
static int
xmlParseURIRelSegment(xmlURIPtr uri, const char **str)
{
    const char *cur;

    if (str == NULL)
        return (-1);

    cur = *str;
    if (!((IS_SEGMENT(cur)) ||
          ((uri != NULL) && (uri->cleanup & 1) && (IS_UNWISE(cur))))) {
        return (3);
    }
    NEXT(cur);
    while ((IS_SEGMENT(cur)) ||
           ((uri != NULL) && (uri->cleanup & 1) && (IS_UNWISE(cur))))
        NEXT(cur);
    if (uri != NULL) {
        if (uri->path != NULL)
            xmlFree(uri->path);
	if (uri->cleanup & 2)
	    uri->path = STRNDUP(*str, cur - *str);
	else
	    uri->path = xmlURIUnescapeString(*str, cur - *str, NULL);
    }
    *str = cur;
    return (0);
}

/**
 * xmlParseURIPathSegments:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 * @slash:  should we add a leading slash
 *
 * Parse an URI set of path segments
 * 
 * path_segments = segment *( "/" segment )
 * segment       = *pchar *( ";" param )
 * param         = *pchar
 *
 * Returns 0 or the error code
 */
static int
xmlParseURIPathSegments(xmlURIPtr uri, const char **str, int slash)
{
    const char *cur;

    if (str == NULL)
        return (-1);

    cur = *str;

    do {
        while ((IS_PCHAR(cur)) ||
	       ((uri != NULL) && (uri->cleanup & 1) && (IS_UNWISE(cur))))
            NEXT(cur);
        while (*cur == ';') {
            cur++;
            while ((IS_PCHAR(cur)) ||
	           ((uri != NULL) && (uri->cleanup & 1) && (IS_UNWISE(cur))))
                NEXT(cur);
        }
        if (*cur != '/')
            break;
        cur++;
    } while (1);
    if (uri != NULL) {
        int len, len2 = 0;
        char *path;

        /*
         * Concat the set of path segments to the current path
         */
        len = cur - *str;
        if (slash)
            len++;

        if (uri->path != NULL) {
            len2 = strlen(uri->path);
            len += len2;
        }
        path = (char *) xmlMallocAtomic(len + 1);
        if (path == NULL) {
	    xmlGenericError(xmlGenericErrorContext,
	    		    "xmlParseURIPathSegments: out of memory\n");
            *str = cur;
            return (-1);
        }
        if (uri->path != NULL)
            memcpy(path, uri->path, len2);
        if (slash) {
            path[len2] = '/';
            len2++;
        }
        path[len2] = 0;
        if (cur - *str > 0) {
	    if (uri->cleanup & 2) {
	        memcpy(&path[len2], *str, cur - *str);
		path[len2 + (cur - *str)] = 0;
	    } else
		xmlURIUnescapeString(*str, cur - *str, &path[len2]);
	}
        if (uri->path != NULL)
            xmlFree(uri->path);
        uri->path = path;
    }
    *str = cur;
    return (0);
}

/**
 * xmlParseURIAuthority:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse the authority part of an URI.
 * 
 * authority = server | reg_name
 * server    = [ [ userinfo "@" ] hostport ]
 * reg_name  = 1*( unreserved | escaped | "$" | "," | ";" | ":" |
 *                        "@" | "&" | "=" | "+" )
 *
 * Note : this is completely ambiguous since reg_name is allowed to
 *        use the full set of chars in use by server:
 *
 *        3.2.1. Registry-based Naming Authority
 *
 *        The structure of a registry-based naming authority is specific
 *        to the URI scheme, but constrained to the allowed characters
 *        for an authority component.
 *
 * Returns 0 or the error code
 */
static int
xmlParseURIAuthority(xmlURIPtr uri, const char **str) {
    const char *cur;
    int ret;

    if (str == NULL)
	return(-1);
    
    cur = *str;

    /*
     * try first to parse it as a server string.
     */
    ret = xmlParseURIServer(uri, str);
    if ((ret == 0) && (*str != NULL) &&
	((**str == 0) || (**str == '/') || (**str == '?')))
        return(0);
    *str = cur;

    /*
     * failed, fallback to reg_name
     */
    if (!IS_REG_NAME(cur)) {
	return(5);
    }
    NEXT(cur);
    while (IS_REG_NAME(cur)) NEXT(cur);
    if (uri != NULL) {
	if (uri->server != NULL) xmlFree(uri->server);
	uri->server = NULL;
	if (uri->user != NULL) xmlFree(uri->user);
	uri->user = NULL;
	if (uri->authority != NULL) xmlFree(uri->authority);
	if (uri->cleanup & 2)
	    uri->authority = STRNDUP(*str, cur - *str);
	else
	    uri->authority = xmlURIUnescapeString(*str, cur - *str, NULL);
    }
    *str = cur;
    return(0);
}

/**
 * xmlParseURIHierPart:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse an URI hierarchical part
 * 
 * hier_part = ( net_path | abs_path ) [ "?" query ]
 * abs_path = "/"  path_segments
 * net_path = "//" authority [ abs_path ]
 *
 * Returns 0 or the error code
 */
static int
xmlParseURIHierPart(xmlURIPtr uri, const char **str) {
    int ret;
    const char *cur;

    if (str == NULL)
	return(-1);
    
    cur = *str;

    if ((cur[0] == '/') && (cur[1] == '/')) {
	cur += 2;
	ret = xmlParseURIAuthority(uri, &cur);
	if (ret != 0)
	    return(ret);
	if (cur[0] == '/') {
	    cur++;
	    ret = xmlParseURIPathSegments(uri, &cur, 1);
	}
    } else if (cur[0] == '/') {
	cur++;
	ret = xmlParseURIPathSegments(uri, &cur, 1);
    } else {
	return(4);
    }
    if (ret != 0)
	return(ret);
    if (*cur == '?') {
	cur++;
	ret = xmlParseURIQuery(uri, &cur);
	if (ret != 0)
	    return(ret);
    }
    *str = cur;
    return(0);
}

/**
 * xmlParseAbsoluteURI:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse an URI reference string and fills in the appropriate fields
 * of the @uri structure
 * 
 * absoluteURI   = scheme ":" ( hier_part | opaque_part )
 *
 * Returns 0 or the error code
 */
static int
xmlParseAbsoluteURI(xmlURIPtr uri, const char **str) {
    int ret;
    const char *cur;

    if (str == NULL)
	return(-1);
    
    cur = *str;

    ret = xmlParseURIScheme(uri, str);
    if (ret != 0) return(ret);
    if (**str != ':') {
	*str = cur;
	return(1);
    }
    (*str)++;
    if (**str == '/')
	return(xmlParseURIHierPart(uri, str));
    return(xmlParseURIOpaquePart(uri, str));
}

/**
 * xmlParseRelativeURI:
 * @uri:  pointer to an URI structure
 * @str:  pointer to the string to analyze
 *
 * Parse an relative URI string and fills in the appropriate fields
 * of the @uri structure
 * 
 * relativeURI = ( net_path | abs_path | rel_path ) [ "?" query ]
 * abs_path = "/"  path_segments
 * net_path = "//" authority [ abs_path ]
 * rel_path = rel_segment [ abs_path ]
 *
 * Returns 0 or the error code
 */
static int
xmlParseRelativeURI(xmlURIPtr uri, const char **str) {
    int ret = 0;
    const char *cur;

    if (str == NULL)
	return(-1);
    
    cur = *str;
    if ((cur[0] == '/') && (cur[1] == '/')) {
	cur += 2;
	ret = xmlParseURIAuthority(uri, &cur);
	if (ret != 0)
	    return(ret);
	if (cur[0] == '/') {
	    cur++;
	    ret = xmlParseURIPathSegments(uri, &cur, 1);
	}
    } else if (cur[0] == '/') {
	cur++;
	ret = xmlParseURIPathSegments(uri, &cur, 1);
    } else if (cur[0] != '#' && cur[0] != '?') {
	ret = xmlParseURIRelSegment(uri, &cur);
	if (ret != 0)
	    return(ret);
	if (cur[0] == '/') {
	    cur++;
	    ret = xmlParseURIPathSegments(uri, &cur, 1);
	}
    }
    if (ret != 0)
	return(ret);
    if (*cur == '?') {
	cur++;
	ret = xmlParseURIQuery(uri, &cur);
	if (ret != 0)
	    return(ret);
    }
    *str = cur;
    return(ret);
}

/**
 * xmlParseURIReference:
 * @uri:  pointer to an URI structure
 * @str:  the string to analyze
 *
 * Parse an URI reference string and fills in the appropriate fields
 * of the @uri structure
 * 
 * URI-reference = [ absoluteURI | relativeURI ] [ "#" fragment ]
 *
 * Returns 0 or the error code
 */
int
xmlParseURIReference(xmlURIPtr uri, const char *str) {
    int ret;
    const char *tmp = str;

    if (str == NULL)
	return(-1);
    xmlCleanURI(uri);

    /*
     * Try first to parse absolute refs, then fallback to relative if
     * it fails.
     */
    ret = xmlParseAbsoluteURI(uri, &str);
    if (ret != 0) {
	xmlCleanURI(uri);
	str = tmp;
        ret = xmlParseRelativeURI(uri, &str);
    }
    if (ret != 0) {
	xmlCleanURI(uri);
	return(ret);
    }

    if (*str == '#') {
	str++;
	ret = xmlParseURIFragment(uri, &str);
	if (ret != 0) return(ret);
    }
    if (*str != 0) {
	xmlCleanURI(uri);
	return(1);
    }
    return(0);
}

/**
 * xmlParseURI:
 * @str:  the URI string to analyze
 *
 * Parse an URI 
 * 
 * URI-reference = [ absoluteURI | relativeURI ] [ "#" fragment ]
 *
 * Returns a newly built xmlURIPtr or NULL in case of error
 */
xmlURIPtr
xmlParseURI(const char *str) {
    xmlURIPtr uri;
    int ret;

    if (str == NULL)
	return(NULL);
    uri = xmlCreateURI();
    if (uri != NULL) {
	ret = xmlParseURIReference(uri, str);
        if (ret) {
	    xmlFreeURI(uri);
	    return(NULL);
	}
    }
    return(uri);
}

/**
 * xmlParseURIRaw:
 * @str:  the URI string to analyze
 * @raw:  if 1 unescaping of URI pieces are disabled
 *
 * Parse an URI but allows to keep intact the original fragments.
 * 
 * URI-reference = [ absoluteURI | relativeURI ] [ "#" fragment ]
 *
 * Returns a newly built xmlURIPtr or NULL in case of error
 */
xmlURIPtr
xmlParseURIRaw(const char *str, int raw) {
    xmlURIPtr uri;
    int ret;

    if (str == NULL)
	return(NULL);
    uri = xmlCreateURI();
    if (uri != NULL) {
        if (raw) {
	    uri->cleanup |= 2;
	}
	ret = xmlParseURIReference(uri, str);
        if (ret) {
	    xmlFreeURI(uri);
	    return(NULL);
	}
    }
    return(uri);
}

/************************************************************************
 *									*
 *			Public functions				*
 *									*
 ************************************************************************/

/**
 * xmlBuildURI:
 * @URI:  the URI instance found in the document
 * @base:  the base value
 *
 * Computes he final URI of the reference done by checking that
 * the given URI is valid, and building the final URI using the
 * base URI. This is processed according to section 5.2 of the 
 * RFC 2396
 *
 * 5.2. Resolving Relative References to Absolute Form
 *
 * Returns a new URI string (to be freed by the caller) or NULL in case
 *         of error.
 */
xmlChar *
xmlBuildURI(const xmlChar *URI, const xmlChar *base) {
    xmlChar *val = NULL;
    int ret, len, indx, cur, out;
    xmlURIPtr ref = NULL;
    xmlURIPtr bas = NULL;
    xmlURIPtr res = NULL;

    /*
     * 1) The URI reference is parsed into the potential four components and
     *    fragment identifier, as described in Section 4.3.
     *
     *    NOTE that a completely empty URI is treated by modern browsers
     *    as a reference to "." rather than as a synonym for the current
     *    URI.  Should we do that here?
     */
    if (URI == NULL) 
	ret = -1;
    else {
	if (*URI) {
	    ref = xmlCreateURI();
	    if (ref == NULL)
		goto done;
	    ret = xmlParseURIReference(ref, (const char *) URI);
	}
	else
	    ret = 0;
    }
    if (ret != 0)
	goto done;
    if ((ref != NULL) && (ref->scheme != NULL)) {
	/*
	 * The URI is absolute don't modify.
	 */
	val = xmlStrdup(URI);
	goto done;
    }
    if (base == NULL)
	ret = -1;
    else {
	bas = xmlCreateURI();
	if (bas == NULL)
	    goto done;
	ret = xmlParseURIReference(bas, (const char *) base);
    }
    if (ret != 0) {
	if (ref)
	    val = xmlSaveUri(ref);
	goto done;
    }
    if (ref == NULL) {
	/*
	 * the base fragment must be ignored
	 */
	if (bas->fragment != NULL) {
	    xmlFree(bas->fragment);
	    bas->fragment = NULL;
	}
	val = xmlSaveUri(bas);
	goto done;
    }

    /*
     * 2) If the path component is empty and the scheme, authority, and
     *    query components are undefined, then it is a reference to the
     *    current document and we are done.  Otherwise, the reference URI's
     *    query and fragment components are defined as found (or not found)
     *    within the URI reference and not inherited from the base URI.
     *
     *    NOTE that in modern browsers, the parsing differs from the above
     *    in the following aspect:  the query component is allowed to be
     *    defined while still treating this as a reference to the current
     *    document.
     */
    res = xmlCreateURI();
    if (res == NULL)
	goto done;
    if ((ref->scheme == NULL) && (ref->path == NULL) &&
	((ref->authority == NULL) && (ref->server == NULL))) {
	if (bas->scheme != NULL)
	    res->scheme = xmlMemStrdup(bas->scheme);
	if (bas->authority != NULL)
	    res->authority = xmlMemStrdup(bas->authority);
	else if (bas->server != NULL) {
	    res->server = xmlMemStrdup(bas->server);
	    if (bas->user != NULL)
		res->user = xmlMemStrdup(bas->user);
	    res->port = bas->port;		
	}
	if (bas->path != NULL)
	    res->path = xmlMemStrdup(bas->path);
	if (ref->query_raw != NULL)
	    res->query_raw = xmlMemStrdup (ref->query_raw);
	else if (ref->query != NULL)
	    res->query = xmlMemStrdup(ref->query);
	else if (bas->query_raw != NULL)
	    res->query_raw = xmlMemStrdup(bas->query_raw);
	else if (bas->query != NULL)
	    res->query = xmlMemStrdup(bas->query);
	if (ref->fragment != NULL)
	    res->fragment = xmlMemStrdup(ref->fragment);
	goto step_7;
    }

    /*
     * 3) If the scheme component is defined, indicating that the reference
     *    starts with a scheme name, then the reference is interpreted as an
     *    absolute URI and we are done.  Otherwise, the reference URI's
     *    scheme is inherited from the base URI's scheme component.
     */
    if (ref->scheme != NULL) {
	val = xmlSaveUri(ref);
	goto done;
    }
    if (bas->scheme != NULL)
	res->scheme = xmlMemStrdup(bas->scheme);
 
    if (ref->query_raw != NULL)
	res->query_raw = xmlMemStrdup(ref->query_raw);
    else if (ref->query != NULL)
	res->query = xmlMemStrdup(ref->query);
    if (ref->fragment != NULL)
	res->fragment = xmlMemStrdup(ref->fragment);

    /*
     * 4) If the authority component is defined, then the reference is a
     *    network-path and we skip to step 7.  Otherwise, the reference
     *    URI's authority is inherited from the base URI's authority
     *    component, which will also be undefined if the URI scheme does not
     *    use an authority component.
     */
    if ((ref->authority != NULL) || (ref->server != NULL)) {
	if (ref->authority != NULL)
	    res->authority = xmlMemStrdup(ref->authority);
	else {
	    res->server = xmlMemStrdup(ref->server);
	    if (ref->user != NULL)
		res->user = xmlMemStrdup(ref->user);
            res->port = ref->port;		
	}
	if (ref->path != NULL)
	    res->path = xmlMemStrdup(ref->path);
	goto step_7;
    }
    if (bas->authority != NULL)
	res->authority = xmlMemStrdup(bas->authority);
    else if (bas->server != NULL) {
	res->server = xmlMemStrdup(bas->server);
	if (bas->user != NULL)
	    res->user = xmlMemStrdup(bas->user);
	res->port = bas->port;		
    }

    /*
     * 5) If the path component begins with a slash character ("/"), then
     *    the reference is an absolute-path and we skip to step 7.
     */
    if ((ref->path != NULL) && (ref->path[0] == '/')) {
	res->path = xmlMemStrdup(ref->path);
	goto step_7;
    }


    /*
     * 6) If this step is reached, then we are resolving a relative-path
     *    reference.  The relative path needs to be merged with the base
     *    URI's path.  Although there are many ways to do this, we will
     *    describe a simple method using a separate string buffer.
     *
     * Allocate a buffer large enough for the result string.
     */
    len = 2; /* extra / and 0 */
    if (ref->path != NULL)
	len += strlen(ref->path);
    if (bas->path != NULL)
	len += strlen(bas->path);
    res->path = (char *) xmlMallocAtomic(len);
    if (res->path == NULL) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlBuildURI: out of memory\n");
	goto done;
    }
    res->path[0] = 0;

    /*
     * a) All but the last segment of the base URI's path component is
     *    copied to the buffer.  In other words, any characters after the
     *    last (right-most) slash character, if any, are excluded.
     */
    cur = 0;
    out = 0;
    if (bas->path != NULL) {
	while (bas->path[cur] != 0) {
	    while ((bas->path[cur] != 0) && (bas->path[cur] != '/'))
		cur++;
	    if (bas->path[cur] == 0)
		break;

	    cur++;
	    while (out < cur) {
		res->path[out] = bas->path[out];
		out++;
	    }
	}
    }
    res->path[out] = 0;

    /*
     * b) The reference's path component is appended to the buffer
     *    string.
     */
    if (ref->path != NULL && ref->path[0] != 0) {
	indx = 0;
	/*
	 * Ensure the path includes a '/'
	 */
	if ((out == 0) && (bas->server != NULL))
	    res->path[out++] = '/';
	while (ref->path[indx] != 0) {
	    res->path[out++] = ref->path[indx++];
	}
    }
    res->path[out] = 0;

    /*
     * Steps c) to h) are really path normalization steps
     */
    xmlNormalizeURIPath(res->path);

step_7:

    /*
     * 7) The resulting URI components, including any inherited from the
     *    base URI, are recombined to give the absolute form of the URI
     *    reference.
     */
    val = xmlSaveUri(res);

done:
    if (ref != NULL)
	xmlFreeURI(ref);
    if (bas != NULL)
	xmlFreeURI(bas);
    if (res != NULL)
	xmlFreeURI(res);
    return(val);
}

/**
 * xmlBuildRelativeURI:
 * @URI:  the URI reference under consideration
 * @base:  the base value
 *
 * Expresses the URI of the reference in terms relative to the
 * base.  Some examples of this operation include:
 *     base = "http://site1.com/docs/book1.html"
 *        URI input                        URI returned
 *     docs/pic1.gif                    pic1.gif
 *     docs/img/pic1.gif                img/pic1.gif
 *     img/pic1.gif                     ../img/pic1.gif
 *     http://site1.com/docs/pic1.gif   pic1.gif
 *     http://site2.com/docs/pic1.gif   http://site2.com/docs/pic1.gif
 *
 *     base = "docs/book1.html"
 *        URI input                        URI returned
 *     docs/pic1.gif                    pic1.gif
 *     docs/img/pic1.gif                img/pic1.gif
 *     img/pic1.gif                     ../img/pic1.gif
 *     http://site1.com/docs/pic1.gif   http://site1.com/docs/pic1.gif
 *
 *
 * Note: if the URI reference is really wierd or complicated, it may be
 *       worthwhile to first convert it into a "nice" one by calling
 *       xmlBuildURI (using 'base') before calling this routine,
 *       since this routine (for reasonable efficiency) assumes URI has
 *       already been through some validation.
 *
 * Returns a new URI string (to be freed by the caller) or NULL in case
 * error.
 */
xmlChar *
xmlBuildRelativeURI (const xmlChar * URI, const xmlChar * base)
{
    xmlChar *val = NULL;
    int ret;
    int ix;
    int pos = 0;
    int nbslash = 0;
    int len;
    xmlURIPtr ref = NULL;
    xmlURIPtr bas = NULL;
    xmlChar *bptr, *uptr, *vptr;
    int remove_path = 0;

    if ((URI == NULL) || (*URI == 0))
	return NULL;

    /*
     * First parse URI into a standard form
     */
    ref = xmlCreateURI ();
    if (ref == NULL)
	return NULL;
    /* If URI not already in "relative" form */
    if (URI[0] != '.') {
	ret = xmlParseURIReference (ref, (const char *) URI);
	if (ret != 0)
	    goto done;		/* Error in URI, return NULL */
    } else
	ref->path = (char *)xmlStrdup(URI);

    /*
     * Next parse base into the same standard form
     */
    if ((base == NULL) || (*base == 0)) {
	val = xmlStrdup (URI);
	goto done;
    }
    bas = xmlCreateURI ();
    if (bas == NULL)
	goto done;
    if (base[0] != '.') {
	ret = xmlParseURIReference (bas, (const char *) base);
	if (ret != 0)
	    goto done;		/* Error in base, return NULL */
    } else
	bas->path = (char *)xmlStrdup(base);

    /*
     * If the scheme / server on the URI differs from the base,
     * just return the URI
     */
    if ((ref->scheme != NULL) &&
	((bas->scheme == NULL) ||
	 (xmlStrcmp ((xmlChar *)bas->scheme, (xmlChar *)ref->scheme)) ||
	 (xmlStrcmp ((xmlChar *)bas->server, (xmlChar *)ref->server)))) {
	val = xmlStrdup (URI);
	goto done;
    }
    if (xmlStrEqual((xmlChar *)bas->path, (xmlChar *)ref->path)) {
	val = xmlStrdup(BAD_CAST "");
	goto done;
    }
    if (bas->path == NULL) {
	val = xmlStrdup((xmlChar *)ref->path);
	goto done;
    }
    if (ref->path == NULL) {
        ref->path = (char *) "/";
	remove_path = 1;
    }

    /*
     * At this point (at last!) we can compare the two paths
     *
     * First we take care of the special case where either of the
     * two path components may be missing (bug 316224)
     */
    if (bas->path == NULL) {
	if (ref->path != NULL) {
	    uptr = (xmlChar *) ref->path;
	    if (*uptr == '/')
		uptr++;
	    /* exception characters from xmlSaveUri */
	    val = xmlURIEscapeStr(uptr, BAD_CAST "/;&=+$,");
	}
	goto done;
    }
    bptr = (xmlChar *)bas->path;
    if (ref->path == NULL) {
	for (ix = 0; bptr[ix] != 0; ix++) {
	    if (bptr[ix] == '/')
		nbslash++;
	}
	uptr = NULL;
	len = 1;	/* this is for a string terminator only */
    } else {
    /*
     * Next we compare the two strings and find where they first differ
     */
	if ((ref->path[pos] == '.') && (ref->path[pos+1] == '/'))
            pos += 2;
	if ((*bptr == '.') && (bptr[1] == '/'))
            bptr += 2;
	else if ((*bptr == '/') && (ref->path[pos] != '/'))
	    bptr++;
	while ((bptr[pos] == ref->path[pos]) && (bptr[pos] != 0))
	    pos++;

	if (bptr[pos] == ref->path[pos]) {
	    val = xmlStrdup(BAD_CAST "");
	    goto done;		/* (I can't imagine why anyone would do this) */
	}

	/*
	 * In URI, "back up" to the last '/' encountered.  This will be the
	 * beginning of the "unique" suffix of URI
	 */
	ix = pos;
	if ((ref->path[ix] == '/') && (ix > 0))
	    ix--;
	else if ((ref->path[ix] == 0) && (ix > 1) && (ref->path[ix - 1] == '/'))
	    ix -= 2;
	for (; ix > 0; ix--) {
	    if (ref->path[ix] == '/')
		break;
	}
	if (ix == 0) {
	    uptr = (xmlChar *)ref->path;
	} else {
	    ix++;
	    uptr = (xmlChar *)&ref->path[ix];
	}

	/*
	 * In base, count the number of '/' from the differing point
	 */
	if (bptr[pos] != ref->path[pos]) {/* check for trivial URI == base */
	    for (; bptr[ix] != 0; ix++) {
		if (bptr[ix] == '/')
		    nbslash++;
	    }
	}
	len = xmlStrlen (uptr) + 1;
    }
    
    if (nbslash == 0) {
	if (uptr != NULL)
	    /* exception characters from xmlSaveUri */
	    val = xmlURIEscapeStr(uptr, BAD_CAST "/;&=+$,");
	goto done;
    }

    /*
     * Allocate just enough space for the returned string -
     * length of the remainder of the URI, plus enough space
     * for the "../" groups, plus one for the terminator
     */
    val = (xmlChar *) xmlMalloc (len + 3 * nbslash);
    if (val == NULL) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlBuildRelativeURI: out of memory\n");
	goto done;
    }
    vptr = val;
    /*
     * Put in as many "../" as needed
     */
    for (; nbslash>0; nbslash--) {
	*vptr++ = '.';
	*vptr++ = '.';
	*vptr++ = '/';
    }
    /*
     * Finish up with the end of the URI
     */
    if (uptr != NULL) {
        if ((vptr > val) && (len > 0) &&
	    (uptr[0] == '/') && (vptr[-1] == '/')) {
	    memcpy (vptr, uptr + 1, len - 1);
	    vptr[len - 2] = 0;
	} else {
	    memcpy (vptr, uptr, len);
	    vptr[len - 1] = 0;
	}
    } else {
	vptr[len - 1] = 0;
    }

    /* escape the freshly-built path */
    vptr = val;
	/* exception characters from xmlSaveUri */
    val = xmlURIEscapeStr(vptr, BAD_CAST "/;&=+$,");
    xmlFree(vptr);

done:
    /*
     * Free the working variables
     */
    if (remove_path != 0)
        ref->path = NULL;
    if (ref != NULL)
	xmlFreeURI (ref);
    if (bas != NULL)
	xmlFreeURI (bas);

    return val;
}

/**
 * xmlCanonicPath:
 * @path:  the resource locator in a filesystem notation
 *
 * Constructs a canonic path from the specified path. 
 *
 * Returns a new canonic path, or a duplicate of the path parameter if the 
 * construction fails. The caller is responsible for freeing the memory occupied
 * by the returned string. If there is insufficient memory available, or the 
 * argument is NULL, the function returns NULL.
 */
#define IS_WINDOWS_PATH(p) 					\
	((p != NULL) &&						\
	 (((p[0] >= 'a') && (p[0] <= 'z')) ||			\
	  ((p[0] >= 'A') && (p[0] <= 'Z'))) &&			\
	 (p[1] == ':') && ((p[2] == '/') || (p[2] == '\\')))
xmlChar *
xmlCanonicPath(const xmlChar *path)
{
/*
 * For Windows implementations, additional work needs to be done to
 * replace backslashes in pathnames with "forward slashes"
 */
#if defined(_WIN32) && !defined(__CYGWIN__)    
    int len = 0;
    int i = 0;
    xmlChar *p = NULL;
#endif
    xmlURIPtr uri;
    xmlChar *ret;
    const xmlChar *absuri;

    if (path == NULL)
	return(NULL);

    /* sanitize filename starting with // so it can be used as URI */
    if ((path[0] == '/') && (path[1] == '/') && (path[2] != '/'))
        path++;

    if ((uri = xmlParseURI((const char *) path)) != NULL) {
	xmlFreeURI(uri);
	return xmlStrdup(path);
    }

    /* Check if this is an "absolute uri" */
    absuri = xmlStrstr(path, BAD_CAST "://");
    if (absuri != NULL) {
        int l, j;
	unsigned char c;
	xmlChar *escURI;

        /*
	 * this looks like an URI where some parts have not been
	 * escaped leading to a parsing problem.  Check that the first
	 * part matches a protocol.
	 */
	l = absuri - path;
	/* Bypass if first part (part before the '://') is > 20 chars */
	if ((l <= 0) || (l > 20))
	    goto path_processing;
	/* Bypass if any non-alpha characters are present in first part */
	for (j = 0;j < l;j++) {
	    c = path[j];
	    if (!(((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))))
	        goto path_processing;
	}

	/* Escape all except the characters specified in the supplied path */
        escURI = xmlURIEscapeStr(path, BAD_CAST ":/?_.#&;=");
	if (escURI != NULL) {
	    /* Try parsing the escaped path */
	    uri = xmlParseURI((const char *) escURI);
	    /* If successful, return the escaped string */
	    if (uri != NULL) {
	        xmlFreeURI(uri);
		return escURI;
	    }
	}
    }

path_processing:
/* For Windows implementations, replace backslashes with 'forward slashes' */
#if defined(_WIN32) && !defined(__CYGWIN__)    
    /*
     * Create a URI structure
     */
    uri = xmlCreateURI();
    if (uri == NULL) {		/* Guard against 'out of memory' */
        return(NULL);
    }

    len = xmlStrlen(path);
    if ((len > 2) && IS_WINDOWS_PATH(path)) {
        /* make the scheme 'file' */
	uri->scheme = xmlStrdup(BAD_CAST "file");
	/* allocate space for leading '/' + path + string terminator */
	uri->path = xmlMallocAtomic(len + 2);
	if (uri->path == NULL) {
	    xmlFreeURI(uri);	/* Guard agains 'out of memory' */
	    return(NULL);
	}
	/* Put in leading '/' plus path */
	uri->path[0] = '/';
	p = uri->path + 1;
	strncpy(p, path, len + 1);
    } else {
	uri->path = xmlStrdup(path);
	if (uri->path == NULL) {
	    xmlFreeURI(uri);
	    return(NULL);
	}
	p = uri->path;
    }
    /* Now change all occurences of '\' to '/' */
    while (*p != '\0') {
	if (*p == '\\')
	    *p = '/';
	p++;
    }

    if (uri->scheme == NULL) {
	ret = xmlStrdup((const xmlChar *) uri->path);
    } else {
	ret = xmlSaveUri(uri);
    }

    xmlFreeURI(uri);
#else
    ret = xmlStrdup((const xmlChar *) path);
#endif
    return(ret);
}

/**
 * xmlPathToURI:
 * @path:  the resource locator in a filesystem notation
 *
 * Constructs an URI expressing the existing path
 *
 * Returns a new URI, or a duplicate of the path parameter if the 
 * construction fails. The caller is responsible for freeing the memory
 * occupied by the returned string. If there is insufficient memory available,
 * or the argument is NULL, the function returns NULL.
 */
xmlChar *
xmlPathToURI(const xmlChar *path)
{
    xmlURIPtr uri;
    xmlURI temp;
    xmlChar *ret, *cal;

    if (path == NULL)
        return(NULL);

    if ((uri = xmlParseURI((const char *) path)) != NULL) {
	xmlFreeURI(uri);
	return xmlStrdup(path);
    }
    cal = xmlCanonicPath(path);
    if (cal == NULL)
        return(NULL);
#if defined(_WIN32) && !defined(__CYGWIN__)
    /* xmlCanonicPath can return an URI on Windows (is that the intended behaviour?) 
       If 'cal' is a valid URI allready then we are done here, as continuing would make
       it invalid. */
    if ((uri = xmlParseURI((const char *) cal)) != NULL) {
	xmlFreeURI(uri);
	return cal;
    }
    /* 'cal' can contain a relative path with backslashes. If that is processed
       by xmlSaveURI, they will be escaped and the external entity loader machinery
       will fail. So convert them to slashes. Misuse 'ret' for walking. */
    ret = cal;
    while (*ret != '\0') {
	if (*ret == '\\')
	    *ret = '/';
	ret++;
    }
#endif
    memset(&temp, 0, sizeof(temp));
    temp.path = (char *) cal;
    ret = xmlSaveUri(&temp);
    xmlFree(cal);
    return(ret);
}
#define bottom_uri
#include "elfgcchack.h"
