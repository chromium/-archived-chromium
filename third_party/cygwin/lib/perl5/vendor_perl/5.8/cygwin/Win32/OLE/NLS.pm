# The documentation is at the __END__

package Win32::OLE::NLS;
require Win32::OLE;  # Make sure the XS bootstrap has been called

use strict;
use vars qw(@EXPORT @EXPORT_OK %EXPORT_TAGS @ISA);

use Exporter;
@ISA = qw(Exporter);

@EXPORT = qw(
	     CompareString
	     LCMapString
	     GetLocaleInfo
	     GetStringType
	     GetSystemDefaultLangID
	     GetSystemDefaultLCID
	     GetUserDefaultLangID
	     GetUserDefaultLCID

	     MAKELANGID
	     PRIMARYLANGID
	     SUBLANGID
	     LANG_SYSTEM_DEFAULT
	     LANG_USER_DEFAULT
	     MAKELCID
	     LANGIDFROMLCID
	     LOCALE_SYSTEM_DEFAULT
	     LOCALE_USER_DEFAULT
	    );

@EXPORT_OK = qw(SetLocaleInfo SendSettingChange);

%EXPORT_TAGS =
(
 CT	 => [qw(CT_CTYPE1 CT_CTYPE2 CT_CTYPE3)],
 C1	 => [qw(C1_UPPER C1_LOWER C1_DIGIT C1_SPACE C1_PUNCT
		C1_CNTRL C1_BLANK C1_XDIGIT C1_ALPHA)],
 C2	 => [qw(C2_LEFTTORIGHT C2_RIGHTTOLEFT C2_EUROPENUMBER
		C2_EUROPESEPARATOR C2_EUROPETERMINATOR C2_ARABICNUMBER
		C2_COMMONSEPARATOR C2_BLOCKSEPARATOR C2_SEGMENTSEPARATOR
		C2_WHITESPACE C2_OTHERNEUTRAL C2_NOTAPPLICABLE)],
 C3	 => [qw(C3_NONSPACING C3_DIACRITIC C3_VOWELMARK C3_SYMBOL C3_KATAKANA
		C3_HIRAGANA C3_HALFWIDTH C3_FULLWIDTH C3_IDEOGRAPH C3_KASHIDA
		C3_ALPHA C3_NOTAPPLICABLE)],
 NORM	 => [qw(NORM_IGNORECASE NORM_IGNORENONSPACE NORM_IGNORESYMBOLS
		NORM_IGNOREWIDTH NORM_IGNOREKANATYPE NORM_IGNOREKASHIDA)],
 LCMAP	 => [qw(LCMAP_LOWERCASE LCMAP_UPPERCASE LCMAP_SORTKEY LCMAP_HALFWIDTH
		LCMAP_FULLWIDTH LCMAP_HIRAGANA LCMAP_KATAKANA)],
 LANG	 => [qw(LANG_NEUTRAL LANG_ALBANIAN LANG_ARABIC LANG_BAHASA
		LANG_BULGARIAN LANG_CATALAN LANG_CHINESE LANG_CZECH LANG_DANISH
		LANG_DUTCH LANG_ENGLISH LANG_FINNISH LANG_FRENCH LANG_GERMAN
		LANG_GREEK LANG_HEBREW LANG_HUNGARIAN LANG_ICELANDIC
		LANG_ITALIAN LANG_JAPANESE LANG_KOREAN LANG_NORWEGIAN
		LANG_POLISH LANG_PORTUGUESE LANG_RHAETO_ROMAN LANG_ROMANIAN
		LANG_RUSSIAN LANG_SERBO_CROATIAN LANG_SLOVAK LANG_SPANISH
		LANG_SWEDISH LANG_THAI LANG_TURKISH LANG_URDU)],
 SUBLANG => [qw(SUBLANG_NEUTRAL SUBLANG_DEFAULT SUBLANG_SYS_DEFAULT
		SUBLANG_CHINESE_SIMPLIFIED SUBLANG_CHINESE_TRADITIONAL
		SUBLANG_DUTCH SUBLANG_DUTCH_BELGIAN SUBLANG_ENGLISH_US
		SUBLANG_ENGLISH_UK SUBLANG_ENGLISH_AUS SUBLANG_ENGLISH_CAN
		SUBLANG_ENGLISH_NZ SUBLANG_ENGLISH_EIRE SUBLANG_FRENCH
		SUBLANG_FRENCH_BELGIAN SUBLANG_FRENCH_CANADIAN
		SUBLANG_FRENCH_SWISS SUBLANG_GERMAN SUBLANG_GERMAN_SWISS
		SUBLANG_GERMAN_AUSTRIAN SUBLANG_ITALIAN SUBLANG_ITALIAN_SWISS
		SUBLANG_NORWEGIAN_BOKMAL SUBLANG_NORWEGIAN_NYNORSK
		SUBLANG_PORTUGUESE SUBLANG_PORTUGUESE_BRAZILIAN
		SUBLANG_SERBO_CROATIAN_CYRILLIC SUBLANG_SERBO_CROATIAN_LATIN
		SUBLANG_SPANISH SUBLANG_SPANISH_MEXICAN
		SUBLANG_SPANISH_MODERN)],
 CTRY	 => [qw(CTRY_DEFAULT CTRY_AUSTRALIA CTRY_AUSTRIA CTRY_BELGIUM
		CTRY_BRAZIL CTRY_CANADA CTRY_DENMARK CTRY_FINLAND CTRY_FRANCE
		CTRY_GERMANY CTRY_ICELAND CTRY_IRELAND CTRY_ITALY CTRY_JAPAN
		CTRY_MEXICO CTRY_NETHERLANDS CTRY_NEW_ZEALAND CTRY_NORWAY
		CTRY_PORTUGAL CTRY_PRCHINA CTRY_SOUTH_KOREA CTRY_SPAIN
		CTRY_SWEDEN CTRY_SWITZERLAND CTRY_TAIWAN CTRY_UNITED_KINGDOM
		CTRY_UNITED_STATES)],
 LOCALE	 => [qw(LOCALE_NOUSEROVERRIDE LOCALE_ILANGUAGE LOCALE_SLANGUAGE
		LOCALE_SENGLANGUAGE LOCALE_SABBREVLANGNAME
		LOCALE_SNATIVELANGNAME LOCALE_ICOUNTRY LOCALE_SCOUNTRY
		LOCALE_SENGCOUNTRY LOCALE_SABBREVCTRYNAME LOCALE_SNATIVECTRYNAME
		LOCALE_IDEFAULTLANGUAGE LOCALE_IDEFAULTCOUNTRY
		LOCALE_IDEFAULTCODEPAGE LOCALE_IDEFAULTANSICODEPAGE LOCALE_SLIST
		LOCALE_IMEASURE LOCALE_SDECIMAL LOCALE_STHOUSAND
		LOCALE_SGROUPING LOCALE_IDIGITS LOCALE_ILZERO LOCALE_INEGNUMBER
		LOCALE_SNATIVEDIGITS LOCALE_SCURRENCY LOCALE_SINTLSYMBOL
		LOCALE_SMONDECIMALSEP LOCALE_SMONTHOUSANDSEP LOCALE_SMONGROUPING
		LOCALE_ICURRDIGITS LOCALE_IINTLCURRDIGITS LOCALE_ICURRENCY
		LOCALE_INEGCURR LOCALE_SDATE LOCALE_STIME LOCALE_SSHORTDATE
		LOCALE_SLONGDATE LOCALE_STIMEFORMAT LOCALE_IDATE LOCALE_ILDATE
		LOCALE_ITIME LOCALE_ITIMEMARKPOSN LOCALE_ICENTURY LOCALE_ITLZERO
		LOCALE_IDAYLZERO LOCALE_IMONLZERO LOCALE_S1159 LOCALE_S2359
		LOCALE_ICALENDARTYPE LOCALE_IOPTIONALCALENDAR
		LOCALE_IFIRSTDAYOFWEEK LOCALE_IFIRSTWEEKOFYEAR LOCALE_SDAYNAME1
		LOCALE_SDAYNAME2 LOCALE_SDAYNAME3 LOCALE_SDAYNAME4
		LOCALE_SDAYNAME5 LOCALE_SDAYNAME6 LOCALE_SDAYNAME7
		LOCALE_SABBREVDAYNAME1 LOCALE_SABBREVDAYNAME2
		LOCALE_SABBREVDAYNAME3 LOCALE_SABBREVDAYNAME4
		LOCALE_SABBREVDAYNAME5 LOCALE_SABBREVDAYNAME6
		LOCALE_SABBREVDAYNAME7 LOCALE_SMONTHNAME1 LOCALE_SMONTHNAME2
		LOCALE_SMONTHNAME3 LOCALE_SMONTHNAME4 LOCALE_SMONTHNAME5
		LOCALE_SMONTHNAME6 LOCALE_SMONTHNAME7 LOCALE_SMONTHNAME8
		LOCALE_SMONTHNAME9 LOCALE_SMONTHNAME10 LOCALE_SMONTHNAME11
		LOCALE_SMONTHNAME12 LOCALE_SMONTHNAME13 LOCALE_SABBREVMONTHNAME1
		LOCALE_SABBREVMONTHNAME2 LOCALE_SABBREVMONTHNAME3
		LOCALE_SABBREVMONTHNAME4 LOCALE_SABBREVMONTHNAME5
		LOCALE_SABBREVMONTHNAME6 LOCALE_SABBREVMONTHNAME7
		LOCALE_SABBREVMONTHNAME8 LOCALE_SABBREVMONTHNAME9
		LOCALE_SABBREVMONTHNAME10 LOCALE_SABBREVMONTHNAME11
		LOCALE_SABBREVMONTHNAME12 LOCALE_SABBREVMONTHNAME13
		LOCALE_SPOSITIVESIGN LOCALE_SNEGATIVESIGN LOCALE_IPOSSIGNPOSN
		LOCALE_INEGSIGNPOSN LOCALE_IPOSSYMPRECEDES LOCALE_IPOSSEPBYSPACE
		LOCALE_INEGSYMPRECEDES LOCALE_INEGSEPBYSPACE)],
 TIME    => [qw(TIME_NOMINUTESORSECONDS TIME_NOSECONDS TIME_NOTIMEMARKER
                TIME_FORCE24HOURFORMAT)],
 DATE    => [qw(DATE_SHORTDATE DATE_LONGDATE DATE_USE_ALT_CALENDAR
                DATE_YEARMONTH DATE_LTRREADING DATE_RTLREADING)],
);

