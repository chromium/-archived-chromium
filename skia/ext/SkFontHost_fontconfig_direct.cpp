/* libs/graphics/ports/SkFontHost_fontconfig_direct.cpp
**
** Copyright 2009, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "SkFontHost_fontconfig_direct.h"

#include <unistd.h>
#include <fcntl.h>

#include <fontconfig/fontconfig.h>

FontConfigDirect::FontConfigDirect()
    : next_file_id_(0) {
  FcInit();
}

// -----------------------------------------------------------------------------
// Normally we only return exactly the font asked for. In last-resort cases,
// the request is for one of the basic font names "Sans", "Serif" or
// "Monospace". This function tells you whether a given request is for such a
// fallback.
// -----------------------------------------------------------------------------
static bool IsFallbackFontAllowed(const std::string& family)
{
    const char* family_cstr = family.c_str();
    return strcasecmp(family_cstr, "sans") == 0 ||
           strcasecmp(family_cstr, "serif") == 0 ||
           strcasecmp(family_cstr, "monospace") == 0;
}

bool FontConfigDirect::Match(std::string* result_family,
                             unsigned* result_fileid,
                             bool fileid_valid, unsigned fileid,
                             const std::string& family, bool* is_bold,
                             bool* is_italic) {
    SkAutoMutexAcquire ac(mutex_);
    FcPattern* pattern = FcPatternCreate();

    FcValue fcvalue;
    if (fileid_valid) {
        const std::map<unsigned, std::string>::const_iterator
            i = fileid_to_filename_.find(fileid);
        if (i == fileid_to_filename_.end()) {
            FcPatternDestroy(pattern);
            return false;
        }

        fcvalue.type = FcTypeString;
        fcvalue.u.s = (FcChar8*) i->second.c_str();
        FcPatternAdd(pattern, FC_FILE, fcvalue, 0);
    }
    if (!family.empty()) {
        fcvalue.type = FcTypeString;
        fcvalue.u.s = (FcChar8*) family.c_str();
        FcPatternAdd(pattern, FC_FAMILY, fcvalue, 0);
    }

    fcvalue.type = FcTypeInteger;
    fcvalue.u.i = is_bold && *is_bold ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL;
    FcPatternAdd(pattern, FC_WEIGHT, fcvalue, 0);

    fcvalue.type = FcTypeInteger;
    fcvalue.u.i = is_italic && *is_italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
    FcPatternAdd(pattern, FC_SLANT, fcvalue, 0);

    fcvalue.type = FcTypeBool;
    fcvalue.u.b = FcTrue;
    FcPatternAdd(pattern, FC_SCALABLE, fcvalue, 0);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    // Font matching:
    // CSS often specifies a fallback list of families:
    //    font-family: a, b, c, serif;
    // However, fontconfig will always do its best to find *a* font when asked
    // for something so we need a way to tell if the match which it has found is
    // "good enough" for us. Otherwise, we can return NULL which gets piped up
    // and lets WebKit know to try the next CSS family name. However, fontconfig
    // configs allow substitutions (mapping "Arial -> Helvetica" etc) and we
    // wish to support that.
    //
    // Thus, if a specific family is requested we set @family_requested. Then we
    // record two strings: the family name after config processing and the
    // family name after resolving. If the two are equal, it's a good match.
    //
    // So consider the case where a user has mapped Arial to Helvetica in their
    // config.
    //    requested family: "Arial"
    //    post_config_family: "Helvetica"
    //    post_match_family: "Helvetica"
    //      -> good match
    //
    // and for a missing font:
    //    requested family: "Monaco"
    //    post_config_family: "Monaco"
    //    post_match_family: "Times New Roman"
    //      -> BAD match
    //
    // However, we special-case fallback fonts; see IsFallbackFontAllowed().
    FcChar8* post_config_family;
    FcPatternGetString(pattern, FC_FAMILY, 0, &post_config_family);

    FcResult result;
    FcPattern* match = FcFontMatch(0, pattern, &result);
    if (!match) {
        FcPatternDestroy(pattern);
        return false;
    }

    FcChar8* post_match_family;
    FcPatternGetString(match, FC_FAMILY, 0, &post_match_family);
    const bool family_names_match =
        family.empty() ?
        true :
        strcasecmp((char *)post_config_family, (char *)post_match_family) == 0;

    FcPatternDestroy(pattern);

    if (!family_names_match && !IsFallbackFontAllowed(family)) {
        FcPatternDestroy(match);
        return false;
    }

    FcChar8* c_filename;
    if (FcPatternGetString(match, FC_FILE, 0, &c_filename) != FcResultMatch) {
        FcPatternDestroy(match);
        return NULL;
    }
    const std::string filename((char *) c_filename);

    unsigned out_fileid;
    if (fileid_valid) {
        out_fileid = fileid;
    } else {
        const std::map<std::string, unsigned>::const_iterator
            i = filename_to_fileid_.find(filename);
        if (i == filename_to_fileid_.end()) {
            out_fileid = next_file_id_++;
            filename_to_fileid_[filename] = out_fileid;
            fileid_to_filename_[out_fileid] = filename;
        } else {
            out_fileid = i->second;
        }
    }

    if (result_fileid)
        *result_fileid = out_fileid;

    FcChar8* c_family;
    if (FcPatternGetString(match, FC_FAMILY, 0, &c_family)) {
        FcPatternDestroy(match);
        return NULL;
    }

    int resulting_bold;
    if (FcPatternGetInteger(match, FC_WEIGHT, 0, &resulting_bold))
      resulting_bold = FC_WEIGHT_NORMAL;

    int resulting_italic;
    if (FcPatternGetInteger(match, FC_SLANT, 0, &resulting_italic))
      resulting_italic = FC_SLANT_ROMAN;

    // If we ask for an italic font, fontconfig might take a roman font and set
    // the undocumented property FC_MATRIX to a skew matrix. It'll then say
    // that the font is italic or oblique. So, if we see a matrix, we don't
    // believe that it's italic.
    FcValue matrix;
    const bool have_matrix = FcPatternGet(match, FC_MATRIX, 0, &matrix) == 0;

    // If we ask for an italic font, fontconfig might take a roman font and set
    // FC_EMBOLDEN.
    FcValue embolden;
    const bool have_embolden =
        FcPatternGet(match, FC_EMBOLDEN, 0, &embolden) == 0;

    if (is_bold)
      *is_bold = resulting_bold >= FC_WEIGHT_BOLD && !have_embolden;
    if (is_italic)
      *is_italic = resulting_italic > FC_SLANT_ROMAN && !have_matrix;

    if (result_family)
        *result_family = (char *) c_family;

    FcPatternDestroy(match);

    return true;
}

int FontConfigDirect::Open(unsigned fileid) {
    SkAutoMutexAcquire ac(mutex_);
    const std::map<unsigned, std::string>::const_iterator
        i = fileid_to_filename_.find(fileid);
    if (i == fileid_to_filename_.end())
        return -1;

    return open(i->second.c_str(), O_RDONLY);
}
