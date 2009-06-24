/* libs/graphics/ports/SkFontHost_fontconfig_impl.h
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

/* The SkFontHost_fontconfig code requires an implementation of an abstact
 * fontconfig interface. We do this because sometimes fontconfig is not
 * directly availible and this provides an ability to change the fontconfig
 * implementation at run-time.
 */

#ifndef FontConfigInterface_DEFINED
#define FontConfigInterface_DEFINED

#include <string>

class FontConfigInterface {
  public:
    virtual ~FontConfigInterface() { }

    /** Performs config match
     *
     *  @param result_family (output) on success, the resulting family name.
     *  @param result_fileid (output) on success, the resulting file id.
     *  @param fileid_valid if true, then |fileid| is valid
     *  @param fileid the fileid (as returned by this function) which we are
     *    trying to match.
     *  @param family (optional) the family of the font that we are trying to
     *    match.
     *  @param is_bold (optional, set to NULL to ignore, in/out)
     *  @param is_italic (optional, set to NULL to ignore, in/out)
     *  @return true iff successful.
     */
    virtual bool Match(
          std::string* result_family,
          unsigned* result_fileid,
          bool fileid_valid,
          unsigned fileid,
          const std::string& family,
          bool* is_bold,
          bool* is_italic) = 0;

    /** Open a font file given the fileid as returned by Match
     */
    virtual int Open(unsigned fileid) = 0;
};

#endif  // FontConfigInterface_DEFINED