foreach my $tag (keys %EXPORT_TAGS) {
    push @EXPORT_OK, @{$EXPORT_TAGS{$tag}};
}

# Character Type Flags
sub CT_CTYPE1                       { 0x0001 }
sub CT_CTYPE2                       { 0x0002 }
sub CT_CTYPE3                       { 0x0004 }

# Character Type 1 Bits
sub C1_UPPER                        { 0x0001 }
sub C1_LOWER                        { 0x0002 }
sub C1_DIGIT                        { 0x0004 }
sub C1_SPACE                        { 0x0008 }
sub C1_PUNCT                        { 0x0010 }
sub C1_CNTRL                        { 0x0020 }
sub C1_BLANK                        { 0x0040 }
sub C1_XDIGIT                       { 0x0080 }
sub C1_ALPHA                        { 0x0100 }

# Character Type 2 Bits
sub C2_LEFTTORIGHT                  { 0x1 }
sub C2_RIGHTTOLEFT                  { 0x2 }
sub C2_EUROPENUMBER                 { 0x3 }
sub C2_EUROPESEPARATOR              { 0x4 }
sub C2_EUROPETERMINATOR             { 0x5 }
sub C2_ARABICNUMBER                 { 0x6 }
sub C2_COMMONSEPARATOR              { 0x7 }
sub C2_BLOCKSEPARATOR               { 0x8 }
sub C2_SEGMENTSEPARATOR             { 0x9 }
sub C2_WHITESPACE                   { 0xA }
sub C2_OTHERNEUTRAL                 { 0xB }
sub C2_NOTAPPLICABLE                { 0x0 }

