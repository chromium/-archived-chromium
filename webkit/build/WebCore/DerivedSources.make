VPATH = \
    $(WebCore) \
    $(WebCore)/dom \

.PHONY : all

all : \
    CSSGrammar.cpp \
    CSSPropertyNames.h \
    CSSValueKeywords.h \
    ColorData.c \
    DocTypeStrings.cpp \
    HTMLEntityNames.c \
    HTMLEntityCodes.c \
    SVGNames.cpp \
    HTMLNames.cpp \
    UserAgentStyleSheets.h \
    XLinkNames.cpp \
    XMLNames.cpp \
    XPathGrammar.cpp \
    tokenizer.cpp \
    JSNode.h \


# CSS property names and value keywords

CSSPropertyNames.h : css/CSSPropertyNames.in css/SVGCSSPropertyNames.in
	# if sort $< $(WebCore)/css/SVGCSSPropertyNames.in | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	cat $< $(WebCore)/css/SVGCSSPropertyNames.in > CSSPropertyNames.in
	perl "$(WebCore)/../../../webkit/pending/makeprop.pl"

CSSValueKeywords.h : css/CSSValueKeywords.in css/SVGCSSValueKeywords.in
	# Lower case all the values, as CSS values are case-insensitive
	perl -ne 'print lc' $(WebCore)/css/SVGCSSValueKeywords.in > SVGCSSValueKeywords.in
	# if sort $< SVGCSSValueKeywords.in | uniq -d | grep -E '^[^#]'; then echo 'Duplicate value!'; exit 1; fi
	cat $< SVGCSSValueKeywords.in > CSSValueKeywords.in
	perl "$(WebCore)/../../../webkit/pending/makevalues.pl"

# DOCTYPE strings

DocTypeStrings.cpp : html/DocTypeStrings.gperf
	gperf -CEot -L ANSI-C -k "*" -N findDoctypeEntry -F ,PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards $< > $@

# HTML entity names

HTMLEntityNames.c : html/HTMLEntityNames.gperf
	gperf -a -L ANSI-C -C -G -c -o -t -k '*' -N findEntity -D -s 2 $< > $@

HTMLEntityCodes.c : html/HTMLEntityNames.gperf
	perl $(WebCore)/../../../webkit/build/WebCore/generate_entitycodes.pl $< > $@

# color names

ColorData.c : platform/ColorData.gperf
	gperf -CDEot -L ANSI-C -k '*' -N findColor -D -s 2 $< > $@

# CSS tokenizer

tokenizer.cpp : css/tokenizer.flex css/maketokenizer
	flex -t $< | perl $(WebCore)/css/maketokenizer > $@

# CSS grammar
# NOTE: older versions of bison do not inject an inclusion guard, so we do it

CSSGrammar.cpp : css/CSSGrammar.y
	bison -d -p cssyy $< -o $@
	touch CSSGrammar.cpp.h
	touch CSSGrammar.hpp
	echo '#ifndef CSSGrammar_h' > CSSGrammar.h
	echo '#define CSSGrammar_h' >> CSSGrammar.h
	cat CSSGrammar.cpp.h CSSGrammar.hpp >> CSSGrammar.h
	echo '#endif' >> CSSGrammar.h
	rm -f CSSGrammar.cpp.h CSSGrammar.hpp

# XPath grammar
# NOTE: older versions of bison do not inject an inclusion guard, so we do it

XPathGrammar.cpp : xml/XPathGrammar.y $(PROJECT_FILE)
	bison -d -p xpathyy $< -o $@
	touch XPathGrammar.cpp.h
	touch XPathGrammar.hpp
	echo '#ifndef XPathGrammar_h' > XPathGrammar.h
	echo '#define XPathGrammar_h' >> XPathGrammar.h
	cat XPathGrammar.cpp.h XPathGrammar.hpp >> XPathGrammar.h
	echo '#endif' >> XPathGrammar.h
	rm -f XPathGrammar.cpp.h XPathGrammar.hpp

# user agent style sheets

