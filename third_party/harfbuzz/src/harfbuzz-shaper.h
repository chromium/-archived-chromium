/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This is part of HarfBuzz, an OpenType Layout engine library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#ifndef HARFBUZZ_SHAPER_H
#define HARFBUZZ_SHAPER_H

#include "harfbuzz-global.h"
#include "harfbuzz-gdef.h"
#include "harfbuzz-gpos.h"
#include "harfbuzz-gsub.h"
#include "harfbuzz-external.h"
#include "harfbuzz-stream-private.h"

HB_BEGIN_HEADER

typedef enum {
        HB_Script_Common,
        HB_Script_Greek,
        HB_Script_Cyrillic,
        HB_Script_Armenian,
        HB_Script_Hebrew,
        HB_Script_Arabic,
        HB_Script_Syriac,
        HB_Script_Thaana,
        HB_Script_Devanagari,
        HB_Script_Bengali,
        HB_Script_Gurmukhi,
        HB_Script_Gujarati,
        HB_Script_Oriya,
        HB_Script_Tamil,
        HB_Script_Telugu,
        HB_Script_Kannada,
        HB_Script_Malayalam,
        HB_Script_Sinhala,
        HB_Script_Thai,
        HB_Script_Lao,
        HB_Script_Tibetan,
        HB_Script_Myanmar,
        HB_Script_Georgian,
        HB_Script_Hangul,
        HB_Script_Ogham,
        HB_Script_Runic,
        HB_Script_Khmer,
        HB_Script_Inherited,
        HB_ScriptCount = HB_Script_Inherited
        /*
        HB_Script_Latin = Common,
        HB_Script_Ethiopic = Common,
        HB_Script_Cherokee = Common,
        HB_Script_CanadianAboriginal = Common,
        HB_Script_Mongolian = Common,
        HB_Script_Hiragana = Common,
        HB_Script_Katakana = Common,
        HB_Script_Bopomofo = Common,
        HB_Script_Han = Common,
        HB_Script_Yi = Common,
        HB_Script_OldItalic = Common,
        HB_Script_Gothic = Common,
        HB_Script_Deseret = Common,
        HB_Script_Tagalog = Common,
        HB_Script_Hanunoo = Common,
        HB_Script_Buhid = Common,
        HB_Script_Tagbanwa = Common,
        HB_Script_Limbu = Common,
        HB_Script_TaiLe = Common,
        HB_Script_LinearB = Common,
        HB_Script_Ugaritic = Common,
        HB_Script_Shavian = Common,
        HB_Script_Osmanya = Common,
        HB_Script_Cypriot = Common,
        HB_Script_Braille = Common,
        HB_Script_Buginese = Common,
        HB_Script_Coptic = Common,
        HB_Script_NewTaiLue = Common,
        HB_Script_Glagolitic = Common,
        HB_Script_Tifinagh = Common,
        HB_Script_SylotiNagri = Common,
        HB_Script_OldPersian = Common,
        HB_Script_Kharoshthi = Common,
        HB_Script_Balinese = Common,
        HB_Script_Cuneiform = Common,
        HB_Script_Phoenician = Common,
        HB_Script_PhagsPa = Common,
        HB_Script_Nko = Common
        */
} HB_Script;

typedef struct
{
    hb_uint32 pos;
    hb_uint32 length;
    HB_Script script;
    hb_uint8 bidiLevel;
} HB_ScriptItem;

typedef enum {
    HB_NoBreak,
    HB_SoftHyphen,
    HB_Break,
    HB_ForcedBreak
} HB_LineBreakType;


typedef struct {
    /*HB_LineBreakType*/ unsigned lineBreakType  :2;
    /*HB_Bool*/ unsigned whiteSpace              :1;     /* A unicode whitespace character, except NBSP, ZWNBSP */
    /*HB_Bool*/ unsigned charStop                :1;     /* Valid cursor position (for left/right arrow) */
    /*HB_Bool*/ unsigned wordBoundary            :1;
    /*HB_Bool*/ unsigned sentenceBoundary        :1;
    unsigned unused                  :2;
} HB_CharAttributes;

void HB_GetCharAttributes(const HB_UChar16 *string, hb_uint32 stringLength,
                          const HB_ScriptItem *items, hb_uint32 numItems,
                          HB_CharAttributes *attributes);

/* requires HB_GetCharAttributes to be called before */
void HB_GetWordBoundaries(const HB_UChar16 *string, hb_uint32 stringLength,
                          const HB_ScriptItem *items, hb_uint32 numItems,
                          HB_CharAttributes *attributes);

/* requires HB_GetCharAttributes to be called before */
void HB_GetSentenceBoundaries(const HB_UChar16 *string, hb_uint32 stringLength,
                              const HB_ScriptItem *items, hb_uint32 numItems,
                              HB_CharAttributes *attributes);


typedef enum {
    HB_LeftToRight = 0,
    HB_RightToLeft = 1
} HB_StringToGlyphsFlags;

typedef enum {
    HB_ShaperFlag_Default = 0,
    HB_ShaperFlag_NoKerning = 1,
    HB_ShaperFlag_UseDesignMetrics = 2
} HB_ShaperFlag;