# Character Type 3 Bits
sub C3_NONSPACING                   { 0x0001 }
sub C3_DIACRITIC                    { 0x0002 }
sub C3_VOWELMARK                    { 0x0004 }
sub C3_SYMBOL                       { 0x0008 }
sub C3_KATAKANA                     { 0x0010 }
sub C3_HIRAGANA                     { 0x0020 }
sub C3_HALFWIDTH                    { 0x0040 }
sub C3_FULLWIDTH                    { 0x0080 }
sub C3_IDEOGRAPH                    { 0x0100 }
sub C3_KASHIDA                      { 0x0200 }
sub C3_ALPHA                        { 0x8000 }
sub C3_NOTAPPLICABLE                { 0x0 }

# String Flags
sub NORM_IGNORECASE                 { 0x0001 }
sub NORM_IGNORENONSPACE             { 0x0002 }
sub NORM_IGNORESYMBOLS              { 0x0004 }
sub NORM_IGNOREWIDTH                { 0x0008 }
sub NORM_IGNOREKANATYPE             { 0x0040 }
sub NORM_IGNOREKASHIDA              { 0x40000}

# Locale Dependent Mapping Flags
sub LCMAP_LOWERCASE                 { 0x0100 }
sub LCMAP_UPPERCASE                 { 0x0200 }
sub LCMAP_SORTKEY                   { 0x0400 }
sub LCMAP_HALFWIDTH                 { 0x0800 }
sub LCMAP_FULLWIDTH                 { 0x1000 }
sub LCMAP_HIRAGANA                  { 0x2000 }
sub LCMAP_KATAKANA                  { 0x4000 }

# Primary Language Identifier
sub LANG_NEUTRAL                    { 0x00 }
sub LANG_ALBANIAN                   { 0x1c }
sub LANG_ARABIC                     { 0x01 }
sub LANG_BAHASA                     { 0x21 }
sub LANG_BULGARIAN                  { 0x02 }
sub LANG_CATALAN                    { 0x03 }
sub LANG_CHINESE                    { 0x04 }
sub LANG_CZECH                      { 0x05 }
sub LANG_DANISH                     { 0x06 }
sub LANG_DUTCH                      { 0x13 }
sub LANG_ENGLISH                    { 0x09 }
sub LANG_FINNISH                    { 0x0b }
sub LANG_FRENCH                     { 0x0c }
sub LANG_GERMAN                     { 0x07 }
sub LANG_GREEK                      { 0x08 }
sub LANG_HEBREW                     { 0x0d }
sub LANG_HUNGARIAN                  { 0x0e }
sub LANG_ICELANDIC                  { 0x0f }
sub LANG_ITALIAN                    { 0x10 }
sub LANG_JAPANESE                   { 0x11 }
sub LANG_KOREAN                     { 0x12 }
sub LANG_NORWEGIAN                  { 0x14 }
sub LANG_POLISH                     { 0x15 }
sub LANG_PORTUGUESE                 { 0x16 }
sub LANG_RHAETO_ROMAN               { 0x17 }
sub LANG_ROMANIAN                   { 0x18 }
sub LANG_RUSSIAN                    { 0x19 }
sub LANG_SERBO_CROATIAN             { 0x1a }
sub LANG_SLOVAK                     { 0x1b }
sub LANG_SPANISH                    { 0x0a }
sub LANG_SWEDISH                    { 0x1d }
sub LANG_THAI                       { 0x1e }
sub LANG_TURKISH                    { 0x1f }
sub LANG_URDU                       { 0x20 }

