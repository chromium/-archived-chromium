/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderThemeWin_h
#define RenderThemeWin_h

#include "RenderTheme.h"

#if WIN32
typedef void* HANDLE;
typedef struct HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;
#endif

namespace WebCore {

struct ThemeData {
    ThemeData() : m_part(0), m_state(0), m_classicState(0) {}

    unsigned m_part;
    unsigned m_state;
    unsigned m_classicState;
};

class RenderThemeWin : public RenderTheme {
public:
    RenderThemeWin() { }
    ~RenderThemeWin() { }

    // A method asking if the theme's controls actually care about redrawing when hovered.
    virtual bool supportsHover(const RenderStyle*) const { return true; }

    // A method asking if the theme is able to draw the focus ring.
    virtual bool supportsFocusRing(const RenderStyle*) const;

    // The platform selection color.
    virtual Color platformActiveSelectionBackgroundColor() const;
    virtual Color platformInactiveSelectionBackgroundColor() const;
    virtual Color platformActiveSelectionForegroundColor() const;
    virtual Color platformInactiveSelectionForegroundColor() const;

    virtual double caretBlinkFrequency() const;

    // System fonts.
    virtual void systemFont(int propId, Document*, FontDescription&) const;

    virtual int minimumMenuListSize(RenderStyle*) const;

    virtual bool paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    { return paintButton(o, i, r); }
    virtual void setCheckboxSize(RenderStyle*) const;

    virtual bool paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    { return paintButton(o, i, r); }
    virtual void setRadioSize(RenderStyle* style) const;

    virtual bool paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    virtual bool paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
    { return paintTextField(o, i, r); }

    virtual bool paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    // MenuList refers to an unstyled menulist (meaning a menulist without
    // background-color or border set) and MenuListButton refers to a styled
    // menulist (a menulist with background-color or border set). They have
    // this distinction to support showing aqua style themes whenever they
    // possibly can, which is something we don't want to replicate.
    //
    // In short, we either go down the MenuList code path or the MenuListButton
    // codepath. We never go down both. And in both cases, they render the
    // entire menulist.
    virtual void adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;
    virtual bool paintMenuList(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);
    virtual void adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const;
    virtual bool paintMenuListButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&);

    // These methods define the padding for the MenuList's inner block.
    virtual int popupInternalPaddingLeft(RenderStyle*) const;
    virtual int popupInternalPaddingRight(RenderStyle*) const;
    virtual int popupInternalPaddingTop(RenderStyle*) const;
    virtual int popupInternalPaddingBottom(RenderStyle*) const;

    virtual void adjustButtonInnerStyle(RenderStyle* style) const;

    virtual void adjustSliderThumbSize(RenderObject*) const;

    virtual void adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual void adjustSearchFieldCancelButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual void adjustSearchFieldDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual void adjustSearchFieldResultsDecorationStyle(CSSStyleSelector*, RenderStyle*, Element*) const;
    virtual void adjustSearchFieldResultsButtonStyle(CSSStyleSelector*, RenderStyle*, Element*) const;

    // Provide a way to pass the default font size from the Settings object to
    // the render theme.  TODO(tc): http://b/1129186 A cleaner way would be to
    // remove the default font size from this object and have callers that need
    // the value to get it directly from the appropriate Settings object.
    static void setDefaultFontSize(int);

private:
    unsigned determineState(RenderObject*);
    unsigned determineClassicState(RenderObject*);

    ThemeData getThemeData(RenderObject*);

    bool paintTextFieldInternal(RenderObject*, const RenderObject::PaintInfo&, const IntRect&, bool);

    // Gets the minimal x button padding according to the current theme.
    void getMinimalButtonPadding(Length* minXPadding) const;

    int menuListInternalPadding(RenderStyle* style, int paddingType) const;
};

}

#endif