/* 
   highest value means highest priority for justification. Justification is done by first inserting kashidas
   starting with the highest priority positions, then stretching spaces, afterwards extending inter char
   spacing, and last spacing between arabic words.
   NoJustification is for example set for arabic where no Kashida can be inserted or for diacritics.
*/
typedef enum {
    HB_NoJustification= 0,   /* Justification can't be applied after this glyph */
    HB_Arabic_Space   = 1,   /* This glyph represents a space inside arabic text */
    HB_Character      = 2,   /* Inter-character justification point follows this glyph */
    HB_Space          = 4,   /* This glyph represents a blank outside an Arabic run */
    HB_Arabic_Normal  = 7,   /* Normal Middle-Of-Word glyph that connects to the right (begin) */
    HB_Arabic_Waw     = 8,   /* Next character is final form of Waw/Ain/Qaf/Fa */
    HB_Arabic_BaRa    = 9,   /* Next two chars are Ba + Ra/Ya/AlefMaksura */
    HB_Arabic_Alef    = 10,  /* Next character is final form of Alef/Tah/Lam/Kaf/Gaf */
    HB_Arabic_HaaDal  = 11,  /* Next character is final form of Haa/Dal/Taa Marbutah */
    HB_Arabic_Seen    = 12,  /* Initial or Medial form Of Seen/Sad */
    HB_Arabic_Kashida = 13   /* Kashida(U+640) in middle of word */
} HB_JustificationClass;

/* This structure is binary compatible with Uniscribe's SCRIPT_VISATTR. Would be nice to keep
 * it like that. If this is a problem please tell Trolltech :)
 */
typedef struct {
    unsigned justification   :4;  /* Justification class */
    unsigned clusterStart    :1;  /* First glyph of representation of cluster */
    unsigned mark            :1;  /* needs to be positioned around base char */
    unsigned zeroWidth       :1;  /* ZWJ, ZWNJ etc, with no width */
    unsigned dontPrint       :1;
    unsigned combiningClass  :8;
} HB_GlyphAttributes;

typedef struct HB_FaceRec_ {
    HB_Bool isSymbolFont;

    HB_GDEF gdef;
    HB_GSUB gsub;
    HB_GPOS gpos;
    HB_Bool supported_scripts[HB_ScriptCount];
    HB_Buffer buffer;
    HB_Script current_script;
    int current_flags; /* HB_ShaperFlags */
    HB_Bool has_opentype_kerning;
    HB_Bool glyphs_substituted;
    HB_GlyphAttributes *tmpAttributes;
    unsigned int *tmpLogClusters;
    int length;
    int orig_nglyphs;
} HB_FaceRec;

typedef HB_Error (*HB_GetFontTableFunc)(void *font, HB_Tag tag, HB_Byte *buffer, HB_UInt *length);

HB_Face HB_NewFace(void *font, HB_GetFontTableFunc tableFunc);
void HB_FreeFace(HB_Face face);

typedef struct {
    HB_Fixed x, y;
    HB_Fixed width, height;
    HB_Fixed xOffset, yOffset;
} HB_GlyphMetrics;

typedef enum {
    HB_FontAscent
} HB_FontMetric;

typedef struct {
    HB_Bool  (*convertStringToGlyphIndices)(HB_Font font, const HB_UChar16 *string, hb_uint32 length, HB_Glyph *glyphs, hb_uint32 *numGlyphs, HB_Bool rightToLeft);
    void     (*getGlyphAdvances)(HB_Font font, const HB_Glyph *glyphs, hb_uint32 numGlyphs, HB_Fixed *advances, int flags /*HB_ShaperFlag*/);
    HB_Bool  (*canRender)(HB_Font font, const HB_UChar16 *string, hb_uint32 length);
    /* implementation needs to make sure to load a scaled glyph, so /no/ FT_LOAD_NO_SCALE */
    HB_Error (*getPointInOutline)(HB_Font font, HB_Glyph glyph, int flags /*HB_ShaperFlag*/, hb_uint32 point, HB_Fixed *xpos, HB_Fixed *ypos, hb_uint32 *nPoints);
    void     (*getGlyphMetrics)(HB_Font font, HB_Glyph glyph, HB_GlyphMetrics *metrics);
    HB_Fixed (*getFontMetric)(HB_Font font, HB_FontMetric metric);
} HB_FontClass;

typedef struct HB_Font_ {
    const HB_FontClass *klass;

    /* Metrics */
    HB_UShort x_ppem, y_ppem;
    HB_16Dot16 x_scale, y_scale;

    void *userData;
} HB_FontRec;

typedef struct HB_ShaperItem_ HB_ShaperItem;

struct HB_ShaperItem_ {
    const HB_UChar16 *string;
    hb_uint32 stringLength;
    HB_ScriptItem item;
    HB_Font font;
    HB_Face face;
    int shaperFlags; /* HB_ShaperFlags */

    HB_Bool glyphIndicesPresent; /* set to true if the glyph indicies are already setup in the glyphs array */
    hb_uint32 initialGlyphCount;

    hb_uint32 num_glyphs; /* in: available glyphs out: glyphs used/needed */
    HB_Glyph *glyphs; /* out parameter */
    HB_GlyphAttributes *attributes; /* out */
    HB_Fixed *advances; /* out */
    HB_FixedPoint *offsets; /* out */
    unsigned short *log_clusters; /* out */

    /* internal */
    HB_Bool kerning_applied; /* out: kerning applied by shaper */
};

HB_Bool HB_ShapeItem(HB_ShaperItem *item);

HB_END_HEADER

#endif