# Sublanguage Identifier
sub SUBLANG_NEUTRAL                 { 0x00 }
sub SUBLANG_DEFAULT                 { 0x01 }
sub SUBLANG_SYS_DEFAULT             { 0x02 }
sub SUBLANG_CHINESE_SIMPLIFIED      { 0x02 }
sub SUBLANG_CHINESE_TRADITIONAL     { 0x01 }
sub SUBLANG_DUTCH                   { 0x01 }
sub SUBLANG_DUTCH_BELGIAN           { 0x02 }
sub SUBLANG_ENGLISH_US              { 0x01 }
sub SUBLANG_ENGLISH_UK              { 0x02 }
sub SUBLANG_ENGLISH_AUS             { 0x03 }
sub SUBLANG_ENGLISH_CAN             { 0x04 }
sub SUBLANG_ENGLISH_NZ              { 0x05 }
sub SUBLANG_ENGLISH_EIRE            { 0x06 }
sub SUBLANG_FRENCH                  { 0x01 }
sub SUBLANG_FRENCH_BELGIAN          { 0x02 }
sub SUBLANG_FRENCH_CANADIAN         { 0x03 }
sub SUBLANG_FRENCH_SWISS            { 0x04 }
sub SUBLANG_GERMAN                  { 0x01 }
sub SUBLANG_GERMAN_SWISS            { 0x02 }
sub SUBLANG_GERMAN_AUSTRIAN         { 0x03 }
sub SUBLANG_ITALIAN                 { 0x01 }
sub SUBLANG_ITALIAN_SWISS           { 0x02 }
sub SUBLANG_NORWEGIAN_BOKMAL        { 0x01 }
sub SUBLANG_NORWEGIAN_NYNORSK       { 0x02 }
sub SUBLANG_PORTUGUESE              { 0x02 }
sub SUBLANG_PORTUGUESE_BRAZILIAN    { 0x01 }
sub SUBLANG_SERBO_CROATIAN_CYRILLIC { 0x02 }
sub SUBLANG_SERBO_CROATIAN_LATIN    { 0x01 }
sub SUBLANG_SPANISH                 { 0x01 }
sub SUBLANG_SPANISH_MEXICAN         { 0x02 }
sub SUBLANG_SPANISH_MODERN          { 0x03 }

# Country codes
sub CTRY_DEFAULT                    { 0   }
sub CTRY_AUSTRALIA                  { 61  }
sub CTRY_AUSTRIA                    { 43  }
sub CTRY_BELGIUM                    { 32  }
sub CTRY_BRAZIL                     { 55  }
sub CTRY_CANADA                     { 2   }
sub CTRY_DENMARK                    { 45  }
sub CTRY_FINLAND                    { 358 }
sub CTRY_FRANCE                     { 33  }
sub CTRY_GERMANY                    { 49  }
sub CTRY_ICELAND                    { 354 }
sub CTRY_IRELAND                    { 353 }
sub CTRY_ITALY                      { 39  }
sub CTRY_JAPAN                      { 81  }
sub CTRY_MEXICO                     { 52  }
sub CTRY_NETHERLANDS                { 31  }
sub CTRY_NEW_ZEALAND                { 64  }
sub CTRY_NORWAY                     { 47  }
sub CTRY_PORTUGAL                   { 351 }
sub CTRY_PRCHINA                    { 86  }
sub CTRY_SOUTH_KOREA                { 82  }
sub CTRY_SPAIN                      { 34  }
sub CTRY_SWEDEN                     { 46  }
sub CTRY_SWITZERLAND                { 41  }
sub CTRY_TAIWAN                     { 886 }
sub CTRY_UNITED_KINGDOM             { 44  }
sub CTRY_UNITED_STATES              { 1   }