USER_AGENT_STYLE_SHEETS = $(WebCore)/css/view-source.css $(WebCore)/css/svg.css 
UserAgentStyleSheets.h : css/make-css-file-arrays.pl $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/html4.css $(PORTROOT)/css/html4-overrides.css $(PORTROOT)/css/quirks-overrides.css
	cat $(WebCore)/css/html4.css $(PORTROOT)/css/html4-overrides.css > $(DerivedSourcesDir)/html4.css
	cat $(WebCore)/css/quirks.css $(PORTROOT)/css/quirks-overrides.css > $(DerivedSourcesDir)/quirks.css
	perl $< $@ UserAgentStyleSheetsData.cpp $(DerivedSourcesDir)/html4.css $(DerivedSourcesDir)/quirks.css $(USER_AGENT_STYLE_SHEETS)

# HTML tag and attribute names

HTMLNames.cpp : dom/make_names.pl html/HTMLTagNames.in html/HTMLAttributeNames.in
	perl $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in \
            --namespace HTML --namespacePrefix xhtml --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xhtml" --attrsNullNamespace --output .

XMLNames.cpp : dom/make_names.pl xml/xmlattrs.in
	perl $< --attrs $(WebCore)/xml/xmlattrs.in \
            --namespace XML --cppNamespace WebCore --namespaceURI "http://www.w3.org/XML/1998/namespace" --output .

ifeq ($(findstring ENABLE_SVG,$(FEATURE_DEFINES)), ENABLE_SVG)

ifeq ($(findstring ENABLE_SVG_USE,$(FEATURE_DEFINES)), ENABLE_SVG_USE)
  SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_USE=1
endif

ifeq ($(findstring ENABLE_SVG_FONTS,$(FEATURE_DEFINES)), ENABLE_SVG_FONTS)
  SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FONTS=1
endif

ifeq ($(findstring ENABLE_SVG_FILTERS,$(FEATURE_DEFINES)), ENABLE_SVG_FILTERS)
  SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FILTERS=1
endif

ifeq ($(findstring ENABLE_SVG_AS_IMAGE,$(FEATURE_DEFINES)), ENABLE_SVG_AS_IMAGE)
  SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_AS_IMAGE=1
endif

ifeq ($(findstring ENABLE_SVG_ANIMATION,$(FEATURE_DEFINES)), ENABLE_SVG_ANIMATION)
  SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_ANIMATION=1
  endif

ifeq ($(findstring ENABLE_SVG_FOREIGN_OBJECT,$(FEATURE_DEFINES)), ENABLE_SVG_FOREIGN_OBJECT)
  SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FOREIGN_OBJECT=1
endif

# SVG tag and attribute names (need to pass an extra flag if svg experimental features are enabled)
ifdef SVG_FLAGS
SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	        perl $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --extraDefines "$(SVG_FLAGS)" \
	            --namespace SVG --cppNamespace WebCore --namespaceURI "http://www.w3.org/2000/svg" --factory --attrsNullNamespace --output .
else
SVGElementFactory.cpp SVGNames.cpp : dom/make_names.pl svg/svgtags.in svg/svgattrs.in
	        perl $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in \
	            --namespace SVG --cppNamespace WebCore --namespaceURI "http://www.w3.org/2000/svg" --factory --attrsNullNamespace --output .

endif

XLinkNames.cpp : dom/make_names.pl svg/xlinkattrs.in
	perl $< --attrs $(WebCore)/svg/xlinkattrs.in \
            --namespace XLink --cppNamespace WebCore --namespaceURI "http://www.w3.org/1999/xlink" --output .

# SVG CSS property names and value keywords

else

SVGElementFactory.cpp :
	echo > SVGElementFactory.cpp

SVGNames.cpp :
	echo > SVGNames.cpp

XLinkNames.cpp :
	echo > XLinkNames.cpp

ksvgcssproperties.h :
	echo > ksvgcssproperties.h

ksvgcssvalues.h :
	echo > ksvgcssvalues.h

endif

# new-style JavaScript bindings
 
JS_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/CodeGeneratorJS.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/IDLStructure.pm \
    bindings/scripts/generate-bindings.pl \
#
 
JSNode.h : Node.idl $(JS_BINDINGS_SCRIPTS)
	perl -I $(WebCore)/bindings/scripts $(WebCore)/bindings/scripts/generate-bindings.pl --defines "$(FEATURE_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --include dom --include html --include css --include page --include xml --include svg --include bingings/js --outputdir . $<
