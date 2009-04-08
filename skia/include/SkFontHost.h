/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SkFontHost_DEFINED
#define SkFontHost_DEFINED

#include "SkScalerContext.h"
#include "SkTypeface.h"

class SkDescriptor;
class SkStream;
class SkWStream;

typedef uint32_t SkFontTableTag;
typedef uint32_t SkFontID;

/** \class SkFontHost

    This class is ported to each environment. It is responsible for bridging
    the gap between the (sort of) abstract class SkTypeface and the
    platform-specific implementation that provides access to font files.
 
    One basic task is for each create (subclass of) SkTypeface, the FontHost is
    resonsible for assigning a uniqueID. The ID should be unique for the
    underlying font file/data, not unique per typeface instance. Thus it is
    possible/common to request a typeface for the same font more than once
    (e.g. asking for the same font by name several times). The FontHost may
    return seperate typeface instances in that case, or it may choose to use a
    cache and return the same instance (but calling typeface->ref(), since the
    caller is always responsible for calling unref() on each instance that is
    returned). Either way, the fontID for those instance(s) will be the same.
    In addition, the fontID should never be set to 0. That value is used as a
    sentinel to indicate no-font-id.
 
    The major aspects are:
    1) Given either a name/style, return a subclass of SkTypeface that
        references the closest matching font available on the host system.
    2) Given the data for a font (either in a stream or a file name), return
        a typeface that allows access to that data.
    3) Each typeface instance carries a 32bit ID for its corresponding font.
        SkFontHost turns that ID into a stream to access the font's data.
    4) Given a font ID, return a subclass of SkScalerContext, which connects a
        font scaler (e.g. freetype or other) to the font's data.
    5) Utilites to manage the font cache (budgeting) and gamma correction
*/
class SkFontHost {
public:
    /** Return the closest matching typeface given either an existing family
        (specified by a typeface in that family) or by a familyName, and a
        requested style.
        1) If familyFace is null, use famillyName.
        2) If famillyName is null, use familyFace.
        3) If both are null, return the default font that best matches style
    */
    static SkTypeface* FindTypeface(const SkTypeface* familyFace,
                                    const char famillyName[],
                                    SkTypeface::Style style);

    /** Return the typeface associated with the uniqueID, or null if that ID
        does not match any faces.
    */
    static SkTypeface* ResolveTypeface(SkFontID uniqueID);
    
    /** Return a new stream to read the font data, or null if the uniqueID does
        not match an existing typeface
    */
    static SkStream* OpenStream(SkFontID uniqueID);
    static void CloseStream(SkFontID uniqueID, SkStream*);

    /** Return a new typeface given the data buffer (owned by the caller).
        If the data does not represent a valid font, return null.
    */
    static SkTypeface* CreateTypeface(SkStream*);

    ///////////////////////////////////////////////////////////////////////////

    /** Return the number of tables in the font
     */
    static int CountTables(SkFontID);

    /** Copy into tags[] (allocated by the caller) the list of table tags in
        the font, and return the number. This will be the same as CountTables()
        or 0 if an error occured.
     */
    static int GetTableTags(SkFontID, SkFontTableTag[]);

    /** Given a table tag, return the size of its contents, or 0 if not present
     */
    static size_t GetTableSize(SkFontID, SkFontTableTag);

    /** Copy the contents of a table into data (allocated by the caller). Note
        that the contents of the table will be in their native endian order
        (which for most truetype tables is big endian). If the table tag is
        not found, or there is an error copying the data, then 0 is returned.
        If this happens, it is possible that some or all of the memory pointed
        to by data may have been written to, even though an error has occured.

        @param fontID the font to copy the table from
        @param tag  The table tag whose contents are to be copied
        @param offset The offset in bytes into the table's contents where the
                copy should start from.
        @param length The number of bytes, starting at offset, of table data
                to copy.
        @param data storage address where the table contents are copied to
        @return the number of bytes actually copied into data. If offset+length
                exceeds the table's size, then only the bytes up to the table's
                size are actually copied, and this is the value returned. If
                offset > the table's size, or tag is not a valid table,
                then 0 is returned.
     */
    static size_t GetTableData(SkFontID fontID, SkFontTableTag tag,
                               size_t offset, size_t length, void* data);

    ///////////////////////////////////////////////////////////////////////////

    /** Write a unique identifier to the stream, so that the same typeface can
        be retrieved with Deserialize().
    */
    static void Serialize(const SkTypeface*, SkWStream*);
    static SkTypeface* Deserialize(SkStream*);

    ///////////////////////////////////////////////////////////////////////////
    
    /** Return a subclass of SkScalarContext
    */
    static SkScalerContext* CreateScalerContext(const SkDescriptor* desc);
    /** Return a scalercontext using the "fallback" font. If there is no designated
        fallback, return null.
    */
    static SkScalerContext* CreateFallbackScalerContext(const SkScalerContext::Rec&);

    /** Return the number of bytes (approx) that should be purged from the font
        cache. The input parameter is the cache's estimate of how much as been
        allocated by the cache so far.
        To purge (basically) everything, return the input parameter.
        To purge nothing, return 0
    */
    static size_t ShouldPurgeFontCache(size_t sizeAllocatedSoFar);

    /** Return SkScalerContext gamma flag, or 0, based on the paint that will be
        used to draw something with antialiasing.
    */
    static int ComputeGammaFlag(const SkPaint& paint);

    /** Return NULL or a pointer to 256 bytes for the black (table[0]) and
        white (table[1]) gamma tables.
    */
    static void GetGammaTables(const uint8_t* tables[2]);
};

#endif