# Locale Types
sub LOCALE_NOUSEROVERRIDE           { 0x80000000 }
sub LOCALE_ILANGUAGE                { 0x0001 }
sub LOCALE_SLANGUAGE                { 0x0002 }
sub LOCALE_SENGLANGUAGE             { 0x1001 }
sub LOCALE_SABBREVLANGNAME          { 0x0003 }
sub LOCALE_SNATIVELANGNAME          { 0x0004 }
sub LOCALE_ICOUNTRY                 { 0x0005 }
sub LOCALE_SCOUNTRY                 { 0x0006 }
sub LOCALE_SENGCOUNTRY              { 0x1002 }
sub LOCALE_SABBREVCTRYNAME          { 0x0007 }
sub LOCALE_SNATIVECTRYNAME          { 0x0008 }
sub LOCALE_IDEFAULTLANGUAGE         { 0x0009 }
sub LOCALE_IDEFAULTCOUNTRY          { 0x000A }
sub LOCALE_IDEFAULTCODEPAGE         { 0x000B }
sub LOCALE_IDEFAULTANSICODEPAGE     { 0x1004 }
sub LOCALE_SLIST                    { 0x000C }
sub LOCALE_IMEASURE                 { 0x000D }
sub LOCALE_SDECIMAL                 { 0x000E }
sub LOCALE_STHOUSAND                { 0x000F }
sub LOCALE_SGROUPING                { 0x0010 }
sub LOCALE_IDIGITS                  { 0x0011 }
sub LOCALE_ILZERO                   { 0x0012 }
sub LOCALE_INEGNUMBER               { 0x1010 }
sub LOCALE_SNATIVEDIGITS            { 0x0013 }
sub LOCALE_SCURRENCY                { 0x0014 }
sub LOCALE_SINTLSYMBOL              { 0x0015 }
sub LOCALE_SMONDECIMALSEP           { 0x0016 }
sub LOCALE_SMONTHOUSANDSEP          { 0x0017 }
sub LOCALE_SMONGROUPING             { 0x0018 }
sub LOCALE_ICURRDIGITS              { 0x0019 }
sub LOCALE_IINTLCURRDIGITS          { 0x001A }
sub LOCALE_ICURRENCY                { 0x001B }
sub LOCALE_INEGCURR                 { 0x001C }
sub LOCALE_SDATE                    { 0x001D }
sub LOCALE_STIME                    { 0x001E }
sub LOCALE_SSHORTDATE               { 0x001F }
sub LOCALE_SLONGDATE                { 0x0020 }
sub LOCALE_STIMEFORMAT              { 0x1003 }
sub LOCALE_IDATE                    { 0x0021 }
sub LOCALE_ILDATE                   { 0x0022 }
sub LOCALE_ITIME                    { 0x0023 }
sub LOCALE_ITIMEMARKPOSN            { 0x1005 }
sub LOCALE_ICENTURY                 { 0x0024 }
sub LOCALE_ITLZERO                  { 0x0025 }
sub LOCALE_IDAYLZERO                { 0x0026 }
sub LOCALE_IMONLZERO                { 0x0027 }
sub LOCALE_S1159                    { 0x0028 }
sub LOCALE_S2359                    { 0x0029 }
sub LOCALE_ICALENDARTYPE            { 0x1009 }
sub LOCALE_IOPTIONALCALENDAR        { 0x100B }
sub LOCALE_IFIRSTDAYOFWEEK          { 0x100C }
sub LOCALE_IFIRSTWEEKOFYEAR         { 0x100D }
sub LOCALE_SDAYNAME1                { 0x002A }
sub LOCALE_SDAYNAME2                { 0x002B }
sub LOCALE_SDAYNAME3                { 0x002C }
sub LOCALE_SDAYNAME4                { 0x002D }
sub LOCALE_SDAYNAME5                { 0x002E }
sub LOCALE_SDAYNAME6                { 0x002F }
sub LOCALE_SDAYNAME7                { 0x0030 }
sub LOCALE_SABBREVDAYNAME1          { 0x0031 }
sub LOCALE_SABBREVDAYNAME2          { 0x0032 }
sub LOCALE_SABBREVDAYNAME3          { 0x0033 }
sub LOCALE_SABBREVDAYNAME4          { 0x0034 }
sub LOCALE_SABBREVDAYNAME5          { 0x0035 }
sub LOCALE_SABBREVDAYNAME6          { 0x0036 }
sub LOCALE_SABBREVDAYNAME7          { 0x0037 }
sub LOCALE_SMONTHNAME1              { 0x0038 }
sub LOCALE_SMONTHNAME2              { 0x0039 }
sub LOCALE_SMONTHNAME3              { 0x003A }
sub LOCALE_SMONTHNAME4              { 0x003B }
sub LOCALE_SMONTHNAME5              { 0x003C }
sub LOCALE_SMONTHNAME6              { 0x003D }
sub LOCALE_SMONTHNAME7              { 0x003E }
sub LOCALE_SMONTHNAME8              { 0x003F }
sub LOCALE_SMONTHNAME9              { 0x0040 }
sub LOCALE_SMONTHNAME10             { 0x0041 }
sub LOCALE_SMONTHNAME11             { 0x0042 }
sub LOCALE_SMONTHNAME12             { 0x0043 }
sub LOCALE_SMONTHNAME13             { 0x100E }
sub LOCALE_SABBREVMONTHNAME1        { 0x0044 }
sub LOCALE_SABBREVMONTHNAME2        { 0x0045 }
sub LOCALE_SABBREVMONTHNAME3        { 0x0046 }
sub LOCALE_SABBREVMONTHNAME4        { 0x0047 }
sub LOCALE_SABBREVMONTHNAME5        { 0x0048 }
sub LOCALE_SABBREVMONTHNAME6        { 0x0049 }
sub LOCALE_SABBREVMONTHNAME7        { 0x004A }
sub LOCALE_SABBREVMONTHNAME8        { 0x004B }
sub LOCALE_SABBREVMONTHNAME9        { 0x004C }
sub LOCALE_SABBREVMONTHNAME10       { 0x004D }
sub LOCALE_SABBREVMONTHNAME11       { 0x004E }
sub LOCALE_SABBREVMONTHNAME12       { 0x004F }
sub LOCALE_SABBREVMONTHNAME13       { 0x100F }
sub LOCALE_SPOSITIVESIGN            { 0x0050 }
sub LOCALE_SNEGATIVESIGN            { 0x0051 }
sub LOCALE_IPOSSIGNPOSN             { 0x0052 }
sub LOCALE_INEGSIGNPOSN             { 0x0053 }
sub LOCALE_IPOSSYMPRECEDES          { 0x0054 }
sub LOCALE_IPOSSEPBYSPACE           { 0x0055 }
sub LOCALE_INEGSYMPRECEDES          { 0x0056 }
sub LOCALE_INEGSEPBYSPACE           { 0x0057 }

# GetTimeFormat Flags
sub TIME_NOMINUTESORSECONDS         { 0x0001 }
sub TIME_NOSECONDS                  { 0x0002 }
sub TIME_NOTIMEMARKER               { 0x0004 }
sub TIME_FORCE24HOURFORMAT          { 0x0008 }

# GetDateFormat Flags
sub DATE_SHORTDATE		    { 0x0001 }
sub DATE_LONGDATE                   { 0x0002 }
sub DATE_USE_ALT_CALENDAR           { 0x0004 }
sub DATE_YEARMONTH                  { 0x0008 }
sub DATE_LTRREADING                 { 0x0010 }
sub DATE_RTLREADING                 { 0x0020 }

# Language Identifier Functions
sub MAKELANGID	   { my ($p,$s) = @_; (($s & 0xffff) << 10) | ($p & 0xffff); }
sub PRIMARYLANGID  { my $lgid = shift; $lgid & 0x3ff; }
sub SUBLANGID	   { my $lgid = shift; ($lgid >> 10) & 0x3f; }

