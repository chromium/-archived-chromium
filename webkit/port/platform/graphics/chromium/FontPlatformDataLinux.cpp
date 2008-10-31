// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FontPlatformData.h"

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangoft2.h>

#include "CString.h"
#include "FontDescription.h"
#include "NotImplemented.h"
#include "PlatformString.h"

namespace WebCore {

PangoFontMap* FontPlatformData::m_fontMap = 0;
GHashTable* FontPlatformData::m_hashTable = 0;

FontPlatformData::FontPlatformData(const FontDescription& fontDescription, const AtomicString& familyName)
    : m_context(0)
    , m_font(0)
    , m_size(fontDescription.computedSize())
    , m_syntheticBold(false)
    , m_syntheticOblique(false)
{
    FontPlatformData::init();

    CString stored_family = familyName.string().utf8();
    char const* families[] = {
      stored_family.data(),
      NULL
    };

    switch (fontDescription.genericFamily()) {
    case FontDescription::SerifFamily:
        families[1] = "serif";
        break;
    case FontDescription::SansSerifFamily:
        families[1] = "sans";
        break;
    case FontDescription::MonospaceFamily:
        families[1] = "monospace";
        break;
    case FontDescription::NoFamily:
    case FontDescription::StandardFamily:
    default:
        families[1] = "sans";
        break;
    }

    PangoFontDescription* description = pango_font_description_new();
    pango_font_description_set_absolute_size(description, fontDescription.computedSize() * PANGO_SCALE);

    // FIXME: Map all FontWeight values to Pango font weights.
    if (fontDescription.weight() >= FontWeight600)
        pango_font_description_set_weight(description, PANGO_WEIGHT_BOLD);
    if (fontDescription.italic())
        pango_font_description_set_style(description, PANGO_STYLE_ITALIC);

    m_context = pango_ft2_font_map_create_context(PANGO_FT2_FONT_MAP(m_fontMap));

    for (unsigned int i = 0; !m_font && i < G_N_ELEMENTS(families); i++) {
        pango_font_description_set_family(description, families[i]);
        pango_context_set_font_description(m_context, description);
        m_font = pango_font_map_load_font(m_fontMap, m_context, description);
    }

    pango_font_description_free(description);
}

FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
    : m_context(0)
    , m_font(0)
    , m_size(size)
    , m_syntheticBold(bold)
    , m_syntheticOblique(oblique)
{
}

bool FontPlatformData::init()
{
    static bool initialized = false;
    if (initialized)
        return true;
    initialized = true;

    if (!m_fontMap)
        m_fontMap = pango_ft2_font_map_new();
    if (!m_hashTable) {
        PangoFontFamily** families = 0;
        int n_families = 0;

        m_hashTable = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);

        pango_font_map_list_families(m_fontMap, &families, &n_families);

        for (int family = 0; family < n_families; family++)
            g_hash_table_insert(m_hashTable,
                                g_strdup(pango_font_family_get_name(families[family])),
                                g_object_ref(families[family]));

        g_free(families);
    }

    return true;
}

bool FontPlatformData::isFixedPitch() const
{
    PangoFontDescription* description = pango_font_describe_with_absolute_size(m_font);
    PangoFontFamily* family = reinterpret_cast<PangoFontFamily*>(g_hash_table_lookup(m_hashTable, pango_font_description_get_family(description)));
    pango_font_description_free(description);
    return pango_font_family_is_monospace(family);
}

FontPlatformData::~FontPlatformData() {
}

bool FontPlatformData::operator==(const FontPlatformData& other) const
{
    if (m_font == other.m_font)
        return true;
    if (m_font == 0 || m_font == reinterpret_cast<PangoFont*>(-1)
        || other.m_font == 0 || other.m_font == reinterpret_cast<PangoFont*>(-1))
        return false;
    PangoFontDescription* thisDesc = pango_font_describe(m_font);
    PangoFontDescription* otherDesc = pango_font_describe(other.m_font);
    bool result = pango_font_description_equal(thisDesc, otherDesc);
    pango_font_description_free(otherDesc);
    pango_font_description_free(thisDesc);
    return result;
}

}  // namespace WebCore
