// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_prepopulate_data.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

#if defined(OS_WIN)
#undef IN  // On Windows, windef.h defines this, which screws up "India" cases.
#elif defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#endif

using base::Time;

namespace {

// NOTE: See comments in GetDataVersion() below!  You should probably not change
// the data in this file without changing the result of that function!

// Engine definitions //////////////////////////////////////////////////////////

struct PrepopulatedEngine {
  const wchar_t* const name;
  // If NULL, we'll autogenerate a keyword based on the search_url every time
  // someone asks.  Only entries which need keywords to auto-track a dynamically
  // generated search URL should use this.
  // If the empty string, the engine has no keyword.
  const wchar_t* const keyword;      
  const char* const favicon_url;  // If NULL, there is no favicon.
  const wchar_t* const search_url;
  const char* const encoding;
  const wchar_t* const suggest_url;  // If NULL, this engine does not support
                                     // suggestions.
  // Unique id for this prepopulate engine (corresponds to
  // TemplateURL::prepopulate_id). This ID must be greater than zero and must
  // remain the same for a particular site regardless of how the url changes;
  // the ID is used when modifying engine data in subsequent versions, so that
  // we can find the "old" entry to update even when the name or URL changes.
  //
  // This ID must be "unique" within one country's prepopulated data, but two
  // entries can share an ID if they represent the "same" engine (e.g. Yahoo! US
  // vs. Yahoo! UK) and will not appear in the same user-visible data set.  This
  // facilitates changes like adding more specific per-country data in the
  // future; in such a case the localized engines will transparently replace the
  // previous, non-localized versions.  For engines where we need two instances
  // to appear for one country (e.g. Live Search U.S. English and Spanish), we
  // must use two different unique IDs (and different keywords).
  //
  // The following unique IDs are available: 66, 93, 103+
  // NOTE: CHANGE THE ABOVE NUMBERS IF YOU ADD A NEW ENGINE; ID conflicts = bad!
  const int id;
};

const PrepopulatedEngine abcsok = {
  L"ABC S\x00f8k",
  L"abcsok.no",
  "http://abcsok.no/favicon.ico",
  L"http://abcsok.no/index.html?q={searchTerms}",
  "UTF-8",
  NULL,
  72,
};

const PrepopulatedEngine adonde = {
  L"Adonde.com",
  L"adonde.com",
  "http://www.adonde.com/favicon.ico",
  L"http://www.adonde.com/peru/peru.html?sitesearch=adonde.com&"
      L"client=pub-6263803831447773&ie={inputEncoding}&cof=GALT%3A%23CC0000"
      L"%3BGL%3A1%3BDIV%3A%23E6E6E6%3BVLC%3A663399%3BAH%3Acenter%3BBGC%3AFFFFFF"
      L"%3BLBGC%3AFFFFFF%3BALC%3A000000%3BLC%3A000000%3BT%3A0066CC%3BGFNT"
      L"%3ACCCCCC%3BGIMP%3ACCCCCC%3BFORID%3A11&q={searchTerms}",
  "ISO-8859-1",
  NULL,
  95,
};

const PrepopulatedEngine aeiou = {
  L"AEIOU",
  L"aeiou.pt",
  "http://aeiou.pt/favicon.ico",
  L"http://aeiou.pt/pesquisa/index.php?p={searchTerms}",
  "ISO-8859-1",
  NULL,
  79,
};

const PrepopulatedEngine aladin = {
  L"Aladin",
  L"aladin.info",
  "http://www.aladin.info/favicon.ico",
  L"http://www.aladin.info/search/index.php?term={searchTerms}&req=search&"
      L"source=2",
  "UTF-8",
  NULL,
  18,
};

const PrepopulatedEngine altavista = {
  L"AltaVista",
  L"altavista.com",
  "http://www.altavista.com/favicon.ico",
  L"http://www.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine altavista_ar = {
  L"AltaVista",
  L"ar.altavista.com",
  "http://ar.altavista.com/favicon.ico",
  L"http://ar.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine altavista_es = {
  L"AltaVista",
  L"es.altavista.com",
  "http://es.altavista.com/favicon.ico",
  L"http://es.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine altavista_mx = {
  L"AltaVista",
  L"mx.altavista.com",
  "http://mx.altavista.com/favicon.ico",
  L"http://mx.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine altavista_se = {
  L"AltaVista",
  L"se.altavista.com",
  "http://se.altavista.com/favicon.ico",
  L"http://se.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine aol = {
  L"AOL",
  L"aol.com",
  "http://search.aol.com/favicon.ico",
  L"http://search.aol.com/aol/search?query={searchTerms}",
  "UTF-8",
  NULL,
  35,
};

const PrepopulatedEngine aol_fr = {
  L"AOL",
  L"aol.fr",
  "http://www.aol.fr/favicon.ico",
  L"http://www.recherche.aol.fr/aol/search?q={searchTerms}",
  "UTF-8",
  NULL,
  35,
};

const PrepopulatedEngine aonde = {
  L"AONDE.com",
  L"aonde.com",
  "http://busca.aonde.com/favicon.ico",
  L"http://busca.aonde.com/?keys={searchTerms}",
  "ISO-8859-1",
  NULL,
  80,
};

const PrepopulatedEngine araby = {
  L"\x0639\x0631\x0628\x064a",
  L"araby.com",
  "http://araby.com/favicon.ico",
  L"http://araby.com/?q={searchTerms}",
  "UTF-8",
  NULL,
  12,
};

const PrepopulatedEngine ask = {
  L"Ask",
  L"ask.com",
  "http://www.ask.com/favicon.ico",
  L"http://www.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine ask_de = {
  L"Ask.com Deutschland",
  L"de.ask.com",
  "http://de.ask.com/favicon.ico",
  L"http://de.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.de.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine ask_es = {
  L"Ask.com Espa" L"\x00f1" L"a",
  L"es.ask.com",
  "http://es.ask.com/favicon.ico",
  L"http://es.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.es.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine ask_it = {
  L"Ask.com Italia",
  L"it.ask.com",
  "http://it.ask.com/favicon.ico",
  L"http://it.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.it.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine ask_uk = {
  L"Ask.com UK",
  L"uk.ask.com",
  "http://uk.ask.com/favicon.ico",
  L"http://uk.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.uk.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine atlas_cz = {
  L"Atlas",
  L"atlas.cz",
  "http://img.atlas.cz/favicon.ico",
  L"http://search.atlas.cz/?q={searchTerms}",
  "windows-1250",
  NULL,
  27,
};

const PrepopulatedEngine atlas_sk = {
  L"ATLAS.SK",
  L"atlas.sk",
  "http://www.atlas.sk/images/favicon.ico",
  L"http://hladaj.atlas.sk/fulltext/?phrase={searchTerms}",
  "UTF-8",
  NULL,
  27,
};

const PrepopulatedEngine baidu = {
  L"\x767e\x5ea6",
  L"baidu.com",
  "http://www.baidu.com/favicon.ico",
  L"http://www.baidu.com/s?wd={searchTerms}",
  "GB2312",
  NULL,
  21,
};

const PrepopulatedEngine biglobe = {
  L"BIGLOBE",
  L"biglobe.ne.jp",
  "http://cgi.search.biglobe.ne.jp/favicon.ico",
  L"http://cgi.search.biglobe.ne.jp/cgi-bin/search2-b?q={searchTerms}",
  "Shift_JIS",
  NULL,
  64,
};

const PrepopulatedEngine bigmir = {
  L"bigmir)net",
  L"bigmir.net",
  "http://i.bigmir.net/favicon.ico",
  L"http://search.bigmir.net/index.php?q={searchTerms}",
  "windows-1251",
  NULL,
  33,
};

const PrepopulatedEngine bluewin = {
  L"Bluewin",
  L"search.bluewin.ch",
  "http://search.bluewin.ch/favicon.ico",
  L"http://search.bluewin.ch/bw/search/web/de/result.jsp?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  52,
};

const PrepopulatedEngine centrum_cz = {
  L"Centrum.cz",
  L"centrum.cz",
  "http://img.centrum.cz/6/vy2/o/favicon.ico",
  L"http://search.centrum.cz/index.php?charset={inputEncoding}&q={searchTerms}",
  "UTF-8",
  NULL,
  26,
};

const PrepopulatedEngine centrum_sk = {
  L"Centrum.sk",
  L"centrum.sk",
  "http://img.centrum.sk/4/favicon.ico",
  L"http://search.centrum.sk/index.php?charset={inputEncoding}&q={searchTerms}",
  "UTF-8",
  NULL,
  26,
};

const PrepopulatedEngine conexcol = {
  L"Conexcol.com",
  L"conexcol.com",
  "http://www.conexcol.com/favicon.ico",
  L"http://buscar.conexcol.com/cgi-ps/busqueda.cgi?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  91,
};

const PrepopulatedEngine daum = {
  L"Daum",
  L"daum.net",
  "http://search.daum.net/favicon.ico",
  L"http://search.daum.net/search?q={searchTerms}",
  "EUC-KR",
   L"http://sug.search.daum.net/search_nsuggest?mod=fxjson&q={searchTerms}",
  68,
};

const PrepopulatedEngine delfi_ee = {
  L"DELFI",
  L"delfi.ee",
  "http://g.delfi.ee/s/search.png",
  L"http://otsing.delfi.ee/i.php?q={searchTerms}",
  "ISO-8859-1",
  NULL,
  45,
};

const PrepopulatedEngine delfi_lt = {
  L"DELFI",
  L"delfi.lt",
  "http://search.delfi.lt/img/favicon.png",
  L"http://search.delfi.lt/search.php?q={searchTerms}",
  "UTF-8",
  NULL,
  45,
};

const PrepopulatedEngine delfi_lv = {
  L"DELFI",
  L"delfi.lv",
  "http://smart.delfi.lv/img/smart_search.png",
  L"http://smart.delfi.lv/i.php?enc={inputEncoding}&q={searchTerms}",
  "UTF-8",
  NULL,
  45,
};

const PrepopulatedEngine embla = {
  L"Embla",
  L"embla.is",
  "http://embla.is/favicon.ico",
  L"http://embla.is/mm/embla/?s={searchTerms}",
  "ISO-8859-1",
  NULL,
  60,
};

const PrepopulatedEngine empas = {
  L"\xc5e0\xd30c\xc2a4",
  L"empas.com",
  "http://search.empas.com/favicon.ico",
  L"http://search.empas.com/search/all.html?q={searchTerms}",
  "EUC-KR",
  // http://www.empas.com/ac/do.tsp?q={searchTerms}
  // returns non-Firefox JSON.  searchTerms needs to be in Java notation
  // (\uAC00\uAC01).
  NULL,
  70,
};

const PrepopulatedEngine eniro_dk = {
  L"Eniro",
  L"eniro.dk",
  "http://eniro.dk/favicon.ico",
  L"http://eniro.dk/query?search_word={searchTerms}&what=web_local",
  "ISO-8859-1",
  NULL,
  29,
};

const PrepopulatedEngine eniro_fi = {
  L"Eniro",
  L"eniro.fi",
  "http://eniro.fi/favicon.ico",
  L"http://eniro.fi/query?search_word={searchTerms}&what=web_local",
  "ISO-8859-1",
  NULL,
  29,
};

const PrepopulatedEngine eniro_se = {
  L"Eniro",
  L"eniro.se",
  "http://eniro.se/favicon.ico",
  L"http://eniro.se/query?search_word={searchTerms}&what=web_local",
  "ISO-8859-1",
  NULL,
  29,
};

const PrepopulatedEngine finna = {
  L"FINNA",
  L"finna.is",
  "http://finna.is/favicon.ico",
  L"http://finna.is/WWW_Search/?query={searchTerms}",
  "UTF-8",
  NULL,
  61,
};

const PrepopulatedEngine fonecta_02_fi = {
  L"Fonecta 02.fi",
  L"www.fi",
  "http://www.02.fi/img/favicon.ico",
  L"http://www.02.fi/haku/{searchTerms}",
  "UTF-8",
  NULL,
  46,
};

const PrepopulatedEngine forthnet = {
  L"Forthnet",
  L"forthnet.gr",
  "http://search.forthnet.gr/favicon.ico",
  L"http://search.forthnet.gr/cgi-bin/query?mss=search&q={searchTerms}",
  "windows-1253",
  NULL,
  53,
};

const PrepopulatedEngine gigabusca = {
  L"GiGaBusca",
  L"gigabusca.com.br",
  "http://www.gigabusca.com.br/favicon.ico",
  L"http://www.gigabusca.com.br/buscar.php?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  81,
};

const PrepopulatedEngine go = {
  L"GO.com",
  L"go.com",
  "http://search.yahoo.com/favicon.ico",
  L"http://search.yahoo.com/search?ei={inputEncoding}&p={searchTerms}&"
      L"fr=hsusgo1",
  "ISO-8859-1",
  NULL,
  40,
};

const PrepopulatedEngine goo = {
  L"goo",
  L"goo.ne.jp",
  "http://goo.ne.jp/gooicon.ico",
  L"http://search.goo.ne.jp/web.jsp?MT={searchTerms}&IE={inputEncoding}",
  "UTF-8",
  NULL,
  92,
};

const PrepopulatedEngine google = {
  L"Google",
  NULL,
  "http://www.google.com/favicon.ico",
  L"{google:baseURL}search?{google:RLZ}{google:acceptedSuggestion}"
      L"{google:originalQueryForSuggestion}sourceid=chrome&ie={inputEncoding}&"
      L"q={searchTerms}",
  "UTF-8",
  L"{google:baseSuggestURL}search?client=chrome&output=chrome&hl={language}&"
      L"q={searchTerms}",
  1,
};

const PrepopulatedEngine guruji = {
  L"guruji",
  L"guruji.com",
  "http://guruji.com/favicon.ico",
  L"http://guruji.com/search?q={searchTerms}",
  "UTF-8",
  NULL,
  38,
};

const PrepopulatedEngine iafrica = {
  L"iafrica.com",
  L"iafrica.com",
  NULL,
  L"http://search.iafrica.com/search?q={searchTerms}",
  "ISO-8859-1",
  NULL,
  43,
};

const PrepopulatedEngine ilse = {
  L"Ilse",
  L"ilse.nl",
  "http://search.ilse.nl/images/favicon.ico",
  L"http://search.ilse.nl/web?search_for={searchTerms}",
  "ISO-8859-1",
  NULL,
  30,
};

const PrepopulatedEngine in = {
  L"in.gr",
  L"in.gr",
  "http://www.in.gr/favicon.ico",
  L"http://find.in.gr/result.asp?q={searchTerms}",
  "ISO-8859-7",
  NULL,
  54,
};

const PrepopulatedEngine jabse = {
  L"Jabse",
  L"jabse.com",
  "http://www.jabse.com/favicon.ico",
  L"http://www.jabse.com/searchmachine.php?query={searchTerms}",
  "UTF-8",
  NULL,
  19,
};

const PrepopulatedEngine jamaicalive = {
  L"JamaicaLive",
  L"jalive.com.jm",
  "http://jalive.com.jm/favicon.ico",
  L"http://jalive.com.jm/search/?mode=allwords&search={searchTerms}",
  "ISO-8859-1",
  NULL,
  39,
};

const PrepopulatedEngine jubii = {
  L"Jubii",
  L"jubii.dk",
  "http://search.jubii.dk/favicon_jubii.ico",
  L"http://search.jubii.dk/cgi-bin/pursuit?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  28,
};

const PrepopulatedEngine krstarica = {
  L"Krstarica",
  L"krstarica.rs",
  "http://pretraga.krstarica.com/favicon.ico",
  L"http://pretraga.krstarica.com/index.php?q={searchTerms}",
  "windows-1250",
  NULL,
  84,
};

const PrepopulatedEngine kvasir = {
  L"Kvasir",
  L"kvasir.no",
  "http://www.kvasir.no/img/favicon.ico",
  L"http://www.kvasir.no/nettsok/searchResult.html?searchExpr={searchTerms}",
  "ISO-8859-1",
  NULL,
  73,
};

const PrepopulatedEngine latne = {
  L"LATNE",
  L"latne.lv",
  "http://latne.lv/favicon.ico",
  L"http://latne.lv/siets.php?q={searchTerms}",
  "UTF-8",
  NULL,
  71,
};

const PrepopulatedEngine leit = {
  L"leit.is",
  L"leit.is",
  "http://leit.is/leit.ico",
  L"http://leit.is/query.aspx?qt={searchTerms}",
  "ISO-8859-1",
  NULL,
  59,
};

const PrepopulatedEngine libero = {
  L"Libero",
  L"libero.it",
  "http://arianna.libero.it/favicon.ico",
  L"http://arianna.libero.it/search/abin/integrata.cgi?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  63,
};

const PrepopulatedEngine live = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_ar_XA = {
  L"Live Search (\x0627\x0644\x0639\x0631\x0628\x064a\x0629)",
  L"",  // "live.com" is already taken by live_en_XA (see comment on ID below).
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=ar-XA&mkt=ar-XA&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  7,  // Can't be 3 as this has to appear in the Arabian countries' lists
      // alongside live_en_XA.
};

const PrepopulatedEngine live_bg_BG = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=bg-BG&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_cs_CZ = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=cs-CZ&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_el_GR = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=el-GR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_en_ID = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=en_ID&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_en_NZ = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=en-NZ&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_en_US = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=en-US&mkt=en-US&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_en_XA = {
  L"Live Search (English)",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=en-XA&mkt=en-XA&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_et_EE = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=et-EE&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_hr_HR = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=hr-HR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_hu_HU = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=hu-HU&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_it_IT = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=it-IT&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_lt_LT = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=lt-LT&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_pl_PL = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=pl-PL&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_pt_PT = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=pt-PT&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_ro_RO = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=ro-RO&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_ru_RU = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=ru-RU&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_sk_SK = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=sk-SK&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_sl_SI = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=sl-SI&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_th_TH = {
  L"Live Search",
  L"live.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=th-TH&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine lycos_es = {
  L"Lycos Espa" L"\x00f1" L"a",
  L"lycos.es",
  "http://buscador.lycos.es/favicon.ico",
  L"http://buscador.lycos.es/cgi-bin/pursuit?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  34,
};

const PrepopulatedEngine lycos_nl = {
  L"Lycos",
  L"lycos.nl",
  "http://zoek.lycos.nl/favicon.ico",
  L"http://zoek.lycos.nl/cgi-bin/pursuit?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  34,
};

const PrepopulatedEngine mail_ru = {
  L"@MAIL.RU",
  L"mail.ru",
  "http://img.go.mail.ru/favicon.ico",
  L"http://go.mail.ru/search?q={searchTerms}",
  "windows-1251",
  NULL,
  83,
};

const PrepopulatedEngine maktoob = {
  L"\x0645\x0643\x062a\x0648\x0628",
  L"maktoob.com",
  "http://www.maktoob.com/favicon.ico",
  L"http://www.maktoob.com/searchResult.php?q={searchTerms}",
  "UTF-8",
  NULL,
  13,
};

const PrepopulatedEngine masrawy = {
  L"\x0645\x0635\x0631\x0627\x0648\x064a",
  L"masrawy.com",
  "http://www.masrawy.com/new/images/masrawy.ico",
  L"http://masrawy.com/new/search.aspx?sr={searchTerms}",
  "windows-1256",
  NULL,
  14,
};

const PrepopulatedEngine matkurja = {
  L"Mat'Kurja",
  L"matkurja.com",
  "http://matkurja.com/favicon.ico",
  L"http://matkurja.com/si/iskalnik/?q={searchTerms}&search_source=directory",
  "ISO-8859-2",
  NULL,
  88,
};

const PrepopulatedEngine meta = {
  L"<META>",
  L"meta.ua",
  "http://meta.ua/favicon.ico",
  L"http://meta.ua/search.asp?q={searchTerms}",
  "windows-1251",
  L"http://meta.ua/suggestions/?output=fxjson&oe=utf-8&q={searchTerms}",
  102,
};

const PrepopulatedEngine msn = {
  L"MSN",
  L"msn.com",
  "http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_ar_XA = {
  L"MSN (\x0627\x0644\x0639\x0631\x0628\x064a\x0629)",
  L"",  // "arabia.msn.com" is already taken by msn_en_XA (see comment on ID
        // below).
  "http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?setlang=ar-XA&mkt=ar-XA&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  7,  // Can't be 3 as this has to appear in the Arabian countries' lists
      // alongside msn_en_XA.
};

const PrepopulatedEngine msn_da_DK = {
  L"MSN Danmark",
  L"dk.msn.com",
  "http://search.msn.dk/s/wlflag.ico",
  L"http://search.msn.dk/results.aspx?mkt=da-DK&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_de_AT = {
  L"MSN \x00d6sterreich",
  L"at.msn.com",
  "http://search.msn.at/s/wlflag.ico",
  L"http://search.msn.at/results.aspx?mkt=de-AT&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_de_CH = {
  L"MSN Schweiz (Deutsch)",
  L"ch.msn.com",
  "http://search.msn.ch/s/wlflag.ico",
  L"http://search.msn.ch/results.aspx?setlang=de-CH&mkt=de-CH&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_de_DE = {
  L"MSN",
  L"de.msn.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=de-DE&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_AU = {
  L"ninemsn.com.au",
  L"ninemsn.com.au",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=en-AU&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_CA = {
  L"Sympatico / MSN (English)",
  L"sympatico.msn.ca",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=en-CA&mkt=en-CA&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_GB = {
  L"MSN UK",
  L"uk.msn.com",
  "http://search.msn.co.uk/s/wlflag.ico",
  L"http://search.msn.co.uk/results.aspx?mkt=en-GB&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_IE = {
  L"MSN IE",
  L"ie.msn.com",
  "http://search.msn.ie/s/wlflag.ico",
  L"http://search.msn.ie/results.aspx?mkt=en-IE&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_IN = {
  L"MSN India",
  L"in.msn.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=en-IN&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_MY = {
  L"MSN Malaysia",
  L"malaysia.msn.com",
  "http://search.msn.com.my/s/wlflag.ico",
  L"http://search.msn.com.my/results.aspx?mkt=en-MY&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_PH = {
  L"MSN Philippines",
  L"ph.msn.com",
  "http://search.msn.com.ph/s/wlflag.ico",
  L"http://search.msn.com.ph/results.aspx?mkt=en-PH&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_SG = {
  L"MSN Singapore",
  L"sg.msn.com",
  "http://search.msn.com.sg/s/wlflag.ico",
  L"http://search.msn.com.sg/results.aspx?mkt=en-SG&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_XA = {
  L"MSN (English)",
  L"arabia.msn.com",
  "http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?setlang=en-XA&mkt=en-XA&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_ZA = {
  L"MSN ZA",
  L"za.msn.com",
  "http://search.msn.co.za/s/wlflag.ico",
  L"http://search.msn.co.za/results.aspx?mkt=en-ZA&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_AR = {
  L"MSN Argentina",
  L"ar.msn.com",
  "http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?mkt=es-AR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_CL = {
  L"MSN Chile",
  L"cl.msn.com",
  "http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?mkt=es-CL&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_CO = {
  L"MSN Colombia",
  L"co.msn.com",
  "http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?mkt=es-CO&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_ES = {
  L"MSN Espa" L"\x00f1" L"a",
  L"es.msn.com",
  "http://search.msn.es/s/wlflag.ico",
  L"http://search.msn.es/results.aspx?mkt=es-ES&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_MX = {
  L"Prodigy / MSN",
  L"prodigy.msn.com",
  "http://search.prodigy.msn.com/s/wlflag.ico",
  L"http://search.prodigy.msn.com/results.aspx?mkt=es-MX&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_XL = {
  L"MSN Latinoam\x00e9rica",
  L"latam.msn.com",
  "http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?mkt=es-XL&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_fi_FI = {
  L"MSN",
  L"fi.msn.com",
  "http://search.msn.fi/s/wlflag.ico",
  L"http://search.msn.fi/results.aspx?mkt=fi-FI&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_fr_BE = {
  L"MSN Belgique (Fran" L"\x00e7" L"ais)",
  L"",  // "be.msn.com" is already taken by msn_nl_BE (see comment on ID below).
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=fr-BE&mkt=fr-BE&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  8,  // Can't be 3 as this has to appear in the Belgium list alongside
      // msn_nl_BE.
};

const PrepopulatedEngine msn_fr_CA = {
  L"Sympatico / MSN (Fran" L"\x00e7" L"ais)",
  L"",  // "sympatico.msn.ca" is already taken by msn_en_CA (see comment on ID
        // below).
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=fr-CA&mkt=fr-CA&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  9,  // Can't be 3 as this has to appear in the Canada list alongside
      // msn_en_CA.
};

const PrepopulatedEngine msn_fr_CH = {
  L"MSN Suisse (Fran" L"\x00e7" L"ais)",
  L"",  // "ch.msn.com" is already taken by msn_de_CH (see comment on ID below).
  "http://search.msn.ch/s/wlflag.ico",
  L"http://search.msn.ch/results.aspx?setlang=fr-CH&mkt=fr-CH&q={searchTerms}",
  "UTF-8",
  NULL,
  10,  // Can't be 3 as this has to appear in the Switzerland list alongside
       // msn_de_CH.
};

const PrepopulatedEngine msn_fr_FR = {
  L"MSN France",
  L"fr.msn.com",
  "http://search.msn.fr/s/wlflag.ico",
  L"http://search.msn.fr/results.aspx?mkt=fr-FR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_he_IL = {
  L"msn.co.il",
  L"msn.co.il",
  "http://msn.co.il/favicon.ico",
  L"http://search.msn.co.il/Search.aspx?q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_ja_JP = {
  L"MSN Japan",
  L"jp.msn.com",
  "http://search.msn.co.jp/s/wlflag.ico",
  L"http://search.msn.co.jp/results.aspx?mkt=ja-JP&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_nb_NO = {
  L"MSN Norge",
  L"no.msn.com",
  "http://search.msn.no/s/wlflag.ico",
  L"http://search.msn.no/results.aspx?mkt=nb-NO&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_nl_BE = {
  L"MSN (Nederlandstalige)",
  L"be.msn.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=nl-BE&mkt=nl-BE&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_nl_NL = {
  L"MSN.nl",
  L"nl.msn.com",
  "http://search.msn.nl/s/wlflag.ico",
  L"http://search.msn.nl/results.aspx?mkt=nl-NL&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_pt_BR = {
  L"MSN Brasil",
  L"br.msn.com",
  "http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=pt-BR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_sv_SE = {
  L"MSN",
  L"se.msn.com",
  "http://search.msn.se/s/wlflag.ico",
  L"http://search.msn.se/results.aspx?mkt=pv-SE&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_tr_TR = {
  L"MSN T\x00fckiye'ye",
  L"tr.msn.com",
  "http://search.msn.com.tr/s/wlflag.ico",
  L"http://search.msn.com.tr/results.aspx?mkt=tr-TR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_zh_HK = {
  L"MSN Hong Kong",
  L"hk.msn.com",
  "http://search.msn.com.hk/s/wlflag.ico",
  L"http://search.msn.com.hk/results.aspx?mkt=zh-HK&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine mweb = {
  L"MWEB",
  L"mweb.co.za",
  "http://mweb.co.za/favicon.ico",
  L"http://search.mweb.co.za/search?&q={searchTerms}",
  "UTF-8",
  NULL,
  42,
};

const PrepopulatedEngine mynet = {
  L"MYNET",
  L"mynet.com",
  "http://img.mynet.com/mynetfavori.ico",
  L"http://arama.mynet.com/search.aspx?q={searchTerms}&pg=q",
  "windows-1254",
  NULL,
  101,
};

const PrepopulatedEngine mywebsearch = {
  L"mywebsearch",
  L"mywebsearch.com",
  NULL,
  L"http://search.mywebsearch.com/mywebsearch/AJmain.jhtml?"
      L"searchfor={searchTerms}",
  "UTF-8",
  NULL,
  97,
};

const PrepopulatedEngine najdi = {
  L"Najdi.si",
  L"najdi.si",
  "http://www.najdi.si/master/favicon.ico",
  L"http://www.najdi.si/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  87,
};

const PrepopulatedEngine nana10 = {
  L"\x05e0\x05e2\x05e0\x05e2 10",
  L"nana10.co.il",
  "http://f.nau.co.il/Common/Includes/favicon.ico",
  L"http://index.nana10.co.il/search.asp?q={searchTerms}",
  "windows-1255",
  NULL,
  56,
};

const PrepopulatedEngine nate = {
  L"\xb124\xc774\xd2b8\xb2f7\xcef4",
  L"nate.com",
  "http://nate.search.empas.com/favicon.ico",
  L"http://nate.search.empas.com/search/all.html?q={searchTerms}",
  "EUC-KR",
  NULL,
  69,
};

const PrepopulatedEngine naver = {
  L"\xb124\xc774\xbc84",
  L"naver.com",
  "http://search.naver.com/favicon.ico",
  L"http://search.naver.com/search.naver?ie={inputEncoding}"
      L"&query={searchTerms}",
  "UTF-8",
  L"http://ac.search.naver.com/autocompl?m=s&ie={inputEncoding}&oe=utf-8&"
      L"q={searchTerms}",
  67,
};

const PrepopulatedEngine neti = {
  L"NETI",
  L"neti.ee",
  "http://www.neti.ee/favicon.ico",
  L"http://www.neti.ee/cgi-bin/otsing?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  44,
};

const PrepopulatedEngine netindex = {
  L"NetINDEX",
  L"netindex.pt",
  "http://www.netindex.pt/favicon.ico",
  L"http://www.netindex.pt/cgi-bin/index.cgi?question={searchTerms}",
  "ISO-8859-1",
  NULL,
  78,
};

const PrepopulatedEngine nifty = {
  L"@nifty",
  L"nifty.com",
  "http://www.nifty.com/favicon.ico",
  L"http://search.nifty.com/cgi-bin/search.cgi?Text={searchTerms}",
  "Shift_JIS",
  NULL,
  65,
};

const PrepopulatedEngine ohperu = {
  L"Oh Per\x00fa",
  L"ohperu.com",
  NULL,
  L"http://www.google.com.pe/custom?q={searchTerms}&"
      L"client=pub-1950414869696311&ie={inputEncoding}&cof=GALT%3A%23000000"
      L"%3BGL%3A1%3BDIV%3A%23FFFFFF%3BVLC%3A000000%3BAH%3Acenter%3BBGC%3AFFFFFF"
      L"%3BLBGC%3AFFFFFF%3BALC%3A000000%3BLC%3A000000%3BT%3A000000%3BGFNT"
      L"%3A000000%3BGIMP%3A000000%3BLH%3A50%3BLW%3A142%3BL%3Ahttp%3A%2F%2F"
      L"www.ohperu.com%2Fohperu-logo-inv2.gif%3BS%3Ahttp%3A%2F%2Fwww.ohperu.com"
      L"%3BFORID%3A1",
  "ISO-8859-1",
  NULL,
  96,
};

const PrepopulatedEngine ok = {
  L"OK.hu",
  L"ok.hu",
  "http://ok.hu/gfx/favicon.ico",
  L"http://ok.hu/katalogus?q={searchTerms}",
  "ISO-8859-2",
  NULL,
  6,
};

const PrepopulatedEngine onet = {
  L"Onet.pl",
  L"onet.pl",
  "http://szukaj.onet.pl/favicon.ico",
  L"http://szukaj.onet.pl/query.html?qt={searchTerms}",
  "ISO-8859-2",
  NULL,
  75,
};

const PrepopulatedEngine orange = {
  L"Orange",
  L"orange.fr",
  "http://www.orange.fr/favicon.ico",
  L"http://rws.search.ke.voila.fr/RW/S/opensearch_orange?rdata={searchTerms}",
  "ISO-8859-1",
  L"http://search.ke.voila.fr/fr/cmplopensearch/xml/fullxml?"
  L"rdata={searchTerms}",
  48,
};

const PrepopulatedEngine ozu = {
  L"OZ\x00da",
  L"ozu.es",
  "http://www.ozu.es/favicon.ico",
  L"http://buscar.ozu.es/index.php?q={searchTerms}",
  "ISO-8859-1",
  NULL,
  98,
};

const PrepopulatedEngine pogodak_ba = {
  L"Pogodak!",
  L"pogodak.ba",
  "http://www.pogodak.ba/favicon.ico",
  L"http://www.pogodak.ba/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  24,
};

const PrepopulatedEngine pogodak_hr = {
  L"Pogodak!",
  L"pogodak.hr",
  "http://www.pogodak.hr/favicon.ico",
  L"http://www.pogodak.hr/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  24,
};

const PrepopulatedEngine pogodak_rs = {
  L"Pogodak!",
  L"pogodak.rs",
  "http://www.pogodak.rs/favicon.ico",
  L"http://www.pogodak.rs/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  24,
};

const PrepopulatedEngine pogodok = {
  L"\x041f\x043e\x0433\x043e\x0434\x043e\x043a!",
  L"pogodok.com.mk",
  "http://www.pogodok.com.mk/favicon.ico",
  L"http://www.pogodok.com.mk/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  24,  // Really the same engine as Pogodak, just has a small name change.
};

const PrepopulatedEngine rambler = {
  L"Rambler",
  L"rambler.ru",
  "http://www.rambler.ru/favicon.ico",
  L"http://www.rambler.ru/srch?words={searchTerms}",
  "windows-1251",
  NULL,
  16,
};

const PrepopulatedEngine rediff = {
  L"Rediff",
  L"rediff.com",
  "http://search1.rediff.com/favicon.ico",
  L"http://search1.rediff.com/dirsrch/default.asp?MT={searchTerms}",
  "UTF-8",
  NULL,
  37,
};

const PrepopulatedEngine rednano = {
  L"Rednano",
  L"rednano.sg",
  "http://rednano.sg/favicon.ico",
  L"http://rednano.sg/sfe/lwi.action?querystring={searchTerms}",
  "UTF-8",
  NULL,
  41,
};

const PrepopulatedEngine sanook = {
  L"\x0e2a\x0e19\x0e38\x0e01!",
  L"sanook.com",
  "http://search.sanook.com/favicon.ico",
  L"http://search.sanook.com/search.php?q={searchTerms}",
  "UTF-8",
  NULL,
  100,
};

const PrepopulatedEngine sapo = {
  L"SAPO",
  L"sapo.pt",
  "http://imgs.sapo.pt/images/sapo.ico",
  L"http://pesquisa.sapo.pt/?q={searchTerms}",
  "UTF-8",
  L"http://pesquisa.sapo.pt/livesapo?q={searchTerms}",
  77,
};

const PrepopulatedEngine search_ch = {
  L"search.ch",
  L"search.ch",
  "http://www.search.ch/favicon.ico",
  L"http://www.search.ch/?q={searchTerms}",
  "ISO-8859-1",
  NULL,
  51,
};

const PrepopulatedEngine sensis = {
  L"sensis.com.au",
  L"sensis.com.au",
  "http://www.sensis.com.au/favicon.ico",
  L"http://www.sensis.com.au/search.do?find={searchTerms}",
  "UTF-8",
  NULL,
  32,
};

const PrepopulatedEngine sesam = {
  L"Sesam",
  L"sesam.no",
  "http://sesam.no/images/favicon.gif",
  L"http://sesam.no/search/?q={searchTerms}",
  "UTF-8",
  NULL,
  74,
};

const PrepopulatedEngine seznam = {
  L"Seznam",
  L"seznam.cz",
  "http://1.im.cz/szn/img/favicon.ico",
  L"http://search.seznam.cz/?q={searchTerms}",
  "UTF-8",
  L"http:///suggest.fulltext.seznam.cz/?dict=fulltext_ff&phrase={searchTerms}&"
      L"encoding={inputEncoding}&response_encoding=utf-8",
  25,
};

const PrepopulatedEngine sogou = {
  L"\x641c\x72d7",
  L"sogou.com",
  "http://www.sogou.com/favicon.ico",
  L"http://www.sogou.com/web?query={searchTerms}",
  "GB2312",
  NULL,
  20,
};

const PrepopulatedEngine soso = {
  L"\x641c\x641c",
  L"soso.com",
  "http://www.soso.com/favicon.ico",
  L"http://www.soso.com/q?w={searchTerms}",
  "GB2312",
  NULL,
  22,
};

const PrepopulatedEngine spray = {
  L"Spray",
  L"spray.se",
  "http://www.eniro.se/favicon.ico",
  L"http://www.eniro.se/query?ax=spray&search_word={searchTerms}&what=web",
  "ISO-8859-1",
  NULL,
  99,
};

const PrepopulatedEngine szm = {
  L"SZM.sk",
  L"szm.sk",
  "http://szm.sk/favicon.ico",
  L"http://szm.sk/search/?co=1&q={searchTerms}",
  "windows-1250",
  NULL,
  86,
};

const PrepopulatedEngine t_online = {
  L"T-Online",
  L"suche.t-online.de",
  "http://suche.t-online.de/favicon.ico",
  L"http://suche.t-online.de/fast-cgi/tsc?sr=chrome&q={searchTerms}",
  "UTF-8",
  NULL,
  49,
};

const PrepopulatedEngine tango = {
  L"Tango",
  L"tango.hu",
  "http://tango.hu/favicon.ico",
  L"http://tango.hu/search.php?q={searchTerms}",
  "windows-1250",
  NULL,
  58,
};

const PrepopulatedEngine tapuz = {
  L"\x05ea\x05e4\x05d5\x05d6 \x05d0\x05e0\x05e9\x05d9\x05dd",
  L"tapuz.co.il",
  "http://www.tapuz.co.il/favicon.ico",
  L"http://www.tapuz.co.il/search/search.asp?q={searchTerms}",
  "UTF-8",
  NULL,
  57,
};

const PrepopulatedEngine terra_ar = {
  L"Terra Argentina",
  L"terra.com.ar",
  "http://buscar.terra.com.ar/favicon.ico",
  L"http://buscar.terra.com.ar/Default.aspx?query={searchTerms}&source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine terra_ec = {
  L"Terra Ecuador",
  L"terra.com.ec",
  "http://buscador.terra.com.ec/favicon.ico",
  L"http://buscador.terra.com.ec/Default.aspx?query={searchTerms}&"
      L"source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine terra_es = {
  L"Terra",
  L"terra.es",
  "http://buscador.terra.es/favicon.ico",
  L"http://buscador.terra.es/Default.aspx?query={searchTerms}&source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine terra_mx = {
  L"Terra",
  L"terra.com.mx",
  "http://buscador.terra.com.mx/favicon.ico",
  L"http://buscador.terra.com.mx/Default.aspx?query={searchTerms}&"
      L"source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine terra_pe = {
  L"Terra",
  L"terra.com.pe",
  "http://buscador.terra.com.pe/favicon.ico",
  L"http://buscador.terra.com.pe/Default.aspx?query={searchTerms}&"
      L"source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine toile = {
  L"La Toile du Qu" L"\x00e9" L"bec",
  L"toile.com",
  "http://static.search.canoe.ca/s-toile/img/favicon_toile.ico",
  L"http://www.toile.com/search?q={searchTerms}",
  "UTF-8",
  NULL,
  36,
};

const PrepopulatedEngine tut = {
  L"TUT.BY",
  L"tut.by",
  "http://www.tut.by/favicon.ico",
  L"http://search.tut.by/?query={searchTerms}",
  "windows-1251",
  NULL,
  17,
};

const PrepopulatedEngine uol = {
  L"UOL Busca",
  L"busca.uol.com.br",
  "http://busca.uol.com.br/favicon.ico",
  L"http://busca.uol.com.br/www/index.html?q={searchTerms}",
  "ISO-8859-1",
  NULL,
  82,
};

const PrepopulatedEngine vinden = {
  L"Vinden.nl",
  L"vinden.nl",
  "http://www.vinden.nl/favicon.ico",
  L"http://www.vinden.nl/?q={searchTerms}",
  "UTF-8",
  NULL,
  31,
};

const PrepopulatedEngine virgilio = {
  L"Virgilio",
  L"virgilio.alice.it",
  "http://ricerca.alice.it/favicon.ico",
  L"http://ricerca.alice.it/ricerca?qs={searchTerms}",
  "ISO-8859-1",
  NULL,
  62,
};

const PrepopulatedEngine voila = {
  L"Voila",
  L"voila.fr",
  "http://search.ke.voila.fr/favicon.ico",
  L"http://rws.search.ke.voila.fr/RW/S/opensearch_voila?rdata={searchTerms}",
  "ISO-8859-1",
  L"http://search.ke.voila.fr/fr/cmplopensearch/xml/fullxml?"
  L"rdata={searchTerms}",
  47,
};

const PrepopulatedEngine walla = {
  L"\x05d5\x05d5\x05d0\x05dc\x05d4!",
  L"walla.co.il",
  "http://www.walla.co.il/favicon.ico",
  L"http://search.walla.co.il/?e=hew&q={searchTerms}",
  "windows-1255",
  NULL,
  55,
};

const PrepopulatedEngine web_de = {
  L"WEB.DE",
  L"web.de",
  "http://img.ui-portal.de/search/img/webde/favicon.ico",
  L"http://suche.web.de/search/web/?su={searchTerms}",
  "ISO-8859-1",
  NULL,
  50,
};

const PrepopulatedEngine wp = {
  L"Wirtualna Polska",
  L"wp.pl",
  "http://szukaj.wp.pl/favicon.ico",
  L"http://szukaj.wp.pl/szukaj.html?szukaj={searchTerms}",
  "ISO-8859-2",
  NULL,
  76,
};

const PrepopulatedEngine yagua = {
  L"Yagua.com",
  L"yagua.com",
  "http://yagua.paraguay.com/favicon.ico",
  L"http://yagua.paraguay.com/buscador.php?q={searchTerms}&cs={inputEncoding}",
  "ISO-8859-1",
  NULL,
  94,
};

const PrepopulatedEngine yahoo = {
  L"Yahoo!",
  L"yahoo.com",
  "http://search.yahoo.com/favicon.ico",
  L"http://search.yahoo.com/search?ei={inputEncoding}&fr=crmas&p={searchTerms}",
  "UTF-8",
  L"http://ff.search.yahoo.com/gossip?output=fxjson&command={searchTerms}",
  2,
};

// For regional Yahoo variants without region-specific suggestion service,
// suggestion is disabled. For some of them, we might consider
// using a fallback (e.g. de for at/ch, ca or fr for qc, en for nl, no, hk).
const PrepopulatedEngine yahoo_ar = {
  L"Yahoo! Argentina",
  L"ar.yahoo.com",
  "http://ar.search.yahoo.com/favicon.ico",
  L"http://ar.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://ar-sayt.ff.search.yahoo.com/gossip-ar-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_at = {
  L"Yahoo! Suche",
  L"at.yahoo.com",
  "http://at.search.yahoo.com/favicon.ico",
  L"http://at.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_au = {
  L"Yahoo!7",
  L"au.yahoo.com",
  "http://au.search.yahoo.com/favicon.ico",
  L"http://au.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://aue-sayt.ff.search.yahoo.com/gossip-au-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_br = {
  L"Yahoo! Brasil",
  L"br.yahoo.com",
  "http://br.search.yahoo.com/favicon.ico",
  L"http://br.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://br-sayt.ff.search.yahoo.com/gossip-br-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_ca = {
  L"Yahoo! Canada",
  L"ca.yahoo.com",
  "http://ca.search.yahoo.com/favicon.ico",
  L"http://ca.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://gossip.ca.yahoo.com/gossip-ca-sayt?output=fxjsonp&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_ch = {
  L"Yahoo! Suche",
  L"ch.yahoo.com",
  "http://ch.search.yahoo.com/favicon.ico",
  L"http://ch.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_cl = {
  L"Yahoo! Chile",
  L"cl.yahoo.com",
  "http://cl.search.yahoo.com/favicon.ico",
  L"http://cl.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://gossip.telemundo.yahoo.com/gossip-e1-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_cn = {
  L"\x4e2d\x56fd\x96c5\x864e",
  L"cn.yahoo.com",
  "http://search.cn.yahoo.com/favicon.ico",
  L"http://search.cn.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "GB2312",
  // http://cn.yahoo.com/cnsuggestion/suggestion.inc.php?of=fxjson&query=
  // returns in a proprietary format ('|' delimeted word list).
  NULL,
  2,
};

const PrepopulatedEngine yahoo_co = {
  L"Yahoo! Colombia",
  L"co.yahoo.com",
  "http://co.search.yahoo.com/favicon.ico",
  L"http://co.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://gossip.telemundo.yahoo.com/gossip-e1-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_de = {
  L"Yahoo! Deutschland",
  L"de.yahoo.com",
  "http://de.search.yahoo.com/favicon.ico",
  L"http://de.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://de-sayt.ff.search.yahoo.com/gossip-de-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_dk = {
  L"Yahoo! Danmark",
  L"dk.yahoo.com",
  "http://dk.search.yahoo.com/favicon.ico",
  L"http://dk.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_es = {
  L"Yahoo! Espa" L"\x00f1" L"a",
  L"es.yahoo.com",
  "http://es.search.yahoo.com/favicon.ico",
  L"http://es.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://es-sayt.ff.search.yahoo.com/gossip-es-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_fi = {
  L"Yahoo!-haku",
  L"fi.yahoo.com",
  "http://fi.search.yahoo.com/favicon.ico",
  L"http://fi.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_fr = {
  L"Yahoo! France",
  L"fr.yahoo.com",
  "http://fr.search.yahoo.com/favicon.ico",
  L"http://fr.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://fr-sayt.ff.search.yahoo.com/gossip-fr-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_hk = {
  L"Yahoo! Hong Kong",
  L"hk.yahoo.com",
  "http://hk.search.yahoo.com/favicon.ico",
  L"http://hk.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  // http://history.hk.search.yahoo.com/ac/ac_msearch.php?query={searchTerms}
  // returns a JSON with key-value pairs. Setting parameters (ot, of, output)
  // to fxjson, json, or js doesn't help. 
  NULL,
  2,
};

const PrepopulatedEngine yahoo_id = {
  L"Yahoo! Indonesia",
  L"id.yahoo.com",
  "http://id.search.yahoo.com/favicon.ico",
  L"http://id.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://id-sayt.ff.search.yahoo.com/gossip-id-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_in = {
  L"Yahoo! India",
  L"in.yahoo.com",
  "http://in.search.yahoo.com/favicon.ico",
  L"http://in.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://in-sayt.ff.search.yahoo.com/gossip-in-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_it = {
  L"Yahoo! Italia",
  L"it.yahoo.com",
  "http://it.search.yahoo.com/favicon.ico",
  L"http://it.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://it-sayt.ff.search.yahoo.com/gossip-it-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_jp = {
  L"Yahoo! JAPAN",
  L"yahoo.co.jp",
  "http://search.yahoo.co.jp/favicon.ico",
  L"http://search.yahoo.co.jp/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_kr = {
  L"\xc57c\xd6c4! \xcf54\xb9ac\xc544",
  L"kr.yahoo.com",
  "http://kr.search.yahoo.com/favicon.ico",
  L"http://kr.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://kr.atc.search.yahoo.com/atcx.php?property=main&ot=fxjson&"
     L"ei=utf8&eo=utf8&command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_malaysia = {
  L"Yahoo! Malaysia",
  L"malaysia.yahoo.com",
  "http://malaysia.search.yahoo.com/favicon.ico",
  L"http://malaysia.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://my-sayt.ff.search.yahoo.com/gossip-my-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_mx = {
  L"Yahoo! M\x00e9xico",
  L"mx.yahoo.com",
  "http://mx.search.yahoo.com/favicon.ico",
  L"http://mx.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://gossip.mx.yahoo.com/gossip-mx-sayt?output=fxjsonp&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_nl = {
  L"Yahoo! Nederland",
  L"nl.yahoo.com",
  "http://nl.search.yahoo.com/favicon.ico",
  L"http://nl.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_no = {
  L"Yahoo! Norge",
  L"no.yahoo.com",
  "http://no.search.yahoo.com/favicon.ico",
  L"http://no.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_nz = {
  L"Yahoo!Xtra",
  L"nz.yahoo.com",
  "http://nz.search.yahoo.com/favicon.ico",
  L"http://nz.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://aue-sayt.ff.search.yahoo.com/gossip-nz-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_pe = {
  L"Yahoo! Per\x00fa",
  L"pe.yahoo.com",
  "http://pe.search.yahoo.com/favicon.ico",
  L"http://pe.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://gossip.telemundo.yahoo.com/gossip-e1-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_ph = {
  L"Yahoo! Philippines",
  L"ph.yahoo.com",
  "http://ph.search.yahoo.com/favicon.ico",
  L"http://ph.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://ph-sayt.ff.search.yahoo.com/gossip-ph-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_qc = {
  L"Yahoo! Qu" L"\x00e9" L"bec",
  L"qc.yahoo.com",
  "http://qc.search.yahoo.com/favicon.ico",
  L"http://qc.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  5,  // Can't be 2 as this has to appear in the Canada list alongside yahoo_ca.
};

const PrepopulatedEngine yahoo_ru = {
  L"Yahoo! \x043f\x043e-\x0440\x0443\x0441\x0441\x043a\x0438",
  L"ru.yahoo.com",
  "http://ru.search.yahoo.com/favicon.ico",
  L"http://ru.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_sg = {
  L"Yahoo! Singapore",
  L"sg.yahoo.com",
  "http://sg.search.yahoo.com/favicon.ico",
  L"http://sg.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://sg-sayt.ff.search.yahoo.com/gossip-sg-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_th = {
  L"Yahoo! \x0e1b\x0e23\x0e30\x0e40\x0e17\x0e28\x0e44\x0e17\x0e22",
  L"th.yahoo.com",
  "http://th.search.yahoo.com/favicon.ico",
  L"http://th.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://th-sayt.ff.search.yahoo.com/gossip-th-sayt?output=fxjson&"
    L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_tw = {
  L"Yahoo!\x5947\x6469",
  L"tw.yahoo.com",
  "http://tw.search.yahoo.com/favicon.ico",
  L"http://tw.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  // "http://tw.yahoo.com/ac/ac_search.php?eo=utf8&of=js&prop=web&query="
  // returns a JSON file prepended with 'fxjson={'.
  NULL,
  2,
};

const PrepopulatedEngine yahoo_uk = {
  L"Yahoo! UK & Ireland",
  L"uk.yahoo.com",
  "http://uk.search.yahoo.com/favicon.ico",
  L"http://uk.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://uk-sayt.ff.search.yahoo.com/gossip-uk-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_ve = {
  L"Yahoo! Venezuela",
  L"ve.yahoo.com",
  "http://ve.search.yahoo.com/favicon.ico",
  L"http://ve.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://gossip.telemundo.yahoo.com/gossip-e1-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yahoo_vn = {
  L"Yahoo! Vi\x1ec7t Nam",
  L"vn.yahoo.com",
  "http://vn.search.yahoo.com/favicon.ico",
  L"http://vn.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  L"http://vn-sayt.ff.search.yahoo.com/gossip-vn-sayt?output=fxjson&"
      L"command={searchTerms}",
  2,
};

const PrepopulatedEngine yam = {
  L"\u5929\u7a7a",
  L"yam.com",
  "http://www.yam.com/i/8/sky.ico",
  L"http://search.yam.com/wps?k={searchTerms}",
  "Big5",
  NULL,
  23,
};

const PrepopulatedEngine yamli = {
  L"Yamli",
  L"yamli.com",
  "http://www.yamli.com/favicon.ico",
  L"http://www.yamli.com/#q={searchTerms}",
  "UTF-8",
  NULL,
  11,
};

const PrepopulatedEngine yandex_ru = {
  L"\x042f\x043d\x0434\x0435\x043a\x0441",
  L"yandex.ru",
  "http://yandex.ru/favicon.ico",
  L"http://yandex.ru/yandsearch?text={searchTerms}",
  "UTF-8",
  L"http://suggest.yandex.net/suggest-ff.cgi?part={searchTerms}",
  15,
};

const PrepopulatedEngine yandex_ua = {
  L"\x042f\x043d\x0434\x0435\x043a\x0441",
  L"yandex.ua",
  "http://yandex.ua/favicon.ico",
  L"http://yandex.ua/yandsearch?text={searchTerms}",
  "UTF-8",
  L"http://suggest.yandex.net/suggest-ff.cgi?part={searchTerms}",
  15,
};

const PrepopulatedEngine zoznam = {
  L"Zoznam",
  L"zoznam.sk",
  "http://zoznam.sk/favicon.ico",
  L"http://zoznam.sk/hladaj.fcgi?s={searchTerms}",
  "windows-1250",
  NULL,
  85,
};

// Lists of engines per country ////////////////////////////////////////////////

// Put these in order with most interesting/important first.  The default will
// be the first engine.

// Default (for countries with no better engine set)
const PrepopulatedEngine* engines_default[] = { &google, &yahoo, &live, };

// United Arab Emirates
const PrepopulatedEngine* engines_AE[] =
    { &google, &maktoob, &yahoo, &yamli, &araby, &msn_en_XA, &msn_ar_XA, };

// Albania
const PrepopulatedEngine* engines_AL[] =
    { &google, &yahoo, &live_en_XA, &live_ar_XA, };

// Argentina
const PrepopulatedEngine* engines_AR[] =
    { &google, &msn_es_AR, &altavista_ar, &terra_ar, &yahoo_ar, };

// Austria
const PrepopulatedEngine* engines_AT[] = { &google, &yahoo_at, &msn_de_AT, };

// Australia
const PrepopulatedEngine* engines_AU[] =
    { &google, &yahoo_au, &msn_en_AU, &sensis, };

// Bosnia and Herzegovina
const PrepopulatedEngine* engines_BA[] =
    { &google, &pogodak_ba, &yahoo, &live, };

// Belgium
const PrepopulatedEngine* engines_BE[] =
    { &google, &yahoo, &msn_nl_BE, &msn_fr_BE, };

// Bulgaria
// The commented-out entry for "dir" below is for dir.bg, &which we don't
// currently support because it uses POST instead of GET for its searches.
// See http://b/1196285
const PrepopulatedEngine* engines_BG[] =
    { &google, &/*dir,*/ yahoo, &jabse, &live_bg_BG, };

// Bahrain
const PrepopulatedEngine* engines_BH[] =
    { &google, &maktoob, &yamli, &yahoo, &araby, &msn_en_XA, &msn_ar_XA, };

// Brunei
const PrepopulatedEngine* engines_BN[] =
    { &google, &yahoo_malaysia, &msn_en_MY, };

// Bolivia
const PrepopulatedEngine* engines_BO[] =
    { &google, &altavista, &msn_es_XL, &yahoo, &ask_es, };

// Brazil
const PrepopulatedEngine* engines_BR[] =
    { &google, &msn_pt_BR, &yahoo_br, &aonde, &gigabusca, &uol, };

// Belarus
const PrepopulatedEngine* engines_BY[] =
    { &google, &yandex_ru, &rambler, &yahoo, &tut, };

// Belize
const PrepopulatedEngine* engines_BZ[] = { &google, &yahoo, &live, &aol, };

// Canada
const PrepopulatedEngine* engines_CA[] =
    { &google, &msn_en_CA, &msn_fr_CA, &yahoo_ca, &yahoo_qc, &toile, };

// Switzerland
const PrepopulatedEngine* engines_CH[] =
    { &google, &search_ch, &yahoo_ch, &msn_de_CH, &msn_fr_CH, &bluewin, };

// Chile
const PrepopulatedEngine* engines_CL[] =
    { &google, &yahoo_cl, &altavista, &msn_es_CL, };

// China
const PrepopulatedEngine* engines_CN[] =
    { &google, &baidu, &yahoo_cn, &sogou, &soso, };

// Colombia
const PrepopulatedEngine* engines_CO[] =
    { &google, &msn_es_CO, &ask_es, &altavista, &conexcol, &yahoo_co, };

// Costa Rica
const PrepopulatedEngine* engines_CR[] =
    { &google, &msn_es_XL, &yahoo, &altavista, &aol, &lycos_es, };

// Czech Republic
const PrepopulatedEngine* engines_CZ[] =
    { &google, &seznam, &centrum_cz, &atlas_cz, &live_cs_CZ, };

// Germany
const PrepopulatedEngine* engines_DE[] =
    { &google, &msn_de_DE, &yahoo_de, &t_online, &ask_de, &web_de, };

// Denmark
const PrepopulatedEngine* engines_DK[] =
    { &google, &jubii, &msn_da_DK, &yahoo_dk, &eniro_dk, };

// Dominican Republic
const PrepopulatedEngine* engines_DO[] =
    { &google, &msn_es_XL, &yahoo, &altavista, &go, &aol, };

// Algeria
const PrepopulatedEngine* engines_DZ[] =
    { &google, &yahoo, &yamli, &msn_en_XA, &msn_ar_XA, &araby, };

// Ecuador
const PrepopulatedEngine* engines_EC[] =
    { &google, &msn_es_XL, &yahoo, &terra_ec, };

// Estonia
const PrepopulatedEngine* engines_EE[] =
    { &google, &neti, &delfi_ee, &yahoo, &live_et_EE, };

// Egypt
const PrepopulatedEngine* engines_EG[] =
    { &google, &masrawy, &yahoo, &maktoob, &araby, &msn_en_XA, &msn_ar_XA, };

// Spain
const PrepopulatedEngine* engines_ES[] =
    { &google, &msn_es_ES, &yahoo_es, &terra_es, &ozu, &altavista_es, };

// Faroe Islands
const PrepopulatedEngine* engines_FO[] =
    { &google, &jubii, &msn_da_DK, &yahoo_dk, &eniro_dk, };

// Finland
const PrepopulatedEngine* engines_FI[] =
    { &google, &msn_fi_FI, &yahoo_fi, &eniro_fi, &fonecta_02_fi, };

// France
const PrepopulatedEngine* engines_FR[] =
    { &google, &voila, &yahoo_fr, &msn_fr_FR, &orange, &aol_fr, };

// United Kingdom
const PrepopulatedEngine* engines_GB[] =
    { &google, &yahoo_uk, &msn_en_GB, &ask_uk, };

// Greece
const PrepopulatedEngine* engines_GR[] =
    { &google, &yahoo, &forthnet, &in, &live_el_GR };

// Guatemala
const PrepopulatedEngine* engines_GT[] =
    { &google, &msn_es_XL, &yahoo, &ask_es, &altavista, &go, };

// Hong Kong
const PrepopulatedEngine* engines_HK[] =
    { &google, &yahoo_hk, &msn_zh_HK, &sogou, &baidu, };

// Honduras
const PrepopulatedEngine* engines_HN[] =
    { &google, &msn_es_XL, &yahoo, &ask_es, &altavista, };

// Croatia
const PrepopulatedEngine* engines_HR[] =
    { &google, &yahoo, &pogodak_hr, &live_hr_HR, };

// Hungary
const PrepopulatedEngine* engines_HU[] = { &google, &tango, &ok, &live_hu_HU, };

// Indonesia
const PrepopulatedEngine* engines_ID[] = { &google, &yahoo_id, &live_en_ID, };

// Ireland
const PrepopulatedEngine* engines_IE[] = { &google, &yahoo_uk, &msn_en_IE, };

// Israel
const PrepopulatedEngine* engines_IL[] =
    { &google, &walla, &nana10, &tapuz, &msn_he_IL, };

// India
const PrepopulatedEngine* engines_IN[] =
    { &google, &yahoo_in, &msn_en_IN, &rediff, &guruji, };

// Iraq
const PrepopulatedEngine* engines_IQ[] =
    { &google, &maktoob, &yamli, &yahoo, &araby, &msn_en_XA, &msn_ar_XA, };

// Iran
const PrepopulatedEngine* engines_IR[] = { &google, };

// Iceland
const PrepopulatedEngine* engines_IS[] = { &google, &leit, &embla, &finna, };

// Italy
const PrepopulatedEngine* engines_IT[] =
    { &google, &virgilio, &yahoo_it, &libero, &ask_it, &live_it_IT, };

// Jamaica
const PrepopulatedEngine* engines_JM[] =
    { &google, &jamaicalive, &yahoo, &live, &go, &aol, };

// Jordan
const PrepopulatedEngine* engines_JO[] =
    { &google, &maktoob, &yamli, &yahoo, &araby, &msn_en_XA, &msn_ar_XA, };

// Japan
const PrepopulatedEngine* engines_JP[] =
    { &google, &yahoo_jp, &msn_ja_JP, &biglobe, &goo, &nifty, };

// Kenya
const PrepopulatedEngine* engines_KE[] = { &google, &yahoo, &msn, };

// Kuwait
const PrepopulatedEngine* engines_KW[] =
    { &google, &maktoob, &yahoo, &yamli, &araby, &msn_en_XA, &msn_ar_XA, };

// South Korea
const PrepopulatedEngine* engines_KR[] =
    { &google, &naver, &daum, &yahoo_kr, &nate, &empas, };

// Lebanon
const PrepopulatedEngine* engines_LB[] =
    { &google, &maktoob, &yahoo, &yamli, &araby, &msn_en_XA, &msn_ar_XA, };

// Liechtenstein
const PrepopulatedEngine* engines_LI[] =
    { &google, &msn_de_DE, &yahoo_de, &t_online, &ask_de, &web_de, };

// Lithuania
const PrepopulatedEngine* engines_LT[] =
    { &google, &delfi_lt, &yahoo, &yandex_ru, &live_lt_LT, };

// Luxembourg
const PrepopulatedEngine* engines_LU[] =
    { &google, &voila, &yahoo_fr, &msn_fr_FR, &orange, &aol_fr, };

// Latvia
const PrepopulatedEngine* engines_LV[] =
    { &google, &delfi_lv, &yahoo, &yandex_ru, &latne, };

// Libya
const PrepopulatedEngine* engines_LY[] =
    { &google, &maktoob, &yahoo, &yamli, &araby, &msn_en_XA, &msn_ar_XA, };

// Morocco
const PrepopulatedEngine* engines_MA[] =
    { &google, &yamli, &araby, &yahoo, &msn_en_XA, &msn_ar_XA, };

// Monaco
const PrepopulatedEngine* engines_MC[] =
    { &google, &voila, &yahoo_fr, &msn_fr_FR, &orange, &aol_fr, };

// Montenegro
const PrepopulatedEngine* engines_ME[] =
    { &google, &yahoo, &krstarica, &pogodak_rs, &aladin, &live, };

// Macedonia
const PrepopulatedEngine* engines_MK[] = { &google, &pogodok, &yahoo, &live, };

// Mexico
const PrepopulatedEngine* engines_MX[] =
    { &google, &msn_es_MX, &yahoo_mx, &ask_es, &altavista_mx, &terra_mx, };

// Malaysia
const PrepopulatedEngine* engines_MY[] =
    { &google, &yahoo_malaysia, &msn_en_MY, };

// Nicaragua
const PrepopulatedEngine* engines_NI[] =
    { &google, &msn_es_XL, &yahoo, &ask_es, &altavista, };

// Netherlands
const PrepopulatedEngine* engines_NL[] =
    { &google, &ilse, &msn_nl_NL, &yahoo_nl, &lycos_nl, &vinden, };

// Norway
const PrepopulatedEngine* engines_NO[] =
    { &google, &msn_nb_NO, &abcsok, &yahoo_no, &kvasir, &sesam, };

// New Zealand
const PrepopulatedEngine* engines_NZ[] = { &google, &yahoo_nz, &live_en_NZ, };

// Oman
const PrepopulatedEngine* engines_OM[] =
    { &google, &maktoob, &yahoo, &yamli, &araby, &msn_en_XA, &msn_ar_XA, };

// Panama
const PrepopulatedEngine* engines_PA[] =
    { &google, &msn_es_XL, &yahoo, &ask_es, &altavista, &lycos_es, };

// Peru
const PrepopulatedEngine* engines_PE[] =
    { &google, &msn_es_XL, &yahoo_pe, &terra_pe, &adonde, &ohperu, };

// Philippines
const PrepopulatedEngine* engines_PH[] = { &google, &yahoo_ph, &msn_en_PH, };

// Pakistan
const PrepopulatedEngine* engines_PK[] = { &google, &yahoo, &msn, };

// Puerto Rico
const PrepopulatedEngine* engines_PR[] =
    { &google, &msn_es_XL, &yahoo, &ask_es, &altavista, &mywebsearch, };

// Poland
const PrepopulatedEngine* engines_PL[] = { &google, &onet, &wp, &live_pl_PL, };

// Portugal
const PrepopulatedEngine* engines_PT[] =
    { &google, &sapo, &yahoo, &live_pt_PT, &netindex, &aeiou, };

// Paraguay
const PrepopulatedEngine* engines_PY[] =
    { &google, &msn_es_XL, &yahoo, &lycos_es, &yagua, &go, };

// Qatar
const PrepopulatedEngine* engines_QA[] =
    { &google, &maktoob, &yahoo, &araby, &msn_en_XA, &msn_ar_XA, };

// Romania
const PrepopulatedEngine* engines_RO[] = { &google, &yahoo, &live_ro_RO, };

// Serbia
const PrepopulatedEngine* engines_RS[] =
    { &google, &yahoo, &krstarica, &pogodak_rs, &aladin, &live, };

// Russia
const PrepopulatedEngine* engines_RU[] =
    { &google, &yandex_ru, &rambler, &mail_ru, &yahoo_ru, &live_ru_RU, };

// Saudi Arabia
const PrepopulatedEngine* engines_SA[] =
    { &google, &yahoo, &araby, &msn_en_XA, &msn_ar_XA, &maktoob, };

// Sweden
const PrepopulatedEngine* engines_SE[] =
    { &google, &eniro_se, &msn_sv_SE, &altavista_se, &spray, };

// Singapore
const PrepopulatedEngine* engines_SG[] =
    { &google, &yahoo_sg, &msn_en_SG, &rednano, };

// Slovenia
const PrepopulatedEngine* engines_SI[] =
    { &google, &najdi, &yahoo, &matkurja, &live_sl_SI, };

// Slovakia
const PrepopulatedEngine* engines_SK[] =
    { &google, &zoznam, &centrum_sk, &atlas_sk, &szm, &live_sk_SK, };

// El Salvador
const PrepopulatedEngine* engines_SV[] =
    { &google, &msn_es_XL, &yahoo, &ask_es, &altavista, &go, };

// Syria
const PrepopulatedEngine* engines_SY[] =
    { &google, &yahoo, &maktoob, &yamli, &araby, &msn_en_XA, &msn_ar_XA, };

// Thailand
const PrepopulatedEngine* engines_TH[] =
    { &google, &sanook, &yahoo_th, &live_th_TH, };

// Tunisia
const PrepopulatedEngine* engines_TN[] =
    { &google, &maktoob, &yamli, &yahoo, &msn_en_XA, &msn_ar_XA, };

// Turkey
const PrepopulatedEngine* engines_TR[] =
    { &google, &msn_tr_TR, &yahoo, &mynet, };

// Trinidad and Tobago
const PrepopulatedEngine* engines_TT[] = { &google, &live, &yahoo, &go, &aol, };

// Taiwan
const PrepopulatedEngine* engines_TW[] = { &google, &yahoo_tw, &yam, };

// Ukraine
const PrepopulatedEngine* engines_UA[] =
    { &google, &meta, &yandex_ua, &bigmir, &rambler, };

// United States
const PrepopulatedEngine* engines_US[] =
    { &google, &yahoo, &live_en_US, &aol, &ask, };

// Uruguay
const PrepopulatedEngine* engines_UY[] =
    { &google, &msn_es_XL, &yahoo, &go, &lycos_es, };

// Venezuela
const PrepopulatedEngine* engines_VE[] =
    { &google, &msn_es_XL, &yahoo_ve, &altavista, };

// Vietnam
const PrepopulatedEngine* engines_VN[] = { &google, &yahoo_vn, };

// Yemen
const PrepopulatedEngine* engines_YE[] =
    { &google, &yahoo, &maktoob, &yamli, &araby, &msn_en_XA, &msn_ar_XA, };

// South Africa
const PrepopulatedEngine* engines_ZA[] =
    { &google, &yahoo, &msn_en_ZA, &mweb, &iafrica, };

// Zimbabwe
const PrepopulatedEngine* engines_ZW[] = { &google, &yahoo, &msn, };

// Geographic mappings /////////////////////////////////////////////////////////

// Please refer to ISO 3166-1 for information about the two-character country
// codes; http://en.wikipedia.org/wiki/ISO_3166-1_alpha-2 is useful. In the
// following (C++) code, we pack the two letters of the country code into an int
// value we call the CountryID.

const int kCountryIDUnknown = -1;

inline int CountryCharsToCountryID(char c1, char c2) {
  return c1 << 8 | c2;
}

int CountryCharsToCountryIDWithUpdate(char c1, char c2) {
  // SPECIAL CASE: In 2003, Yugoslavia renamed itself to Serbia and Montenegro.
  // Serbia and Montenegro dissolved their union in June 2006. Yugoslavia was
  // ISO 'YU' and Serbia and Montenegro were ISO 'CS'. Serbia was subsequently
  // issued 'RS' and Montenegro 'ME'. Windows XP and Mac OS X Leopard still use
  // the value 'YU'. If we get a value of 'YU' or 'CS' we will map it to 'RS'.
  if ((c1 == 'Y' && c2 == 'U') ||
      (c1 == 'C' && c2 == 'S')) {
    c1 = 'R';
    c2 = 'S';
  }
  
  // SPECIAL CASE: Timor-Leste changed from 'TP' to 'TL' in 2002. Windows XP
  // predates this; we therefore map this value.
  if (c1 == 'T' && c2 == 'P')
    c2 = 'L';
  
  return CountryCharsToCountryID(c1, c2);
}

#if defined(OS_WIN)

// For reference, a list of GeoIDs can be found at
// http://msdn.microsoft.com/en-us/library/ms776390.aspx .
int GeoIDToCountryID(GEOID geo_id) {
  const int kISOBufferSize = 3;  // Two plus one for the terminator.
  wchar_t isobuf[kISOBufferSize] = { 0 };
  int retval = GetGeoInfo(geo_id, GEO_ISO2, isobuf, kISOBufferSize, 0);
  
  if (retval == kISOBufferSize &&
      !(isobuf[0] == L'X' && isobuf[1] == L'X'))
    return CountryCharsToCountryIDWithUpdate(static_cast<char>(isobuf[0]),
                                             static_cast<char>(isobuf[1]));

  // Various locations have ISO codes that Windows does not return.
  switch(geo_id) {
    case 0x144:   // Guernsey
      return CountryCharsToCountryID('G', 'G');
    case 0x148:   // Jersey
      return CountryCharsToCountryID('J', 'E');
    case 0x3B16:  // Isle of Man
      return CountryCharsToCountryID('I', 'M');
      
    // 'UM' (U.S. Minor Outlying Islands)
    case 0x7F:    // Johnston Atoll
    case 0x102:   // Wake Island
    case 0x131:   // Baker Island
    case 0x146:   // Howland Island
    case 0x147:   // Jarvis Island
    case 0x149:   // Kingman Reef
    case 0x152:   // Palmyra Atoll
    case 0x52FA:  // Midway Islands
      return CountryCharsToCountryID('U', 'M');

    // 'SH' (Saint Helena)
    case 0x12F:  // Ascension Island
    case 0x15C:  // Tristan da Cunha
      return CountryCharsToCountryID('S', 'H');

    // 'IO' (British Indian Ocean Territory)
    case 0x13A:  // Diego Garcia
      return CountryCharsToCountryID('I', 'O');

    // Other cases where there is no ISO country code; we assign countries that
    // can serve as reasonable defaults.
    case 0x154:  // Rota Island
    case 0x155:  // Saipan
    case 0x15A:  // Tinian Island
      return CountryCharsToCountryID('U', 'S');
    case 0x134:  // Channel Islands
      return CountryCharsToCountryID('G', 'B');
    case 0x143:  // Guantanamo Bay
    default:
      return kCountryIDUnknown;
  }
}

int GetCurrentCountryID() {
  GEOID geo_id = GetUserGeoID(GEOCLASS_NATION);
  
  return GeoIDToCountryID(geo_id);
}

#elif defined(OS_MACOSX)

int GetCurrentCountryID() {
  scoped_cftyperef<CFLocaleRef> locale(CFLocaleCopyCurrent());
  CFStringRef country = (CFStringRef)CFLocaleGetValue(locale.get(),
                                                      kCFLocaleCountryCode);
  if (!country)
    return kCountryIDUnknown;

  UniChar isobuf[2];
  CFRange char_range = CFRangeMake(0, 2);
  CFStringGetCharacters(country, char_range, isobuf);

  return CountryCharsToCountryIDWithUpdate(static_cast<char>(isobuf[0]),
                                           static_cast<char>(isobuf[1]));
}

#elif defined(OS_LINUX)

int GetCurrentCountryID() {
  NOTIMPLEMENTED();
  return kCountryIDUnknown;
}

#endif  // OS_*

int GetCountryIDFromPrefs(PrefService* prefs) {
  // See if the user overrode the country on the command line.
  const std::wstring country(
    CommandLine::ForCurrentProcess()->GetSwitchValue(switches::kCountry));
  if (country.length() == 2)
    return CountryCharsToCountryIDWithUpdate(static_cast<char>(country[0]),
                                             static_cast<char>(country[1]));

  // Cache first run Country ID value in prefs, and use it afterwards.  This
  // ensures that just because the user moves around, we won't automatically
  // make major changes to their available search providers, which would feel
  // surprising.
  if (!prefs)
    return GetCurrentCountryID();

  if (!prefs->HasPrefPath(prefs::kCountryIDAtInstall)) {
    int new_country_id;
#if defined(OS_WIN)
    // Upgrade the old platform-specific value if it's present.
    if (prefs->HasPrefPath(prefs::kGeoIDAtInstall)) {
      int geo_id = prefs->GetInteger(prefs::kGeoIDAtInstall);
      new_country_id = GeoIDToCountryID(geo_id);
    } else {
      new_country_id = GetCurrentCountryID();
    }
#else
    new_country_id = GetCurrentCountryID();
#endif
    prefs->SetInteger(prefs::kCountryIDAtInstall, new_country_id);
  }

  return prefs->GetInteger(prefs::kCountryIDAtInstall);
}

void GetPrepopulationSetFromCountryID(PrefService* prefs,
                                      const PrepopulatedEngine*** engines,
                                      size_t* num_engines) {
  // NOTE: This function should ALWAYS set its outparams.

  // If you add a new country make sure to update the unit test for coverage.
  switch (GetCountryIDFromPrefs(prefs)) {

#define CHAR_A 'A'
#define CHAR_B 'B'
#define CHAR_C 'C'
#define CHAR_D 'D'
#define CHAR_E 'E'
#define CHAR_F 'F'
#define CHAR_G 'G'
#define CHAR_H 'H'
#define CHAR_I 'I'
#define CHAR_J 'J'
#define CHAR_K 'K'
#define CHAR_L 'L'
#define CHAR_M 'M'
#define CHAR_N 'N'
#define CHAR_O 'O'
#define CHAR_P 'P'
#define CHAR_Q 'Q'
#define CHAR_R 'R'
#define CHAR_S 'S'
#define CHAR_T 'T'
#define CHAR_U 'U'
#define CHAR_V 'V'
#define CHAR_W 'W'
#define CHAR_X 'X'
#define CHAR_Y 'Y'
#define CHAR_Z 'Z'
#define CHAR(ch) CHAR_##ch
#define CODE_TO_ID(code1, code2)\
    (CHAR(code1) << 8 | CHAR(code2))

#define UNHANDLED_COUNTRY(code1, code2)\
    case CODE_TO_ID(code1, code2):
#define END_UNHANDLED_COUNTRIES(code1, code2)\
      *engines = engines_##code1##code2;\
      *num_engines = arraysize(engines_##code1##code2);\
      return;
#define DECLARE_COUNTRY(code1, code2)\
    UNHANDLED_COUNTRY(code1, code2)\
    END_UNHANDLED_COUNTRIES(code1, code2)

    // Countries with their own, dedicated engine set.
    DECLARE_COUNTRY(A, E)  // United Arab Emirates
    DECLARE_COUNTRY(A, L)  // Albania
    DECLARE_COUNTRY(A, R)  // Argentina
    DECLARE_COUNTRY(A, T)  // Austria
    DECLARE_COUNTRY(A, U)  // Australia
    DECLARE_COUNTRY(B, A)  // Bosnia and Herzegovina
    DECLARE_COUNTRY(B, E)  // Belgium
    DECLARE_COUNTRY(B, G)  // Bulgaria
    DECLARE_COUNTRY(B, H)  // Bahrain
    DECLARE_COUNTRY(B, N)  // Brunei
    DECLARE_COUNTRY(B, O)  // Bolivia
    DECLARE_COUNTRY(B, R)  // Brazil
    DECLARE_COUNTRY(B, Y)  // Belarus
    DECLARE_COUNTRY(B, Z)  // Belize
    DECLARE_COUNTRY(C, A)  // Canada
    DECLARE_COUNTRY(C, H)  // Switzerland
    DECLARE_COUNTRY(C, L)  // Chile
    DECLARE_COUNTRY(C, N)  // China
    DECLARE_COUNTRY(C, O)  // Colombia
    DECLARE_COUNTRY(C, R)  // Costa Rica
    DECLARE_COUNTRY(C, Z)  // Czech Republic
    DECLARE_COUNTRY(D, E)  // Germany
    DECLARE_COUNTRY(D, K)  // Denmark
    DECLARE_COUNTRY(D, O)  // Dominican Republic
    DECLARE_COUNTRY(D, Z)  // Algeria
    DECLARE_COUNTRY(E, C)  // Ecuador
    DECLARE_COUNTRY(E, E)  // Estonia
    DECLARE_COUNTRY(E, G)  // Egypt
    DECLARE_COUNTRY(E, S)  // Spain
    DECLARE_COUNTRY(F, I)  // Finland
    DECLARE_COUNTRY(F, O)  // Faroe Islands
    DECLARE_COUNTRY(F, R)  // France
    DECLARE_COUNTRY(G, B)  // United Kingdom
    DECLARE_COUNTRY(G, R)  // Greece
    DECLARE_COUNTRY(G, T)  // Guatemala
    DECLARE_COUNTRY(H, K)  // Hong Kong
    DECLARE_COUNTRY(H, N)  // Honduras
    DECLARE_COUNTRY(H, R)  // Croatia
    DECLARE_COUNTRY(H, U)  // Hungary
    DECLARE_COUNTRY(I, D)  // Indonesia
    DECLARE_COUNTRY(I, E)  // Ireland
    DECLARE_COUNTRY(I, L)  // Israel
    DECLARE_COUNTRY(I, N)  // India
    DECLARE_COUNTRY(I, Q)  // Iraq
    DECLARE_COUNTRY(I, R)  // Iran
    DECLARE_COUNTRY(I, S)  // Iceland
    DECLARE_COUNTRY(I, T)  // Italy
    DECLARE_COUNTRY(J, M)  // Jamaica
    DECLARE_COUNTRY(J, O)  // Jordan
    DECLARE_COUNTRY(J, P)  // Japan
    DECLARE_COUNTRY(K, E)  // Kenya
    DECLARE_COUNTRY(K, R)  // South Korea
    DECLARE_COUNTRY(K, W)  // Kuwait
    DECLARE_COUNTRY(L, B)  // Lebanon
    DECLARE_COUNTRY(L, I)  // Liechtenstein
    DECLARE_COUNTRY(L, T)  // Lithuania
    DECLARE_COUNTRY(L, U)  // Luxembourg
    DECLARE_COUNTRY(L, V)  // Latvia
    DECLARE_COUNTRY(L, Y)  // Libya
    DECLARE_COUNTRY(M, A)  // Morocco
    DECLARE_COUNTRY(M, C)  // Monaco
    DECLARE_COUNTRY(M, E)  // Montenegro
    DECLARE_COUNTRY(M, K)  // Macedonia
    DECLARE_COUNTRY(M, X)  // Mexico
    DECLARE_COUNTRY(M, Y)  // Malaysia
    DECLARE_COUNTRY(N, I)  // Nicaragua
    DECLARE_COUNTRY(N, L)  // Netherlands
    DECLARE_COUNTRY(N, O)  // Norway
    DECLARE_COUNTRY(N, Z)  // New Zealand
    DECLARE_COUNTRY(O, M)  // Oman
    DECLARE_COUNTRY(P, A)  // Panama
    DECLARE_COUNTRY(P, E)  // Peru
    DECLARE_COUNTRY(P, H)  // Philippines
    DECLARE_COUNTRY(P, K)  // Pakistan
    DECLARE_COUNTRY(P, L)  // Poland
    DECLARE_COUNTRY(P, R)  // Puerto Rico
    DECLARE_COUNTRY(P, T)  // Portugal
    DECLARE_COUNTRY(P, Y)  // Paraguay
    DECLARE_COUNTRY(Q, A)  // Qatar
    DECLARE_COUNTRY(R, O)  // Romania
    DECLARE_COUNTRY(R, S)  // Serbia
    DECLARE_COUNTRY(R, U)  // Russia
    DECLARE_COUNTRY(S, A)  // Saudi Arabia
    DECLARE_COUNTRY(S, E)  // Sweden
    DECLARE_COUNTRY(S, G)  // Singapore
    DECLARE_COUNTRY(S, I)  // Slovenia
    DECLARE_COUNTRY(S, K)  // Slovakia
    DECLARE_COUNTRY(S, V)  // El Salvador
    DECLARE_COUNTRY(S, Y)  // Syria
    DECLARE_COUNTRY(T, H)  // Thailand
    DECLARE_COUNTRY(T, N)  // Tunisia
    DECLARE_COUNTRY(T, R)  // Turkey
    DECLARE_COUNTRY(T, T)  // Trinidad and Tobago
    DECLARE_COUNTRY(T, W)  // Taiwan
    DECLARE_COUNTRY(U, A)  // Ukraine
    DECLARE_COUNTRY(U, S)  // United States
    DECLARE_COUNTRY(U, Y)  // Uruguay
    DECLARE_COUNTRY(V, E)  // Venezuela
    DECLARE_COUNTRY(V, N)  // Vietnam
    DECLARE_COUNTRY(Y, E)  // Yemen
    DECLARE_COUNTRY(Z, A)  // South Africa
    DECLARE_COUNTRY(Z, W)  // Zimbabwe

    // Countries using the "Australia" engine set.
    UNHANDLED_COUNTRY(C, C)  // Cocos Islands
    UNHANDLED_COUNTRY(C, X)  // Christmas Island
    UNHANDLED_COUNTRY(H, M)  // Heard Island and McDonald Islands
    UNHANDLED_COUNTRY(N, F)  // Norfolk Island
    END_UNHANDLED_COUNTRIES(A, U)

    // Countries using the "China" engine set.
    UNHANDLED_COUNTRY(M, O)  // Macao
    END_UNHANDLED_COUNTRIES(C, N)

    // Countries using the "Denmark" engine set.
    UNHANDLED_COUNTRY(G, L)  // Greenland
    END_UNHANDLED_COUNTRIES(D, K)

    // Countries using the "Spain" engine set.
    UNHANDLED_COUNTRY(A, D)  // Andorra
    END_UNHANDLED_COUNTRIES(E, S)

    // Countries using the "France" engine set.
    UNHANDLED_COUNTRY(B, F)  // Burkina Faso
    UNHANDLED_COUNTRY(B, I)  // Burundi
    UNHANDLED_COUNTRY(B, J)  // Benin
    UNHANDLED_COUNTRY(C, D)  // Congo - Kinshasa
    UNHANDLED_COUNTRY(C, F)  // Central African Republic
    UNHANDLED_COUNTRY(C, G)  // Congo - Brazzaville
    UNHANDLED_COUNTRY(C, I)  // Ivory Coast
    UNHANDLED_COUNTRY(C, M)  // Cameroon
    UNHANDLED_COUNTRY(D, J)  // Djibouti
    UNHANDLED_COUNTRY(G, A)  // Gabon
    UNHANDLED_COUNTRY(G, F)  // French Guiana
    UNHANDLED_COUNTRY(G, N)  // Guinea
    UNHANDLED_COUNTRY(G, P)  // Guadeloupe
    UNHANDLED_COUNTRY(H, T)  // Haiti
#if defined(OS_WIN)
    UNHANDLED_COUNTRY(I, P)  // Clipperton Island ('IP' is an WinXP-ism; ISO
                             //                    includes it with France)
#endif
    UNHANDLED_COUNTRY(M, L)  // Mali
    UNHANDLED_COUNTRY(M, Q)  // Martinique
    UNHANDLED_COUNTRY(N, C)  // New Caledonia
    UNHANDLED_COUNTRY(N, E)  // Niger
    UNHANDLED_COUNTRY(P, F)  // French Polynesia
    UNHANDLED_COUNTRY(P, M)  // Saint Pierre and Miquelon
    UNHANDLED_COUNTRY(R, E)  // Reunion
    UNHANDLED_COUNTRY(S, N)  // Senegal
    UNHANDLED_COUNTRY(T, D)  // Chad
    UNHANDLED_COUNTRY(T, F)  // French Southern Territories
    UNHANDLED_COUNTRY(T, G)  // Togo
    UNHANDLED_COUNTRY(W, F)  // Wallis and Futuna
    UNHANDLED_COUNTRY(Y, T)  // Mayotte
    END_UNHANDLED_COUNTRIES(F, R)

    // Countries using the "Greece" engine set.
    UNHANDLED_COUNTRY(C, Y)  // Cyprus
    END_UNHANDLED_COUNTRIES(G, R)

    // Countries using the "Italy" engine set.
    UNHANDLED_COUNTRY(S, M)  // San Marino
    UNHANDLED_COUNTRY(V, A)  // Vatican
    END_UNHANDLED_COUNTRIES(I, T)

    // Countries using the "Netherlands" engine set.
    UNHANDLED_COUNTRY(A, N)  // Netherlands Antilles
    UNHANDLED_COUNTRY(A, W)  // Aruba
    END_UNHANDLED_COUNTRIES(N, L)

    // Countries using the "Norway" engine set.
    UNHANDLED_COUNTRY(B, V)  // Bouvet Island
    UNHANDLED_COUNTRY(S, J)  // Svalbard and Jan Mayen
    END_UNHANDLED_COUNTRIES(N, O)

    // Countries using the "New Zealand" engine set.
    UNHANDLED_COUNTRY(C, K)  // Cook Islands
    UNHANDLED_COUNTRY(N, U)  // Niue
    UNHANDLED_COUNTRY(T, K)  // Tokelau
    END_UNHANDLED_COUNTRIES(N, Z)

    // Countries using the "Portugal" engine set.
    UNHANDLED_COUNTRY(C, V)  // Cape Verde
    UNHANDLED_COUNTRY(G, W)  // Guinea-Bissau
    UNHANDLED_COUNTRY(M, Z)  // Mozambique
    UNHANDLED_COUNTRY(S, T)  // Sao Tome and Principe
    UNHANDLED_COUNTRY(T, L)  // Timor-Leste
    END_UNHANDLED_COUNTRIES(P, T)

    // Countries using the "Russia" engine set.
    UNHANDLED_COUNTRY(A, M)  // Armenia
    UNHANDLED_COUNTRY(A, Z)  // Azerbaijan
    UNHANDLED_COUNTRY(K, G)  // Kyrgyzstan
    UNHANDLED_COUNTRY(K, Z)  // Kazakhstan
    UNHANDLED_COUNTRY(T, J)  // Tajikistan
    UNHANDLED_COUNTRY(T, M)  // Turkmenistan
    UNHANDLED_COUNTRY(U, Z)  // Uzbekistan
    END_UNHANDLED_COUNTRIES(R, U)

    // Countries using the "Saudi Arabia" engine set.
    UNHANDLED_COUNTRY(M, R)  // Mauritania
    UNHANDLED_COUNTRY(P, S)  // Palestinian Territory
    UNHANDLED_COUNTRY(S, D)  // Sudan
    END_UNHANDLED_COUNTRIES(S, A)

    // Countries using the "United Kingdom" engine set.
    UNHANDLED_COUNTRY(B, M)  // Bermuda
    UNHANDLED_COUNTRY(F, K)  // Falkland Islands
    UNHANDLED_COUNTRY(G, G)  // Guernsey
    UNHANDLED_COUNTRY(G, I)  // Gibraltar
    UNHANDLED_COUNTRY(G, S)  // South Georgia and the South Sandwich
                             //   Islands
    UNHANDLED_COUNTRY(I, M)  // Isle of Man
    UNHANDLED_COUNTRY(I, O)  // British Indian Ocean Territory
    UNHANDLED_COUNTRY(J, E)  // Jersey
    UNHANDLED_COUNTRY(K, Y)  // Cayman Islands
    UNHANDLED_COUNTRY(M, S)  // Montserrat
    UNHANDLED_COUNTRY(M, T)  // Malta
    UNHANDLED_COUNTRY(P, N)  // Pitcairn Islands
    UNHANDLED_COUNTRY(S, H)  // Saint Helena, Ascension Island, and Tristan da
                             //   Cunha
    UNHANDLED_COUNTRY(T, C)  // Turks and Caicos Islands
    UNHANDLED_COUNTRY(V, G)  // British Virgin Islands
    END_UNHANDLED_COUNTRIES(G, B)

    // Countries using the "United States" engine set.
    UNHANDLED_COUNTRY(A, S)  // American Samoa
    UNHANDLED_COUNTRY(G, U)  // Guam
    UNHANDLED_COUNTRY(M, P)  // Northern Mariana Islands
    UNHANDLED_COUNTRY(U, M)  // U.S. Minor Outlying Islands
    UNHANDLED_COUNTRY(V, I)  // U.S. Virgin Islands
    END_UNHANDLED_COUNTRIES(U, S)

    // Countries using the "default" engine set.
    UNHANDLED_COUNTRY(A, F)  // Afghanistan
    UNHANDLED_COUNTRY(A, G)  // Antigua and Barbuda
    UNHANDLED_COUNTRY(A, I)  // Anguilla
    UNHANDLED_COUNTRY(A, O)  // Angola
    UNHANDLED_COUNTRY(A, Q)  // Antarctica
    UNHANDLED_COUNTRY(B, B)  // Barbados
    UNHANDLED_COUNTRY(B, D)  // Bangladesh
    UNHANDLED_COUNTRY(B, S)  // Bahamas
    UNHANDLED_COUNTRY(B, T)  // Bhutan
    UNHANDLED_COUNTRY(B, W)  // Botswana
    UNHANDLED_COUNTRY(C, U)  // Cuba
    UNHANDLED_COUNTRY(D, M)  // Dominica
    UNHANDLED_COUNTRY(E, R)  // Eritrea
    UNHANDLED_COUNTRY(E, T)  // Ethiopia
    UNHANDLED_COUNTRY(F, J)  // Fiji
    UNHANDLED_COUNTRY(F, M)  // Micronesia
    UNHANDLED_COUNTRY(G, D)  // Grenada
    UNHANDLED_COUNTRY(G, E)  // Georgia
    UNHANDLED_COUNTRY(G, H)  // Ghana
    UNHANDLED_COUNTRY(G, M)  // Gambia
    UNHANDLED_COUNTRY(G, Q)  // Equatorial Guinea
    UNHANDLED_COUNTRY(G, Y)  // Guyana
    UNHANDLED_COUNTRY(K, H)  // Cambodia
    UNHANDLED_COUNTRY(K, I)  // Kiribati
    UNHANDLED_COUNTRY(K, M)  // Comoros
    UNHANDLED_COUNTRY(K, N)  // Saint Kitts and Nevis
    UNHANDLED_COUNTRY(K, P)  // North Korea
    UNHANDLED_COUNTRY(L, A)  // Laos
    UNHANDLED_COUNTRY(L, C)  // Saint Lucia
    UNHANDLED_COUNTRY(L, K)  // Sri Lanka
    UNHANDLED_COUNTRY(L, R)  // Liberia
    UNHANDLED_COUNTRY(L, S)  // Lesotho
    UNHANDLED_COUNTRY(M, D)  // Moldova
    UNHANDLED_COUNTRY(M, G)  // Madagascar
    UNHANDLED_COUNTRY(M, H)  // Marshall Islands
    UNHANDLED_COUNTRY(M, M)  // Myanmar
    UNHANDLED_COUNTRY(M, N)  // Mongolia
    UNHANDLED_COUNTRY(M, U)  // Mauritius
    UNHANDLED_COUNTRY(M, V)  // Maldives
    UNHANDLED_COUNTRY(M, W)  // Malawi
    UNHANDLED_COUNTRY(N, A)  // Namibia
    UNHANDLED_COUNTRY(N, G)  // Nigeria
    UNHANDLED_COUNTRY(N, P)  // Nepal
    UNHANDLED_COUNTRY(N, R)  // Nauru
    UNHANDLED_COUNTRY(P, G)  // Papua New Guinea
    UNHANDLED_COUNTRY(P, W)  // Palau
    UNHANDLED_COUNTRY(R, W)  // Rwanda
    UNHANDLED_COUNTRY(S, B)  // Solomon Islands
    UNHANDLED_COUNTRY(S, C)  // Seychelles
    UNHANDLED_COUNTRY(S, L)  // Sierra Leone
    UNHANDLED_COUNTRY(S, O)  // Somalia
    UNHANDLED_COUNTRY(S, R)  // Suriname
    UNHANDLED_COUNTRY(S, Z)  // Swaziland
    UNHANDLED_COUNTRY(T, O)  // Tonga
    UNHANDLED_COUNTRY(T, V)  // Tuvalu
    UNHANDLED_COUNTRY(T, Z)  // Tanzania
    UNHANDLED_COUNTRY(U, G)  // Uganda
    UNHANDLED_COUNTRY(V, C)  // Saint Vincent and the Grenadines
    UNHANDLED_COUNTRY(V, U)  // Vanuatu
    UNHANDLED_COUNTRY(W, S)  // Samoa
    UNHANDLED_COUNTRY(Z, M)  // Zambia
    case kCountryIDUnknown:
    default:                // Unhandled location
    END_UNHANDLED_COUNTRIES(def, ault)
  }
}

}  // namespace

namespace TemplateURLPrepopulateData {

void RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kGeoIDAtInstall, -1);
  prefs->RegisterIntegerPref(prefs::kCountryIDAtInstall, kCountryIDUnknown);
}

int GetDataVersion() {
  return 20;  // Increment this if you change the above data in ways that mean
              // users with existing data should get a new version.
}

void GetPrepopulatedEngines(PrefService* prefs,
                            std::vector<TemplateURL*>* t_urls,
                            size_t* default_search_provider_index) {
  const PrepopulatedEngine** engines;
  size_t num_engines;
  GetPrepopulationSetFromCountryID(prefs, &engines, &num_engines);
  *default_search_provider_index = 0;

  for (size_t i = 0; i < num_engines; ++i) {
    TemplateURL* new_turl = new TemplateURL();
    new_turl->SetURL(engines[i]->search_url, 0, 0);
    if (engines[i]->favicon_url)
      new_turl->SetFavIconURL(GURL(engines[i]->favicon_url));
    if (engines[i]->suggest_url)
      new_turl->SetSuggestionsURL(engines[i]->suggest_url, 0, 0);
    new_turl->set_short_name(engines[i]->name);
    if (engines[i]->keyword == NULL)
      new_turl->set_autogenerate_keyword(true);
    else
      new_turl->set_keyword(engines[i]->keyword);
    new_turl->set_show_in_default_list(true);
    new_turl->set_safe_for_autoreplace(true);
    new_turl->set_date_created(Time());
    std::vector<std::string> turl_encodings;
    turl_encodings.push_back(engines[i]->encoding);
    new_turl->set_input_encodings(turl_encodings);
    new_turl->set_prepopulate_id(engines[i]->id);
    t_urls->push_back(new_turl);
  }
}

}  // namespace TemplateURLPrepopulateData