sub LANG_SYSTEM_DEFAULT { MAKELANGID(LANG_NEUTRAL, SUBLANG_SYS_DEFAULT); }
sub LANG_USER_DEFAULT   { MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT); }

# Locale Identifier Functions
sub MAKELCID	   { my $lgid = shift; $lgid & 0xffff; }
sub LANGIDFROMLCID { my $lcid = shift; $lcid & 0xffff; }

sub LOCALE_SYSTEM_DEFAULT { MAKELCID(LANG_SYSTEM_DEFAULT); }
sub LOCALE_USER_DEFAULT   { MAKELCID(LANG_USER_DEFAULT); }

1;

__END__

=head1 NAME

Win32::OLE::NLS - OLE National Language Support

=head1 SYNOPSIS

	missing

=head1 DESCRIPTION

This module provides access to the national language support features
in the F<OLENLS.DLL>.

=head2 Functions

=over 8

=item CompareString(LCID,FLAGS,STR1,STR2)

Compare STR1 and STR2 in the LCID locale.  FLAGS indicate the character
traits to be used or ignored when comparing the two strings.

	NORM_IGNORECASE		Ignore case
	NORM_IGNOREKANATYPE	Ignore hiragana/katakana character differences
	NORM_IGNORENONSPACE	Ignore accents, diacritics, and vowel marks
	NORM_IGNORESYMBOLS	Ignore symbols
	NORM_IGNOREWIDTH	Ignore character width

Possible return values are:

	0	Function failed
	1	STR1 is less than STR2
	2	STR1 is equal to STR2
	3	STR1 is greater than STR2

Note that you can subtract 2 from the return code to get values
comparable to the C<cmp> operator.

=item LCMapString(LCID,FLAGS,STR)

LCMapString translates STR using LCID dependent translation.
Flags contains a combination of the following options:

	LCMAP_LOWERCASE		Lowercase
	LCMAP_UPPERCASE		Uppercase
	LCMAP_HALFWIDTH		Narrow characters
	LCMAP_FULLWIDTH		Wide characters
	LCMAP_HIRAGANA		Hiragana
	LCMAP_KATAKANA		Katakana
	LCMAP_SORTKEY		Character sort key

The following normalization options can be combined with C<LCMAP_SORTKEY>:

	NORM_IGNORECASE		Ignore case
	NORM_IGNOREKANATYPE	Ignore hiragana/katakana character differences
	NORM_IGNORENONSPACE	Ignore accents, diacritics, and vowel marks
	NORM_IGNORESYMBOLS	Ignore symbols
	NORM_IGNOREWIDTH	Ignore character width

The return value is the translated string.

=item GetLocaleInfo(LCID,LCTYPE)

Retrieve locale setting LCTYPE from the locale specified by LCID.  Use
LOCALE_NOUSEROVERRIDE | LCTYPE to always query the locale database.
Otherwise user changes to C<win.ini> through the windows control panel
take precedence when retrieving values for the system default locale.
See the documentation below for a list of valid LCTYPE values.

The return value is the contents of the requested locale setting.

=item GetStringType(LCID,TYPE,STR)

Retrieve type information from locale LCID about each character in STR.
The requested TYPE can be one of the following 3 levels:

	CT_CTYPE1		ANSI C and POSIX type information
	CT_CTYPE2		Text layout type information
	CT_CTYPE3		Text processing type information

The return value is a list of values, each of wich is a bitwise OR of
the applicable type bits from the corresponding table below:

	@ct = GetStringType(LOCALE_SYSTEM_DEFAULT, CT_CTYPE1, "String");

ANSI C and POSIX character type information:

	C1_UPPER		Uppercase
	C1_LOWER		Lowercase
	C1_DIGIT		Decimal digits
	C1_SPACE		Space characters
	C1_PUNCT		Punctuation
	C1_CNTRL		Control characters
	C1_BLANK		Blank characters
	C1_XDIGIT		Hexadecimal digits
	C1_ALPHA		Any letter

Text layout type information:

	C2_LEFTTORIGHT		Left to right
	C2_RIGHTTOLEFT		Right to left
	C2_EUROPENUMBER		European number, European digit
	C2_EUROPESEPARATOR	European numeric separator
	C2_EUROPETERMINATOR	European numeric terminator
	C2_ARABICNUMBER		Arabic number
	C2_COMMONSEPARATOR	Common numeric separator
	C2_BLOCKSEPARATOR	Block separator
	C2_SEGMENTSEPARATOR	Segment separator
	C2_WHITESPACE		White space
	C2_OTHERNEUTRAL		Other neutrals
	C2_NOTAPPLICABLE	No implicit direction (e.g. ctrl codes)

Text precessing type information:

	C3_NONSPACING		Nonspacing mark
	C3_DIACRITIC		Diacritic nonspacing mark
	C3_VOWELMARK		Vowel nonspacing mark
	C3_SYMBOL		Symbol
	C3_KATAKANA		Katakana character
	C3_HIRAGANA		Hiragana character
	C3_HALFWIDTH		Narrow character
	C3_FULLWIDTH		Wide character
	C3_IDEOGRAPH		Ideograph
	C3_ALPHA		Any letter
	C3_NOTAPPLICABLE	Not applicable


=item GetSystemDefaultLangID()

Returns the system default language identifier.

=item GetSystemDefaultLCID()

Returns the system default locale identifier.

=item GetUserDefaultLangID()

Returns the user default language identifier.

=item GetUserDefaultLCID()

Returns the user default locale identifier.

=item SendSettingChange()

Sends a WM_SETTINGCHANGE message to all top level windows.

=item SetLocaleInfo(LCID, LCTYPE, LCDATA)

Changes an item in the user override part of the locale setting LCID.
It doesn't change the system default database.  The following LCTYPEs are
changeable:

	LOCALE_ICALENDARTYPE	LOCALE_SDATE
	LOCALE_ICURRDIGITS	LOCALE_SDECIMAL
	LOCALE_ICURRENCY	LOCALE_SGROUPING
	LOCALE_IDIGITS		LOCALE_SLIST
	LOCALE_IFIRSTDAYOFWEEK	LOCALE_SLONGDATE
	LOCALE_IFIRSTWEEKOFYEAR	LOCALE_SMONDECIMALSEP
	LOCALE_ILZERO		LOCALE_SMONGROUPING
	LOCALE_IMEASURE		LOCALE_SMONTHOUSANDSEP
	LOCALE_INEGCURR		LOCALE_SNEGATIVESIGN
	LOCALE_INEGNUMBER	LOCALE_SPOSITIVESIGN
	LOCALE_IPAPERSIZE	LOCALE_SSHORTDATE
	LOCALE_ITIME		LOCALE_STHOUSAND
	LOCALE_S1159		LOCALE_STIME
	LOCALE_S2359		LOCALE_STIMEFORMAT
	LOCALE_SCURRENCY	LOCALE_SYEARMONTH

You have to call SendSettingChange() to activate these changes for
subsequent Win32::OLE::Variant object formatting because the OLE
subsystem seems to cache locale information.

=item MAKELANGID(LANG,SUBLANG)

Creates a lnguage identifier from a primary language and a sublanguage.

=item PRIMARYLANGID(LANGID)

Retrieves the primary language from a language identifier.

=item SUBLANGID(LANGID)

Retrieves the sublanguage from a language identifier.

=item MAKELCID(LANGID)

Creates a locale identifies from a language identifier.

=item LANGIDFROMLCID(LCID)

Retrieves a language identifier from a locale identifier.

=back

=head2 Locale Types

=over 8

=item LOCALE_ILANGUAGE

The language identifier (in hex).

=item LOCALE_SLANGUAGE

The localized name of the language.

=item LOCALE_SENGLANGUAGE

The ISO Standard 639 English name of the language.

=item LOCALE_SABBREVLANGNAME

The three-letter abbreviated name of the language.  The first two
letters are from the ISO Standard 639 language name abbreviation.  The
third letter indicates the sublanguage type.

=item LOCALE_SNATIVELANGNAME

The native name of the language.

=item LOCALE_ICOUNTRY

The country code, which is based on international phone codes.

=item LOCALE_SCOUNTRY

The localized name of the country.

=item LOCALE_SENGCOUNTRY

The English name of the country.

=item LOCALE_SABBREVCTRYNAME

The ISO Standard 3166 abbreviated name of the country.

=item LOCALE_SNATIVECTRYNAME

The native name of the country.

=item LOCALE_IDEFAULTLANGUAGE

Language identifier for the principal language spoken in this
locale.

=item LOCALE_IDEFAULTCOUNTRY

Country code for the principal country in this locale.

=item LOCALE_IDEFAULTANSICODEPAGE

The ANSI code page associated with this locale.  Format: 4 Unicode
decimal digits plus a Unicode null terminator.

XXX This should be translated by GetLocaleInfo. XXX

=item LOCALE_IDEFAULTCODEPAGE

The OEM code page associated with the country.

=item LOCALE_SLIST

Characters used to separate list items (often a comma).

=item LOCALE_IMEASURE

Default measurement system:

	0	metric system (S.I.)
	1	U.S. system

=item LOCALE_SDECIMAL

Characters used for the decimal separator (often a dot).

=item LOCALE_STHOUSAND

Characters used as the separator between groups of digits left of the decimal.

=item LOCALE_SGROUPING

Sizes for each group of digits to the left of the decimal.  An explicit
size is required for each group.  Sizes are separated by semicolons.  If
the last value is 0, the preceding value is repeated.  To group
thousands, specify 3;0.

=item LOCALE_IDIGITS

The number of fractional digits.

=item LOCALE_ILZERO

Whether to use leading zeros in decimal fields.  A setting of 0
means use no leading zeros; 1 means use leading zeros.

=item LOCALE_SNATIVEDIGITS

The ten characters that are the native equivalent of the ASCII 0-9.

=item LOCALE_INEGNUMBER

Negative number mode.

	0 	(1.1)
	1 	-1.1
	2 	-1.1
	3 	1.1
	4 	1.1

=item LOCALE_SCURRENCY

The string used as the local monetary symbol.

=item LOCALE_SINTLSYMBOL

Three characters of the International monetary symbol specified in ISO
4217, Codes for the Representation of Currencies and Funds, followed
by the character separating this string from the amount.

=item LOCALE_SMONDECIMALSEP

Characters used for the monetary decimal separators.

=item LOCALE_SMONTHOUSANDSEP

Characters used as monetary separator between groups of digits left of
the decimal.

=item LOCALE_SMONGROUPING

Sizes for each group of monetary digits to the left of the decimal.  An
explicit size is needed for each group.  Sizes are separated by
semicolons.  If the last value is 0, the preceding value is
repeated.  To group thousands, specify 3;0.

=item LOCALE_ICURRDIGITS

Number of fractional digits for the local monetary format.

=item LOCALE_IINTLCURRDIGITS

Number of fractional digits for the international monetary format.

=item LOCALE_ICURRENCY

Positive currency mode.

	0	Prefix, no separation.
	1	Suffix, no separation.
	2	Prefix, 1-character separation.
	3	Suffix, 1-character separation.

=item LOCALE_INEGCURR

Negative currency mode.

	0	($1.1)
	1	-$1.1
	2	$-1.1
	3	$1.1-
	4	$(1.1$)
	5	-1.1$
	6	1.1-$
	7	1.1$-
	8	-1.1 $ (space before $)
	9	-$ 1.1 (space after $)
	10	1.1 $- (space before $)

=item LOCALE_ICALENDARTYPE

The type of calendar currently in use.

	1	Gregorian (as in U.S.)
	2	Gregorian (always English strings)
	3	Era: Year of the Emperor (Japan)
	4	Era: Year of the Republic of China
	5	Tangun Era (Korea)

=item LOCALE_IOPTIONALCALENDAR

The additional calendar types available for this LCID.  Can be a
null-separated list of all valid optional calendars.  Value is
0 for "None available" or any of the LOCALE_ICALENDARTYPE settings.

XXX null separated list should be translated by GetLocaleInfo XXX

=item LOCALE_SDATE

Characters used for the date separator.

=item LOCALE_STIME

Characters used for the time separator.

=item LOCALE_STIMEFORMAT

Time-formatting string.

=item LOCALE_SSHORTDATE

Short Date_Time formatting strings for this locale.

=item LOCALE_SLONGDATE

Long Date_Time formatting strings for this locale.

=item LOCALE_IDATE

Short Date format-ordering specifier.

	0	Month - Day - Year
	1	Day - Month - Year
	2	Year - Month - Day

=item LOCALE_ILDATE

Long Date format ordering specifier.  Value can be any of the valid
LOCALE_IDATE settings.

=item LOCALE_ITIME

Time format specifier.

	0	AM/PM 12-hour format.
	1	24-hour format.

=item LOCALE_ITIMEMARKPOSN

Whether the time marker string (AM|PM) precedes or follows the time
string.
	0 Suffix (9:15 AM).
	1 Prefix (AM 9:15).

=item LOCALE_ICENTURY

Whether to use full 4-digit century.

	0	Two digit.
	1	Full century.

=item LOCALE_ITLZERO

Whether to use leading zeros in time fields.

	0	No leading zeros.
	1	Leading zeros for hours.

=item LOCALE_IDAYLZERO

Whether to use leading zeros in day fields.  Values as for
LOCALE_ITLZERO.

=item LOCALE_IMONLZERO

Whether to use leading zeros in month fields.  Values as for
LOCALE_ITLZERO.

=item LOCALE_S1159

String for the AM designator.

=item LOCALE_S2359

String for the PM designator.

=item LOCALE_IFIRSTWEEKOFYEAR

Specifies which week of the year is considered first.

	0	Week containing 1/1 is the first week of the year.
	1	First full week following 1/1is the first week of the year.
	2	First week with at least 4 days is the first week of the year.

=item LOCALE_IFIRSTDAYOFWEEK

Specifies the day considered first in the week.  Value "0" means
SDAYNAME1 and value "6" means SDAYNAME7.

=item LOCALE_SDAYNAME1 .. LOCALE_SDAYNAME7

Long name for Monday .. Sunday.

=item LOCALE_SABBREVDAYNAME1 .. LOCALE_SABBREVDAYNAME7

Abbreviated name for Monday .. Sunday.

=item LOCALE_SMONTHNAME1 .. LOCALE_SMONTHNAME12

Long name for January .. December.

=item LOCALE_SMONTHNAME13

Native name for 13th month, if it exists.

=item LOCALE_SABBREVMONTHNAME1 .. LOCALE_SABBREVMONTHNAME12

Abbreviated name for January .. December.

=item LOCALE_SABBREVMONTHNAME13

Native abbreviated name for 13th month, if it exists.

=item LOCALE_SPOSITIVESIGN

String value for the positive sign.

=item LOCALE_SNEGATIVESIGN

String value for the negative sign.

=item LOCALE_IPOSSIGNPOSN

Formatting index for positive values.

	0 Parentheses surround the amount and the monetary symbol.
	1 The sign string precedes the amount and the monetary symbol.
	2 The sign string precedes the amount and the monetary symbol.
	3 The sign string precedes the amount and the monetary symbol.
	4 The sign string precedes the amount and the monetary symbol.

=item LOCALE_INEGSIGNPOSN

Formatting index for negative values.  Values as for LOCALE_IPOSSIGNPOSN.

=item LOCALE_IPOSSYMPRECEDES

If the monetary symbol precedes, 1.  If it succeeds a positive amount, 0.

=item LOCALE_IPOSSEPBYSPACE

If the monetary symbol is separated by a space from a positive amount,
1.  Otherwise, 0.

=item LOCALE_INEGSYMPRECEDES

If the monetary symbol precedes, 1.  If it succeeds a negative amount, 0.

=item LOCALE_INEGSEPBYSPACE

If the monetary symbol is separated by a space from a negative amount,
1.  Otherwise, 0.

=back

=head1 AUTHORS/COPYRIGHT

This module is part of the Win32::OLE distribution.

=cut
