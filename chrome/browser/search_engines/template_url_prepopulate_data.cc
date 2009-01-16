// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_prepopulate_data.h"

#include "base/command_line.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#undef IN  // On Windows, windef.h defines this, which screws up "India" cases.

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
  const wchar_t* const favicon_url;  // If NULL, there is no favicon.
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
  L"http://abcsok.no/favicon.ico",
  L"http://abcsok.no/index.html?q={searchTerms}",
  "UTF-8",
  NULL,
  72,
};

const PrepopulatedEngine adonde = {
  L"Adonde.com",
  L"adonde.com",
  L"http://www.adonde.com/favicon.ico",
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
  L"http://aeiou.pt/favicon.ico",
  L"http://aeiou.pt/pesquisa/index.php?p={searchTerms}",
  "ISO-8859-1",
  NULL,
  79,
};

const PrepopulatedEngine aladin = {
  L"Aladin",
  L"aladin.info",
  L"http://www.aladin.info/favicon.ico",
  L"http://www.aladin.info/search/index.php?term={searchTerms}&req=search&"
      L"source=2",
  "UTF-8",
  NULL,
  18,
};

const PrepopulatedEngine altavista = {
  L"AltaVista",
  L"altavista.com",
  L"http://www.altavista.com/favicon.ico",
  L"http://www.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine altavista_ar = {
  L"AltaVista",
  L"ar.altavista.com",
  L"http://ar.altavista.com/favicon.ico",
  L"http://ar.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine altavista_es = {
  L"AltaVista",
  L"es.altavista.com",
  L"http://es.altavista.com/favicon.ico",
  L"http://es.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine altavista_mx = {
  L"AltaVista",
  L"mx.altavista.com",
  L"http://mx.altavista.com/favicon.ico",
  L"http://mx.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine altavista_se = {
  L"AltaVista",
  L"se.altavista.com",
  L"http://se.altavista.com/favicon.ico",
  L"http://se.altavista.com/web/results?q={searchTerms}",
  "UTF-8",
  NULL,
  89,
};

const PrepopulatedEngine aol = {
  L"AOL",
  L"aol.com",
  L"http://search.aol.com/favicon.ico",
  L"http://search.aol.com/aol/search?query={searchTerms}",
  "UTF-8",
  NULL,
  35,
};

const PrepopulatedEngine aol_fr = {
  L"AOL",
  L"aol.fr",
  L"http://www.aol.fr/favicon.ico",
  L"http://www.recherche.aol.fr/aol/search?q={searchTerms}",
  "UTF-8",
  NULL,
  35,
};

const PrepopulatedEngine aonde = {
  L"AONDE.com",
  L"aonde.com",
  L"http://busca.aonde.com/favicon.ico",
  L"http://busca.aonde.com/?keys={searchTerms}",
  "ISO-8859-1",
  NULL,
  80,
};

const PrepopulatedEngine araby = {
  L"\x0639\x0631\x0628\x064a",
  L"araby.com",
  L"http://araby.com/favicon.ico",
  L"http://araby.com/?q={searchTerms}",
  "UTF-8",
  NULL,
  12,
};

const PrepopulatedEngine ask = {
  L"Ask",
  L"ask.com",
  L"http://www.ask.com/favicon.ico",
  L"http://www.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine ask_de = {
  L"Ask.com Deutschland",
  L"de.ask.com",
  L"http://de.ask.com/favicon.ico",
  L"http://de.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.de.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine ask_es = {
  L"Ask.com Espa" L"\x00f1" L"a",
  L"es.ask.com",
  L"http://es.ask.com/favicon.ico",
  L"http://es.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.es.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine ask_it = {
  L"Ask.com Italia",
  L"it.ask.com",
  L"http://it.ask.com/favicon.ico",
  L"http://it.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.it.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine ask_uk = {
  L"Ask.com UK",
  L"uk.ask.com",
  L"http://uk.ask.com/favicon.ico",
  L"http://uk.ask.com/web?q={searchTerms}",
  "UTF-8",
  L"http://ss.uk.ask.com/query?q={searchTerms}&li=ff",
  4,
};

const PrepopulatedEngine atlas_cz = {
  L"Atlas",
  L"atlas.cz",
  L"http://img.atlas.cz/favicon.ico",
  L"http://search.atlas.cz/?q={searchTerms}",
  "windows-1250",
  NULL,
  27,
};

const PrepopulatedEngine atlas_sk = {
  L"ATLAS.SK",
  L"atlas.sk",
  L"http://www.atlas.sk/images/favicon.ico",
  L"http://hladaj.atlas.sk/fulltext/?phrase={searchTerms}",
  "UTF-8",
  NULL,
  27,
};

const PrepopulatedEngine baidu = {
  L"\x767e\x5ea6",
  L"baidu.com",
  L"http://www.baidu.com/favicon.ico",
  L"http://www.baidu.com/s?wd={searchTerms}",
  "GB2312",
  NULL,
  21,
};

const PrepopulatedEngine biglobe = {
  L"BIGLOBE",
  L"biglobe.ne.jp",
  L"http://cgi.search.biglobe.ne.jp/favicon.ico",
  L"http://cgi.search.biglobe.ne.jp/cgi-bin/search2-b?q={searchTerms}",
  "Shift_JIS",
  NULL,
  64,
};

const PrepopulatedEngine bigmir = {
  L"bigmir)net",
  L"bigmir.net",
  L"http://i.bigmir.net/favicon.ico",
  L"http://search.bigmir.net/index.php?q={searchTerms}",
  "windows-1251",
  NULL,
  33,
};

const PrepopulatedEngine bluewin = {
  L"Bluewin",
  L"search.bluewin.ch",
  L"http://search.bluewin.ch/favicon.ico",
  L"http://search.bluewin.ch/bw/search/web/de/result.jsp?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  52,
};

const PrepopulatedEngine centrum_cz = {
  L"Centrum.cz",
  L"centrum.cz",
  L"http://img.centrum.cz/6/vy2/o/favicon.ico",
  L"http://search.centrum.cz/index.php?charset={inputEncoding}&q={searchTerms}",
  "UTF-8",
  NULL,
  26,
};

const PrepopulatedEngine centrum_sk = {
  L"Centrum.sk",
  L"centrum.sk",
  L"http://img.centrum.sk/4/favicon.ico",
  L"http://search.centrum.sk/index.php?charset={inputEncoding}&q={searchTerms}",
  "UTF-8",
  NULL,
  26,
};

const PrepopulatedEngine conexcol = {
  L"Conexcol.com",
  L"conexcol.com",
  L"http://www.conexcol.com/favicon.ico",
  L"http://buscar.conexcol.com/cgi-ps/busqueda.cgi?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  91,
};

const PrepopulatedEngine daum = {
  L"Daum",
  L"daum.net",
  L"http://search.daum.net/favicon.ico",
  L"http://search.daum.net/search?q={searchTerms}",
  "EUC-KR",
   L"http://sug.search.daum.net/search_nsuggest?mod=fxjson&q={searchTerms}",
  68,
};

const PrepopulatedEngine delfi_ee = {
  L"DELFI",
  L"delfi.ee",
  L"http://g.delfi.ee/s/search.png",
  L"http://otsing.delfi.ee/i.php?q={searchTerms}",
  "ISO-8859-1",
  NULL,
  45,
};

const PrepopulatedEngine delfi_lt = {
  L"DELFI",
  L"delfi.lt",
  L"http://search.delfi.lt/img/favicon.png",
  L"http://search.delfi.lt/search.php?q={searchTerms}",
  "UTF-8",
  NULL,
  45,
};

const PrepopulatedEngine delfi_lv = {
  L"DELFI",
  L"delfi.lv",
  L"http://smart.delfi.lv/img/smart_search.png",
  L"http://smart.delfi.lv/i.php?enc={inputEncoding}&q={searchTerms}",
  "UTF-8",
  NULL,
  45,
};

const PrepopulatedEngine embla = {
  L"Embla",
  L"embla.is",
  L"http://embla.is/favicon.ico",
  L"http://embla.is/mm/embla/?s={searchTerms}",
  "ISO-8859-1",
  NULL,
  60,
};

const PrepopulatedEngine empas = {
  L"\xc5e0\xd30c\xc2a4",
  L"empas.com",
  L"http://search.empas.com/favicon.ico",
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
  L"http://eniro.dk/favicon.ico",
  L"http://eniro.dk/query?search_word={searchTerms}&what=web_local",
  "ISO-8859-1",
  NULL,
  29,
};

const PrepopulatedEngine eniro_fi = {
  L"Eniro",
  L"eniro.fi",
  L"http://eniro.fi/favicon.ico",
  L"http://eniro.fi/query?search_word={searchTerms}&what=web_local",
  "ISO-8859-1",
  NULL,
  29,
};

const PrepopulatedEngine eniro_se = {
  L"Eniro",
  L"eniro.se",
  L"http://eniro.se/favicon.ico",
  L"http://eniro.se/query?search_word={searchTerms}&what=web_local",
  "ISO-8859-1",
  NULL,
  29,
};

const PrepopulatedEngine finna = {
  L"FINNA",
  L"finna.is",
  L"http://finna.is/favicon.ico",
  L"http://finna.is/WWW_Search/?query={searchTerms}",
  "UTF-8",
  NULL,
  61,
};

const PrepopulatedEngine fonecta_02_fi = {
  L"Fonecta 02.fi",
  L"www.fi",
  L"http://www.02.fi/img/favicon.ico",
  L"http://www.02.fi/haku/{searchTerms}",
  "UTF-8",
  NULL,
  46,
};

const PrepopulatedEngine forthnet = {
  L"Forthnet",
  L"forthnet.gr",
  L"http://search.forthnet.gr/favicon.ico",
  L"http://search.forthnet.gr/cgi-bin/query?mss=search&q={searchTerms}",
  "windows-1253",
  NULL,
  53,
};

const PrepopulatedEngine gigabusca = {
  L"GiGaBusca",
  L"gigabusca.com.br",
  L"http://www.gigabusca.com.br/favicon.ico",
  L"http://www.gigabusca.com.br/buscar.php?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  81,
};

const PrepopulatedEngine go = {
  L"GO.com",
  L"go.com",
  L"http://search.yahoo.com/favicon.ico",
  L"http://search.yahoo.com/search?ei={inputEncoding}&p={searchTerms}&"
      L"fr=hsusgo1",
  "ISO-8859-1",
  NULL,
  40,
};

const PrepopulatedEngine goo = {
  L"goo",
  L"goo.ne.jp",
  L"http://goo.ne.jp/gooicon.ico",
  L"http://search.goo.ne.jp/web.jsp?MT={searchTerms}&IE={inputEncoding}",
  "UTF-8",
  NULL,
  92,
};

const PrepopulatedEngine google = {
  L"Google",
  NULL,
  L"http://www.google.com/favicon.ico",
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
  L"http://guruji.com/favicon.ico",
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
  L"http://search.ilse.nl/images/favicon.ico",
  L"http://search.ilse.nl/web?search_for={searchTerms}",
  "ISO-8859-1",
  NULL,
  30,
};

const PrepopulatedEngine in = {
  L"in.gr",
  L"in.gr",
  L"http://www.in.gr/favicon.ico",
  L"http://find.in.gr/result.asp?q={searchTerms}",
  "ISO-8859-7",
  NULL,
  54,
};

const PrepopulatedEngine jabse = {
  L"Jabse",
  L"jabse.com",
  L"http://www.jabse.com/favicon.ico",
  L"http://www.jabse.com/searchmachine.php?query={searchTerms}",
  "UTF-8",
  NULL,
  19,
};

const PrepopulatedEngine jamaicalive = {
  L"JamaicaLive",
  L"jalive.com.jm",
  L"http://jalive.com.jm/favicon.ico",
  L"http://jalive.com.jm/search/?mode=allwords&search={searchTerms}",
  "ISO-8859-1",
  NULL,
  39,
};

const PrepopulatedEngine jubii = {
  L"Jubii",
  L"jubii.dk",
  L"http://search.jubii.dk/favicon_jubii.ico",
  L"http://search.jubii.dk/cgi-bin/pursuit?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  28,
};

const PrepopulatedEngine krstarica = {
  L"Krstarica",
  L"krstarica.rs",
  L"http://pretraga.krstarica.com/favicon.ico",
  L"http://pretraga.krstarica.com/index.php?q={searchTerms}",
  "windows-1250",
  NULL,
  84,
};

const PrepopulatedEngine kvasir = {
  L"Kvasir",
  L"kvasir.no",
  L"http://www.kvasir.no/img/favicon.ico",
  L"http://www.kvasir.no/nettsok/searchResult.html?searchExpr={searchTerms}",
  "ISO-8859-1",
  NULL,
  73,
};

const PrepopulatedEngine latne = {
  L"LATNE",
  L"latne.lv",
  L"http://latne.lv/favicon.ico",
  L"http://latne.lv/siets.php?q={searchTerms}",
  "UTF-8",
  NULL,
  71,
};

const PrepopulatedEngine leit = {
  L"leit.is",
  L"leit.is",
  L"http://leit.is/leit.ico",
  L"http://leit.is/query.aspx?qt={searchTerms}",
  "ISO-8859-1",
  NULL,
  59,
};

const PrepopulatedEngine libero = {
  L"Libero",
  L"libero.it",
  L"http://arianna.libero.it/favicon.ico",
  L"http://arianna.libero.it/search/abin/integrata.cgi?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  63,
};

const PrepopulatedEngine live = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_ar_XA = {
  L"Live Search (\x0627\x0644\x0639\x0631\x0628\x064a\x0629)",
  L"",  // "live.com" is already taken by live_en_XA (see comment on ID below).
  L"http://search.live.com/s/wlflag.ico",
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
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=bg-BG&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_cs_CZ = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=cs-CZ&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_el_GR = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=el-GR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_en_ID = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=en_ID&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_en_NZ = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=en-NZ&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_en_US = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=en-US&mkt=en-US&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_en_XA = {
  L"Live Search (English)",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=en-XA&mkt=en-XA&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_et_EE = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=et-EE&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_hr_HR = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=hr-HR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_hu_HU = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=hu-HU&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_it_IT = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=it-IT&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_lt_LT = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=lt-LT&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_pl_PL = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=pl-PL&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_pt_PT = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=pt-PT&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_ro_RO = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=ro-RO&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_ru_RU = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=ru-RU&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_sk_SK = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=sk-SK&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_sl_SI = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=sl-SI&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine live_th_TH = {
  L"Live Search",
  L"live.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=th-TH&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine lycos_es = {
  L"Lycos Espa" L"\x00f1" L"a",
  L"lycos.es",
  L"http://buscador.lycos.es/favicon.ico",
  L"http://buscador.lycos.es/cgi-bin/pursuit?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  34,
};

const PrepopulatedEngine lycos_nl = {
  L"Lycos",
  L"lycos.nl",
  L"http://zoek.lycos.nl/favicon.ico",
  L"http://zoek.lycos.nl/cgi-bin/pursuit?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  34,
};

const PrepopulatedEngine mail_ru = {
  L"@MAIL.RU",
  L"mail.ru",
  L"http://img.go.mail.ru/favicon.ico",
  L"http://go.mail.ru/search?q={searchTerms}",
  "windows-1251",
  NULL,
  83,
};

const PrepopulatedEngine maktoob = {
  L"\x0645\x0643\x062a\x0648\x0628",
  L"maktoob.com",
  L"http://www.maktoob.com/favicon.ico",
  L"http://www.maktoob.com/searchResult.php?q={searchTerms}",
  "UTF-8",
  NULL,
  13,
};

const PrepopulatedEngine masrawy = {
  L"\x0645\x0635\x0631\x0627\x0648\x064a",
  L"masrawy.com",
  L"http://www.masrawy.com/new/images/masrawy.ico",
  L"http://masrawy.com/new/search.aspx?sr={searchTerms}",
  "windows-1256",
  NULL,
  14,
};

const PrepopulatedEngine matkurja = {
  L"Mat'Kurja",
  L"matkurja.com",
  L"http://matkurja.com/favicon.ico",
  L"http://matkurja.com/si/iskalnik/?q={searchTerms}&search_source=directory",
  "ISO-8859-2",
  NULL,
  88,
};

const PrepopulatedEngine meta = {
  L"<META>",
  L"meta.ua",
  L"http://meta.ua/favicon.ico",
  L"http://meta.ua/search.asp?q={searchTerms}",
  "windows-1251",
  L"http://meta.ua/suggestions/?output=fxjson&oe=utf-8&q={searchTerms}",
  102,
};

const PrepopulatedEngine msn = {
  L"MSN",
  L"msn.com",
  L"http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_ar_XA = {
  L"MSN (\x0627\x0644\x0639\x0631\x0628\x064a\x0629)",
  L"",  // "arabia.msn.com" is already taken by msn_en_XA (see comment on ID
        // below).
  L"http://search.msn.com/s/wlflag.ico",
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
  L"http://search.msn.dk/s/wlflag.ico",
  L"http://search.msn.dk/results.aspx?mkt=da-DK&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_de_AT = {
  L"MSN \x00d6sterreich",
  L"at.msn.com",
  L"http://search.msn.at/s/wlflag.ico",
  L"http://search.msn.at/results.aspx?mkt=de-AT&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_de_CH = {
  L"MSN Schweiz (Deutsch)",
  L"ch.msn.com",
  L"http://search.msn.ch/s/wlflag.ico",
  L"http://search.msn.ch/results.aspx?setlang=de-CH&mkt=de-CH&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_de_DE = {
  L"MSN",
  L"de.msn.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=de-DE&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_AU = {
  L"ninemsn.com.au",
  L"ninemsn.com.au",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=en-AU&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_CA = {
  L"Sympatico / MSN (English)",
  L"sympatico.msn.ca",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=en-CA&mkt=en-CA&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_GB = {
  L"MSN UK",
  L"uk.msn.com",
  L"http://search.msn.co.uk/s/wlflag.ico",
  L"http://search.msn.co.uk/results.aspx?mkt=en-GB&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_IE = {
  L"MSN IE",
  L"ie.msn.com",
  L"http://search.msn.ie/s/wlflag.ico",
  L"http://search.msn.ie/results.aspx?mkt=en-IE&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_IN = {
  L"MSN India",
  L"in.msn.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=en-IN&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_MY = {
  L"MSN Malaysia",
  L"malaysia.msn.com",
  L"http://search.msn.com.my/s/wlflag.ico",
  L"http://search.msn.com.my/results.aspx?mkt=en-MY&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_PH = {
  L"MSN Philippines",
  L"ph.msn.com",
  L"http://search.msn.com.ph/s/wlflag.ico",
  L"http://search.msn.com.ph/results.aspx?mkt=en-PH&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_SG = {
  L"MSN Singapore",
  L"sg.msn.com",
  L"http://search.msn.com.sg/s/wlflag.ico",
  L"http://search.msn.com.sg/results.aspx?mkt=en-SG&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_XA = {
  L"MSN (English)",
  L"arabia.msn.com",
  L"http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?setlang=en-XA&mkt=en-XA&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_en_ZA = {
  L"MSN ZA",
  L"za.msn.com",
  L"http://search.msn.co.za/s/wlflag.ico",
  L"http://search.msn.co.za/results.aspx?mkt=en-ZA&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_AR = {
  L"MSN Argentina",
  L"ar.msn.com",
  L"http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?mkt=es-AR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_CL = {
  L"MSN Chile",
  L"cl.msn.com",
  L"http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?mkt=es-CL&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_CO = {
  L"MSN Colombia",
  L"co.msn.com",
  L"http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?mkt=es-CO&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_ES = {
  L"MSN Espa" L"\x00f1" L"a",
  L"es.msn.com",
  L"http://search.msn.es/s/wlflag.ico",
  L"http://search.msn.es/results.aspx?mkt=es-ES&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_MX = {
  L"Prodigy / MSN",
  L"prodigy.msn.com",
  L"http://search.prodigy.msn.com/s/wlflag.ico",
  L"http://search.prodigy.msn.com/results.aspx?mkt=es-MX&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_es_XL = {
  L"MSN Latinoam\x00e9rica",
  L"latam.msn.com",
  L"http://search.msn.com/s/wlflag.ico",
  L"http://search.msn.com/results.aspx?mkt=es-XL&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_fi_FI = {
  L"MSN",
  L"fi.msn.com",
  L"http://search.msn.fi/s/wlflag.ico",
  L"http://search.msn.fi/results.aspx?mkt=fi-FI&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_fr_BE = {
  L"MSN Belgique (Fran" L"\x00e7" L"ais)",
  L"",  // "be.msn.com" is already taken by msn_nl_BE (see comment on ID below).
  L"http://search.live.com/s/wlflag.ico",
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
  L"http://search.live.com/s/wlflag.ico",
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
  L"http://search.msn.ch/s/wlflag.ico",
  L"http://search.msn.ch/results.aspx?setlang=fr-CH&mkt=fr-CH&q={searchTerms}",
  "UTF-8",
  NULL,
  10,  // Can't be 3 as this has to appear in the Switzerland list alongside
       // msn_de_CH.
};

const PrepopulatedEngine msn_fr_FR = {
  L"MSN France",
  L"fr.msn.com",
  L"http://search.msn.fr/s/wlflag.ico",
  L"http://search.msn.fr/results.aspx?mkt=fr-FR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_he_IL = {
  L"msn.co.il",
  L"msn.co.il",
  L"http://msn.co.il/favicon.ico",
  L"http://search.msn.co.il/Search.aspx?q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_ja_JP = {
  L"MSN Japan",
  L"jp.msn.com",
  L"http://search.msn.co.jp/s/wlflag.ico",
  L"http://search.msn.co.jp/results.aspx?mkt=ja-JP&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_nb_NO = {
  L"MSN Norge",
  L"no.msn.com",
  L"http://search.msn.no/s/wlflag.ico",
  L"http://search.msn.no/results.aspx?mkt=nb-NO&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_nl_BE = {
  L"MSN (Nederlandstalige)",
  L"be.msn.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?setlang=nl-BE&mkt=nl-BE&"
      L"q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_nl_NL = {
  L"MSN.nl",
  L"nl.msn.com",
  L"http://search.msn.nl/s/wlflag.ico",
  L"http://search.msn.nl/results.aspx?mkt=nl-NL&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_pt_BR = {
  L"MSN Brasil",
  L"br.msn.com",
  L"http://search.live.com/s/wlflag.ico",
  L"http://search.live.com/results.aspx?mkt=pt-BR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_sv_SE = {
  L"MSN",
  L"se.msn.com",
  L"http://search.msn.se/s/wlflag.ico",
  L"http://search.msn.se/results.aspx?mkt=pv-SE&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_tr_TR = {
  L"MSN T\x00fckiye'ye",
  L"tr.msn.com",
  L"http://search.msn.com.tr/s/wlflag.ico",
  L"http://search.msn.com.tr/results.aspx?mkt=tr-TR&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine msn_zh_HK = {
  L"MSN Hong Kong",
  L"hk.msn.com",
  L"http://search.msn.com.hk/s/wlflag.ico",
  L"http://search.msn.com.hk/results.aspx?mkt=zh-HK&q={searchTerms}",
  "UTF-8",
  NULL,
  3,
};

const PrepopulatedEngine mweb = {
  L"MWEB",
  L"mweb.co.za",
  L"http://mweb.co.za/favicon.ico",
  L"http://search.mweb.co.za/search?&q={searchTerms}",
  "UTF-8",
  NULL,
  42,
};

const PrepopulatedEngine mynet = {
  L"MYNET",
  L"mynet.com",
  L"http://img.mynet.com/mynetfavori.ico",
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
  L"http://www.najdi.si/master/favicon.ico",
  L"http://www.najdi.si/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  87,
};

const PrepopulatedEngine nana10 = {
  L"\x05e0\x05e2\x05e0\x05e2 10",
  L"nana10.co.il",
  L"http://f.nau.co.il/Common/Includes/Favicon.ico",
  L"http://index.nana10.co.il/search.asp?q={searchTerms}",
  "windows-1255",
  NULL,
  56,
};

const PrepopulatedEngine nate = {
  L"\xb124\xc774\xd2b8\xb2f7\xcef4",
  L"nate.com",
  L"http://nate.search.empas.com/favicon.ico",
  L"http://nate.search.empas.com/search/all.html?q={searchTerms}",
  "EUC-KR",
  NULL,
  69,
};

const PrepopulatedEngine naver = {
  L"\xb124\xc774\xbc84",
  L"naver.com",
  L"http://search.naver.com/favicon.ico",
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
  L"http://www.neti.ee/favicon.ico",
  L"http://www.neti.ee/cgi-bin/otsing?query={searchTerms}",
  "ISO-8859-1",
  NULL,
  44,
};

const PrepopulatedEngine netindex = {
  L"NetINDEX",
  L"netindex.pt",
  L"http://www.netindex.pt/favicon.ico",
  L"http://www.netindex.pt/cgi-bin/index.cgi?question={searchTerms}",
  "ISO-8859-1",
  NULL,
  78,
};

const PrepopulatedEngine nifty = {
  L"@nifty",
  L"nifty.com",
  L"http://www.nifty.com/favicon.ico",
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
  L"http://ok.hu/gfx/favicon.ico",
  L"http://ok.hu/katalogus?q={searchTerms}",
  "ISO-8859-2",
  NULL,
  6,
};

const PrepopulatedEngine onet = {
  L"Onet.pl",
  L"onet.pl",
  L"http://szukaj.onet.pl/favicon.ico",
  L"http://szukaj.onet.pl/query.html?qt={searchTerms}",
  "ISO-8859-2",
  NULL,
  75,
};

const PrepopulatedEngine orange = {
  L"Orange",
  L"orange.fr",
  L"http://www.orange.fr/favicon.ico",
  L"http://rws.search.ke.voila.fr/RW/S/opensearch_orange?rdata={searchTerms}",
  "ISO-8859-1",
  L"http://search.ke.voila.fr/fr/cmplopensearch/xml/fullxml?"
  L"rdata={searchTerms}",
  48,
};

const PrepopulatedEngine ozu = {
  L"OZ\x00da",
  L"ozu.es",
  L"http://www.ozu.es/favicon.ico",
  L"http://buscar.ozu.es/index.php?q={searchTerms}",
  "ISO-8859-1",
  NULL,
  98,
};

const PrepopulatedEngine pogodak_ba = {
  L"Pogodak!",
  L"pogodak.ba",
  L"http://www.pogodak.ba/favicon.ico",
  L"http://www.pogodak.ba/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  24,
};

const PrepopulatedEngine pogodak_hr = {
  L"Pogodak!",
  L"pogodak.hr",
  L"http://www.pogodak.hr/favicon.ico",
  L"http://www.pogodak.hr/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  24,
};

const PrepopulatedEngine pogodak_rs = {
  L"Pogodak!",
  L"pogodak.rs",
  L"http://www.pogodak.rs/favicon.ico",
  L"http://www.pogodak.rs/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  24,
};

const PrepopulatedEngine pogodok = {
  L"\x041f\x043e\x0433\x043e\x0434\x043e\x043a!",
  L"pogodok.com.mk",
  L"http://www.pogodok.com.mk/favicon.ico",
  L"http://www.pogodok.com.mk/search.jsp?q={searchTerms}",
  "UTF-8",
  NULL,
  24,  // Really the same engine as Pogodak, just has a small name change.
};

const PrepopulatedEngine rambler = {
  L"Rambler",
  L"rambler.ru",
  L"http://www.rambler.ru/favicon.ico",
  L"http://www.rambler.ru/srch?words={searchTerms}",
  "windows-1251",
  NULL,
  16,
};

const PrepopulatedEngine rediff = {
  L"Rediff",
  L"rediff.com",
  L"http://search1.rediff.com/favicon.ico",
  L"http://search1.rediff.com/dirsrch/default.asp?MT={searchTerms}",
  "UTF-8",
  NULL,
  37,
};

const PrepopulatedEngine rednano = {
  L"Rednano",
  L"rednano.sg",
  L"http://rednano.sg/favicon.ico",
  L"http://rednano.sg/sfe/lwi.action?querystring={searchTerms}",
  "UTF-8",
  NULL,
  41,
};

const PrepopulatedEngine sanook = {
  L"\x0e2a\x0e19\x0e38\x0e01!",
  L"sanook.com",
  L"http://search.sanook.com/favicon.ico",
  L"http://search.sanook.com/search.php?q={searchTerms}",
  "UTF-8",
  NULL,
  100,
};

const PrepopulatedEngine sapo = {
  L"SAPO",
  L"sapo.pt",
  L"http://imgs.sapo.pt/images/sapo.ico",
  L"http://pesquisa.sapo.pt/?q={searchTerms}",
  "UTF-8",
  L"http://pesquisa.sapo.pt/livesapo?q={searchTerms}",
  77,
};

const PrepopulatedEngine search_ch = {
  L"search.ch",
  L"search.ch",
  L"http://www.search.ch/favicon.ico",
  L"http://www.search.ch/?q={searchTerms}",
  "ISO-8859-1",
  NULL,
  51,
};

const PrepopulatedEngine sensis = {
  L"sensis.com.au",
  L"sensis.com.au",
  L"http://www.sensis.com.au/favicon.ico",
  L"http://www.sensis.com.au/search.do?find={searchTerms}",
  "UTF-8",
  NULL,
  32,
};

const PrepopulatedEngine sesam = {
  L"Sesam",
  L"sesam.no",
  L"http://sesam.no/images/favicon.gif",
  L"http://sesam.no/search/?q={searchTerms}",
  "UTF-8",
  NULL,
  74,
};

const PrepopulatedEngine seznam = {
  L"Seznam",
  L"seznam.cz",
  L"http://1.im.cz/szn/img/favicon.ico",
  L"http://search.seznam.cz/?q={searchTerms}",
  "UTF-8",
  L"http:///suggest.fulltext.seznam.cz/?dict=fulltext_ff&phrase={searchTerms}&"
      L"encoding={inputEncoding}&response_encoding=utf-8",
  25,
};

const PrepopulatedEngine sogou = {
  L"\x641c\x72d7",
  L"sogou.com",
  L"http://www.sogou.com/favicon.ico",
  L"http://www.sogou.com/web?query={searchTerms}",
  "GB2312",
  NULL,
  20,
};

const PrepopulatedEngine soso = {
  L"\x641c\x641c",
  L"soso.com",
  L"http://www.soso.com/favicon.ico",
  L"http://www.soso.com/q?w={searchTerms}",
  "GB2312",
  NULL,
  22,
};

const PrepopulatedEngine spray = {
  L"Spray",
  L"spray.se",
  L"http://www.eniro.se/favicon.ico",
  L"http://www.eniro.se/query?ax=spray&search_word={searchTerms}&what=web",
  "ISO-8859-1",
  NULL,
  99,
};

const PrepopulatedEngine szm = {
  L"SZM.sk",
  L"szm.sk",
  L"http://szm.sk/favicon.ico",
  L"http://szm.sk/search/?co=1&q={searchTerms}",
  "windows-1250",
  NULL,
  86,
};

const PrepopulatedEngine t_online = {
  L"T-Online",
  L"suche.t-online.de",
  L"http://suche.t-online.de/favicon.ico",
  L"http://suche.t-online.de/fast-cgi/tsc?sr=chrome&q={searchTerms}",
  "UTF-8",
  NULL,
  49,
};

const PrepopulatedEngine tango = {
  L"Tango",
  L"tango.hu",
  L"http://tango.hu/favicon.ico",
  L"http://tango.hu/search.php?q={searchTerms}",
  "windows-1250",
  NULL,
  58,
};

const PrepopulatedEngine tapuz = {
  L"\x05ea\x05e4\x05d5\x05d6 \x05d0\x05e0\x05e9\x05d9\x05dd",
  L"tapuz.co.il",
  L"http://www.tapuz.co.il/favicon.ico",
  L"http://www.tapuz.co.il/search/search.asp?q={searchTerms}",
  "windows-1255",
  NULL,
  57,
};

const PrepopulatedEngine terra_ar = {
  L"Terra Argentina",
  L"terra.com.ar",
  L"http://buscar.terra.com.ar/favicon.ico",
  L"http://buscar.terra.com.ar/Default.aspx?query={searchTerms}&source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine terra_ec = {
  L"Terra Ecuador",
  L"terra.com.ec",
  L"http://buscador.terra.com.ec/favicon.ico",
  L"http://buscador.terra.com.ec/Default.aspx?query={searchTerms}&"
      L"source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine terra_es = {
  L"Terra",
  L"terra.es",
  L"http://buscador.terra.es/favicon.ico",
  L"http://buscador.terra.es/Default.aspx?query={searchTerms}&source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine terra_mx = {
  L"Terra",
  L"terra.com.mx",
  L"http://buscador.terra.com.mx/favicon.ico",
  L"http://buscador.terra.com.mx/Default.aspx?query={searchTerms}&"
      L"source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine terra_pe = {
  L"Terra",
  L"terra.com.pe",
  L"http://buscador.terra.com.pe/favicon.ico",
  L"http://buscador.terra.com.pe/Default.aspx?query={searchTerms}&"
      L"source=Search",
  "ISO-8859-1",
  NULL,
  90,
};

const PrepopulatedEngine toile = {
  L"La Toile du Qu" L"\x00e9" L"bec",
  L"toile.com",
  L"http://static.search.canoe.ca/s-toile/img/favicon_toile.ico",
  L"http://www.toile.com/search?q={searchTerms}",
  "UTF-8",
  NULL,
  36,
};

const PrepopulatedEngine tut = {
  L"TUT.BY",
  L"tut.by",
  L"http://www.tut.by/favicon.ico",
  L"http://search.tut.by/?query={searchTerms}",
  "windows-1251",
  NULL,
  17,
};

const PrepopulatedEngine uol = {
  L"UOL Busca",
  L"busca.uol.com.br",
  L"http://busca.uol.com.br/favicon.ico",
  L"http://busca.uol.com.br/www/index.html?q={searchTerms}",
  "ISO-8859-1",
  NULL,
  82,
};

const PrepopulatedEngine vinden = {
  L"Vinden.nl",
  L"vinden.nl",
  L"http://www.vinden.nl/favicon.ico",
  L"http://www.vinden.nl/?q={searchTerms}",
  "UTF-8",
  NULL,
  31,
};

const PrepopulatedEngine virgilio = {
  L"Virgilio",
  L"virgilio.alice.it",
  L"http://ricerca.alice.it/favicon.ico",
  L"http://ricerca.alice.it/ricerca?qs={searchTerms}",
  "ISO-8859-1",
  NULL,
  62,
};

const PrepopulatedEngine voila = {
  L"Voila",
  L"voila.fr",
  L"http://search.ke.voila.fr/favicon.ico",
  L"http://rws.search.ke.voila.fr/RW/S/opensearch_voila?rdata={searchTerms}",
  "ISO-8859-1",
  L"http://search.ke.voila.fr/fr/cmplopensearch/xml/fullxml?"
  L"rdata={searchTerms}",
  47,
};

const PrepopulatedEngine walla = {
  L"\x05d5\x05d5\x05d0\x05dc\x05d4!",
  L"walla.co.il",
  L"http://www.walla.co.il/favicon.ico",
  L"http://search.walla.co.il/?e=hew&q={searchTerms}",
  "windows-1255",
  NULL,
  55,
};

const PrepopulatedEngine web_de = {
  L"WEB.DE",
  L"web.de",
  L"http://img.ui-portal.de/search/img/webde/favicon.ico",
  L"http://suche.web.de/search/web/?su={searchTerms}",
  "ISO-8859-1",
  NULL,
  50,
};

const PrepopulatedEngine wp = {
  L"Wirtualna Polska",
  L"wp.pl",
  L"http://szukaj.wp.pl/favicon.ico",
  L"http://szukaj.wp.pl/szukaj.html?szukaj={searchTerms}",
  "ISO-8859-2",
  NULL,
  76,
};

const PrepopulatedEngine yagua = {
  L"Yagua.com",
  L"yagua.com",
  L"http://yagua.paraguay.com/favicon.ico",
  L"http://yagua.paraguay.com/buscador.php?q={searchTerms}&cs={inputEncoding}",
  "ISO-8859-1",
  NULL,
  94,
};

const PrepopulatedEngine yahoo = {
  L"Yahoo!",
  L"yahoo.com",
  L"http://search.yahoo.com/favicon.ico",
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
  L"http://ar.search.yahoo.com/favicon.ico",
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
  L"http://at.search.yahoo.com/favicon.ico",
  L"http://at.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_au = {
  L"Yahoo!7",
  L"au.yahoo.com",
  L"http://au.search.yahoo.com/favicon.ico",
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
  L"http://br.search.yahoo.com/favicon.ico",
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
  L"http://ca.search.yahoo.com/favicon.ico",
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
  L"http://ch.search.yahoo.com/favicon.ico",
  L"http://ch.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_cl = {
  L"Yahoo! Chile",
  L"cl.yahoo.com",
  L"http://cl.search.yahoo.com/favicon.ico",
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
  L"http://search.cn.yahoo.com/favicon.ico",
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
  L"http://co.search.yahoo.com/favicon.ico",
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
  L"http://de.search.yahoo.com/favicon.ico",
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
  L"http://dk.search.yahoo.com/favicon.ico",
  L"http://dk.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_es = {
  L"Yahoo! Espa" L"\x00f1" L"a",
  L"es.yahoo.com",
  L"http://es.search.yahoo.com/favicon.ico",
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
  L"http://fi.search.yahoo.com/favicon.ico",
  L"http://fi.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_fr = {
  L"Yahoo! France",
  L"fr.yahoo.com",
  L"http://fr.search.yahoo.com/favicon.ico",
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
  L"http://hk.search.yahoo.com/favicon.ico",
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
  L"http://id.search.yahoo.com/favicon.ico",
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
  L"http://in.search.yahoo.com/favicon.ico",
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
  L"http://it.search.yahoo.com/favicon.ico",
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
  L"http://search.yahoo.co.jp/favicon.ico",
  L"http://search.yahoo.co.jp/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_kr = {
  L"\xc57c\xd6c4! \xcf54\xb9ac\xc544",
  L"kr.yahoo.com",
  L"http://kr.search.yahoo.com/favicon.ico",
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
  L"http://malaysia.search.yahoo.com/favicon.ico",
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
  L"http://mx.search.yahoo.com/favicon.ico",
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
  L"http://nl.search.yahoo.com/favicon.ico",
  L"http://nl.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_no = {
  L"Yahoo! Norge",
  L"no.yahoo.com",
  L"http://no.search.yahoo.com/favicon.ico",
  L"http://no.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_nz = {
  L"Yahoo!Xtra",
  L"nz.yahoo.com",
  L"http://nz.search.yahoo.com/favicon.ico",
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
  L"http://pe.search.yahoo.com/favicon.ico",
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
  L"http://ph.search.yahoo.com/favicon.ico",
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
  L"http://qc.search.yahoo.com/favicon.ico",
  L"http://qc.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  5,  // Can't be 2 as this has to appear in the Canada list alongside yahoo_ca.
};

const PrepopulatedEngine yahoo_ru = {
  L"Yahoo! \x043f\x043e-\x0440\x0443\x0441\x0441\x043a\x0438",
  L"ru.yahoo.com",
  L"http://ru.search.yahoo.com/favicon.ico",
  L"http://ru.search.yahoo.com/search?ei={inputEncoding}&fr=crmas&"
      L"p={searchTerms}",
  "UTF-8",
  NULL,
  2,
};

const PrepopulatedEngine yahoo_sg = {
  L"Yahoo! Singapore",
  L"sg.yahoo.com",
  L"http://sg.search.yahoo.com/favicon.ico",
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
  L"http://th.search.yahoo.com/favicon.ico",
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
  L"http://tw.search.yahoo.com/favicon.ico",
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
  L"http://uk.search.yahoo.com/favicon.ico",
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
  L"http://ve.search.yahoo.com/favicon.ico",
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
  L"http://vn.search.yahoo.com/favicon.ico",
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
  L"http://www.yam.com/i/8/sky.ico",
  L"http://search.yam.com/wps?k={searchTerms}",
  "Big5",
  NULL,
  23,
};

const PrepopulatedEngine yamli = {
  L"Yamli",
  L"yamli.com",
  L"http://www.yamli.com/favicon.ico",
  L"http://www.yamli.com/#q={searchTerms}",
  "UTF-8",
  NULL,
  11,
};

const PrepopulatedEngine yandex_ru = {
  L"\x042f\x043d\x0434\x0435\x043a\x0441",
  L"yandex.ru",
  L"http://yandex.ru/favicon.ico",
  L"http://yandex.ru/yandsearch?text={searchTerms}",
  "UTF-8",
  L"http://suggest.yandex.net/suggest-ff.cgi?part={searchTerms}",
  15,
};

const PrepopulatedEngine yandex_ua = {
  L"\x042f\x043d\x0434\x0435\x043a\x0441",
  L"yandex.ua",
  L"http://yandex.ua/favicon.ico",
  L"http://yandex.ua/yandsearch?text={searchTerms}",
  "UTF-8",
  L"http://suggest.yandex.net/suggest-ff.cgi?part={searchTerms}",
  15,
};

const PrepopulatedEngine zoznam = {
  L"Zoznam",
  L"zoznam.sk",
  L"http://zoznam.sk/favicon.ico",
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

// Serbia/Montenegro
const PrepopulatedEngine* engines_RS_ME[] =
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

// United Kingdom
const PrepopulatedEngine* engines_UK[] =
    { &google, &yahoo_uk, &msn_en_GB, &ask_uk, };

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

// GeoID mappings //////////////////////////////////////////////////////////////

LONG GetCurrentGeoID() {
  // TODO(pkasting): http://b/1225276  Much of this should live in a utility
  // function somewhere.
  typedef GEOID (WINAPI *GetUserGeoIDFunction)(GEOCLASS);
  const HMODULE kernel32_handle = GetModuleHandle(L"kernel32.dll");
  if (!kernel32_handle) {
    NOTREACHED();
    return GEOID_NOT_AVAILABLE;
  }
  const GetUserGeoIDFunction GetUserGeoIDPtr =
      reinterpret_cast<GetUserGeoIDFunction>(GetProcAddress(kernel32_handle,
                                                            "GetUserGeoID"));
  return GetUserGeoIDPtr ?
      ((*GetUserGeoIDPtr)(GEOCLASS_NATION)) : GEOID_NOT_AVAILABLE;
}

int GetGeoIDFromPrefs(PrefService* prefs) {
  // See if the user overrode the GeoID on the command line.
  CommandLine parsed_command_line;
  const std::wstring geoID(
      parsed_command_line.GetSwitchValue(switches::kGeoID));
  if (!geoID.empty())
    return _wtoi(geoID.c_str());

  // Cache first run GeoID value in prefs, and use it afterwards.  This ensures
  // that just because the user moves around, we won't automatically make major
  // changes to their available search providers, which would feel surprising.
  if (!prefs)
    return GetCurrentGeoID();
  if (!prefs->HasPrefPath(prefs::kGeoIDAtInstall))
    prefs->SetInteger(prefs::kGeoIDAtInstall, GetCurrentGeoID());
  return prefs->GetInteger(prefs::kGeoIDAtInstall);
}

void GetPrepopulationSetFromGeoID(PrefService* prefs,
                                  const PrepopulatedEngine*** engines,
                                  size_t* num_engines) {
  // NOTE: This function should ALWAYS set its outparams.

  // If you add a new geo id make sure and update the unit test for coverage.

  // GeoIDs are from http://msdn.microsoft.com/en-us/library/ms776390.aspx .
  // Country codes and names are from http://www.geonames.org/countries/ .
  switch (GetGeoIDFromPrefs(prefs)) {

#define UNHANDLED_COUNTRY(id, code)\
    case id:
#define END_UNHANDLED_COUNTRIES(code)\
      *engines = engines_##code;\
      *num_engines = arraysize(engines_##code);\
      return;
#define DECLARE_COUNTRY(id, code)\
    UNHANDLED_COUNTRY(id, code)\
    END_UNHANDLED_COUNTRIES(code)

    // Countries with their own, dedicated engine set.
    DECLARE_COUNTRY(0x4,    DZ)     // Algeria
    DECLARE_COUNTRY(0x6,    AL)     // Albania
    DECLARE_COUNTRY(0xB,    AR)     // Argentina
    DECLARE_COUNTRY(0xC,    AU)     // Australia
    DECLARE_COUNTRY(0xE,    AT)     // Austria
    DECLARE_COUNTRY(0x11,   BH)     // Bahrain
    DECLARE_COUNTRY(0x15,   BE)     // Belgium
    DECLARE_COUNTRY(0x18,   BZ)     // Belize
    DECLARE_COUNTRY(0x19,   BA)     // Bosnia and Herzegovina
    DECLARE_COUNTRY(0x1A,   BO)     // Bolivia
    DECLARE_COUNTRY(0x1D,   BY)     // Belarus
    DECLARE_COUNTRY(0x20,   BR)     // Brazil
    DECLARE_COUNTRY(0x23,   BG)     // Bulgaria
    DECLARE_COUNTRY(0x25,   BN)     // Brunei
    DECLARE_COUNTRY(0x27,   CA)     // Canada
    DECLARE_COUNTRY(0x2D,   CN)     // China
    DECLARE_COUNTRY(0x2E,   CL)     // Chile
    DECLARE_COUNTRY(0x33,   CO)     // Colombia
    DECLARE_COUNTRY(0x36,   CR)     // Costa Rica
    DECLARE_COUNTRY(0x3D,   DK)     // Denmark
    DECLARE_COUNTRY(0x41,   DO)     // Dominican Republic
    DECLARE_COUNTRY(0x42,   EC)     // Ecuador
    DECLARE_COUNTRY(0x43,   EG)     // Egypt
    DECLARE_COUNTRY(0x44,   IE)     // Ireland
    DECLARE_COUNTRY(0x46,   EE)     // Estonia
    DECLARE_COUNTRY(0x48,   SV)     // El Salvador
    DECLARE_COUNTRY(0x4B,   CZ)     // Czech Republic
    DECLARE_COUNTRY(0x4D,   FI)     // Finland
    DECLARE_COUNTRY(0x51,   FO)     // Faroe Islands
    DECLARE_COUNTRY(0x54,   FR)     // France
    DECLARE_COUNTRY(0x5E,   DE)     // Germany
    DECLARE_COUNTRY(0x62,   GR)     // Greece
    DECLARE_COUNTRY(0x63,   GT)     // Guatemala
    DECLARE_COUNTRY(0x68,   HK)     // Hong Kong
    DECLARE_COUNTRY(0x6A,   HN)     // Honduras
    DECLARE_COUNTRY(0x6C,   HR)     // Croatia
    DECLARE_COUNTRY(0x6D,   HU)     // Hungary
    DECLARE_COUNTRY(0x6E,   IS)     // Iceland
    DECLARE_COUNTRY(0x6F,   ID)     // Indonesia
    DECLARE_COUNTRY(0x71,   IN)     // India
    DECLARE_COUNTRY(0x74,   IR)     // Iran
    DECLARE_COUNTRY(0x75,   IL)     // Israel
    DECLARE_COUNTRY(0x76,   IT)     // Italy
    DECLARE_COUNTRY(0x79,   IQ)     // Iraq
    DECLARE_COUNTRY(0x7A,   JP)     // Japan
    DECLARE_COUNTRY(0x7C,   JM)     // Jamaica
    DECLARE_COUNTRY(0x7E,   JO)     // Jordan
    DECLARE_COUNTRY(0x81,   KE)     // Kenya
    DECLARE_COUNTRY(0x86,   KR)     // South Korea
    DECLARE_COUNTRY(0x88,   KW)     // Kuwait
    DECLARE_COUNTRY(0x8B,   LB)     // Lebanon
    DECLARE_COUNTRY(0x8C,   LV)     // Latvia
    DECLARE_COUNTRY(0x8D,   LT)     // Lithuania
    DECLARE_COUNTRY(0x8F,   SK)     // Slovakia
    DECLARE_COUNTRY(0x91,   LI)     // Liechtenstein
    DECLARE_COUNTRY(0x93,   LU)     // Luxembourg
    DECLARE_COUNTRY(0x94,   LY)     // Libya
    DECLARE_COUNTRY(0x9E,   MC)     // Monaco
    DECLARE_COUNTRY(0x9F,   MA)     // Morocco
    DECLARE_COUNTRY(0xA4,   OM)     // Oman
    DECLARE_COUNTRY(0xA6,   MX)     // Mexico
    DECLARE_COUNTRY(0xA7,   MY)     // Malaysia
    DECLARE_COUNTRY(0xB0,   NL)     // Netherlands
    DECLARE_COUNTRY(0xB1,   NO)     // Norway
    DECLARE_COUNTRY(0xB6,   NI)     // Nicaragua
    DECLARE_COUNTRY(0xB7,   NZ)     // New Zealand
    DECLARE_COUNTRY(0xB9,   PY)     // Paraguay
    DECLARE_COUNTRY(0xBB,   PE)     // Peru
    DECLARE_COUNTRY(0xBE,   PK)     // Pakistan
    DECLARE_COUNTRY(0xBF,   PL)     // Poland
    DECLARE_COUNTRY(0xC0,   PA)     // Panama
    DECLARE_COUNTRY(0xC1,   PT)     // Portugal
    DECLARE_COUNTRY(0xC5,   QA)     // Qatar
    DECLARE_COUNTRY(0xC8,   RO)     // Romania
    DECLARE_COUNTRY(0xC9,   PH)     // Philippines
    DECLARE_COUNTRY(0xCA,   PR)     // Puerto Rico
    DECLARE_COUNTRY(0xCB,   RU)     // Russia
    DECLARE_COUNTRY(0xCD,   SA)     // Saudi Arabia
    DECLARE_COUNTRY(0xD1,   ZA)     // South Africa
    DECLARE_COUNTRY(0xD4,   SI)     // Slovenia
    DECLARE_COUNTRY(0xD7,   SG)     // Singapore
    DECLARE_COUNTRY(0xD9,   ES)     // Spain
    DECLARE_COUNTRY(0xDD,   SE)     // Sweden
    DECLARE_COUNTRY(0xDE,   SY)     // Syria
    DECLARE_COUNTRY(0xDF,   CH)     // Switzerland
    DECLARE_COUNTRY(0xE0,   AE)     // United Arab Emirates
    DECLARE_COUNTRY(0xE1,   TT)     // Trinidad and Tobago
    DECLARE_COUNTRY(0xE3,   TH)     // Thailand
    DECLARE_COUNTRY(0xEA,   TN)     // Tunisia
    DECLARE_COUNTRY(0xEB,   TR)     // Turkey
    DECLARE_COUNTRY(0xED,   TW)     // Taiwan
    DECLARE_COUNTRY(0xF1,   UA)     // Ukraine
    DECLARE_COUNTRY(0xF2,   UK)     // United Kingdom
    DECLARE_COUNTRY(0xF4,   US)     // United States
    DECLARE_COUNTRY(0xF6,   UY)     // Uruguay
    DECLARE_COUNTRY(0xF9,   VE)     // Venezuela
    DECLARE_COUNTRY(0xFB,   VN)     // Vietnam
    DECLARE_COUNTRY(0x105,  YE)     // Yemen
    DECLARE_COUNTRY(0x108,  ZW)     // Zimbabwe
    DECLARE_COUNTRY(0x10D,  RS_ME)  // Serbia/Montenegro
    DECLARE_COUNTRY(0x4CA2, MK)     // Macedonia

    // Countries using the "Australia" engine set.
    UNHANDLED_COUNTRY(0x130, XX)  // Ashmore and Cartier Islands
    UNHANDLED_COUNTRY(0x135, CX)  // Christmas Island
    UNHANDLED_COUNTRY(0x137, CC)  // Cocos Islands
    UNHANDLED_COUNTRY(0x139, XX)  // Coral Sea Islands
    UNHANDLED_COUNTRY(0x145, HM)  // Heard Island and McDonald Islands
    UNHANDLED_COUNTRY(0x150, NF)  // Norfolk Island
    END_UNHANDLED_COUNTRIES(AU)

    // Countries using the "China" engine set.
    UNHANDLED_COUNTRY(0x97, MO)  // Macao
    END_UNHANDLED_COUNTRIES(CN)

    // Countries using the "Denmark" engine set.
    UNHANDLED_COUNTRY(0x5D, GL)  // Greenland
    END_UNHANDLED_COUNTRIES(DK)

    // Countries using the "Spain" engine set.
    UNHANDLED_COUNTRY(0x8, AD)  // Andorra
    END_UNHANDLED_COUNTRIES(ES)

    // Countries using the "France" engine set.
    UNHANDLED_COUNTRY(0x1C,  BJ)  // Benin
    UNHANDLED_COUNTRY(0x26,  BI)  // Burundi
    UNHANDLED_COUNTRY(0x29,  TD)  // Chad
    UNHANDLED_COUNTRY(0x2B,  CG)  // Congo - Brazzaville
    UNHANDLED_COUNTRY(0x2C,  CD)  // Congo - Kinshasa
    UNHANDLED_COUNTRY(0x31,  CM)  // Cameroon
    UNHANDLED_COUNTRY(0x37,  CF)  // Central African Republic
    UNHANDLED_COUNTRY(0x3E,  DJ)  // Djibouti
    UNHANDLED_COUNTRY(0x57,  GA)  // Gabon
    UNHANDLED_COUNTRY(0x64,  GN)  // Guinea
    UNHANDLED_COUNTRY(0x67,  HT)  // Haiti
    UNHANDLED_COUNTRY(0x77,  CI)  // Ivory Coast
    UNHANDLED_COUNTRY(0x9D,  ML)  // Mali
    UNHANDLED_COUNTRY(0xAD,  NE)  // Niger
    UNHANDLED_COUNTRY(0xC6,  RE)  // Reunion
    UNHANDLED_COUNTRY(0xCE,  PM)  // Saint Pierre and Miquelon
    UNHANDLED_COUNTRY(0xD2,  SN)  // Senegal
    UNHANDLED_COUNTRY(0xE8,  TG)  // Togo
    UNHANDLED_COUNTRY(0xF5,  BF)  // Burkina Faso
    UNHANDLED_COUNTRY(0x136, XX)  // Clipperton Island
    UNHANDLED_COUNTRY(0x13D, GF)  // French Guiana
    UNHANDLED_COUNTRY(0x13E, PF)  // French Polynesia
    UNHANDLED_COUNTRY(0x13F, TF)  // French Southern Territories
    UNHANDLED_COUNTRY(0x141, GP)  // Guadeloupe
    UNHANDLED_COUNTRY(0x14A, MQ)  // Martinique
    UNHANDLED_COUNTRY(0x14B, YT)  // Mayotte
    UNHANDLED_COUNTRY(0x14E, NC)  // New Caledonia
    UNHANDLED_COUNTRY(0x160, WF)  // Wallis and Futuna
    END_UNHANDLED_COUNTRIES(FR)

    // Countries using the "Greece" engine set.
    UNHANDLED_COUNTRY(0x3B, CY)  // Cyprus
    END_UNHANDLED_COUNTRIES(GR)

    // Countries using the "Italy" engine set.
    UNHANDLED_COUNTRY(0xD6, SM)  // San Marino
    UNHANDLED_COUNTRY(0xFD, VA)  // Vatican
    END_UNHANDLED_COUNTRIES(IT)

    // Countries using the "Netherlands" engine set.
    UNHANDLED_COUNTRY(0x12E, AW)  // Aruba
    UNHANDLED_COUNTRY(0x14D, AN)  // Netherlands Antilles
    END_UNHANDLED_COUNTRIES(NL)

    // Countries using the "Norway" engine set.
    UNHANDLED_COUNTRY(0x7D,  SJ)  // [Svalbard and] Jan Mayen
    UNHANDLED_COUNTRY(0xDC,  SJ)  // Svalbard [and Jan Mayen]
    UNHANDLED_COUNTRY(0x132, BV)  // Bouvet Island
    END_UNHANDLED_COUNTRIES(NO)

    // Countries using the "New Zealand" engine set.
    UNHANDLED_COUNTRY(0x138, CK)  // Cook Islands
    UNHANDLED_COUNTRY(0x14F, NU)  // Niue
    UNHANDLED_COUNTRY(0x15B, TK)  // Tokelau
    END_UNHANDLED_COUNTRIES(NZ)

    // Countries using the "Portugal" engine set.
    UNHANDLED_COUNTRY(0x39,     CV)  // Cape Verde
    UNHANDLED_COUNTRY(0xA8,     MZ)  // Mozambique
    UNHANDLED_COUNTRY(0xC4,     GW)  // Guinea-Bissau
    UNHANDLED_COUNTRY(0xE9,     ST)  // Sao Tome and Principe
    UNHANDLED_COUNTRY(0x6F60E7, TL)  // East Timor
    END_UNHANDLED_COUNTRIES(PT)

    // Countries using the "Russia" engine set.
    UNHANDLED_COUNTRY(0x5,  AZ)  // Azerbaijan
    UNHANDLED_COUNTRY(0x7,  AM)  // Armenia
    UNHANDLED_COUNTRY(0x82, KG)  // Kyrgyzstan
    UNHANDLED_COUNTRY(0x89, KZ)  // Kazakhstan
    UNHANDLED_COUNTRY(0xE4, TJ)  // Tajikistan
    UNHANDLED_COUNTRY(0xEE, TM)  // Turkmenistan
    UNHANDLED_COUNTRY(0xF7, UZ)  // Uzbekistan
    END_UNHANDLED_COUNTRIES(RU)

    // Countries using the "Saudi Arabia" engine set.
    UNHANDLED_COUNTRY(0xA2, MR)  // Mauritania
    UNHANDLED_COUNTRY(0xB8, PS)  // Palestinian Territory
    UNHANDLED_COUNTRY(0xDB, SD)  // Sudan
    END_UNHANDLED_COUNTRIES(SA)

    // Countries using the "United Kingdom" engine set.
    UNHANDLED_COUNTRY(0x14,   BM)  // Bermuda
    UNHANDLED_COUNTRY(0x5A,   GI)  // Gibraltar
    UNHANDLED_COUNTRY(0x72,   IO)  // British Indian Ocean Territory
    UNHANDLED_COUNTRY(0xA3,   MT)  // Malta
    UNHANDLED_COUNTRY(0x12F,  XX)  // Ascension Island
    UNHANDLED_COUNTRY(0x133,  KY)  // Cayman Islands
    UNHANDLED_COUNTRY(0x134,  XX)  // Channel Islands
    UNHANDLED_COUNTRY(0x13A,  XX)  // Diego Garcia
    UNHANDLED_COUNTRY(0x13B,  FK)  // Falkland Islands
    UNHANDLED_COUNTRY(0x144,  GG)  // Guernsey
    UNHANDLED_COUNTRY(0x148,  JE)  // Jersey
    UNHANDLED_COUNTRY(0x14C,  MS)  // Montserrat
    UNHANDLED_COUNTRY(0x153,  PN)  // Pitcairn Islands
    UNHANDLED_COUNTRY(0x156,  GS)  // South Georgia and the South Sandwich
                                   // Islands
    UNHANDLED_COUNTRY(0x157,  SH)  // Saint Helena
    UNHANDLED_COUNTRY(0x15C,  XX)  // Tristan da Cunha
    UNHANDLED_COUNTRY(0x15D,  TC)  // Turks and Caicos Islands
    UNHANDLED_COUNTRY(0x15F,  VG)  // British Virgin Islands
    UNHANDLED_COUNTRY(0x3B16, IM)  // Isle of Man
    END_UNHANDLED_COUNTRIES(UK)

    // Countries using the "United States" engine set.
    UNHANDLED_COUNTRY(0xA,    AS)  // American Samoa
    UNHANDLED_COUNTRY(0x7F,   XX)  // Johnston Atoll
    UNHANDLED_COUNTRY(0xFC,   VI)  // U.S. Virgin Islands
    UNHANDLED_COUNTRY(0x102,  XX)  // Wake Island
    UNHANDLED_COUNTRY(0x131,  XX)  // Baker Island
    UNHANDLED_COUNTRY(0x142,  GU)  // Guam
    UNHANDLED_COUNTRY(0x146,  XX)  // Howland Island
    UNHANDLED_COUNTRY(0x147,  XX)  // Jarvis Island
    UNHANDLED_COUNTRY(0x149,  XX)  // Kingman Reef
    UNHANDLED_COUNTRY(0x151,  MP)  // Northern Mariana Islands
    UNHANDLED_COUNTRY(0x152,  XX)  // Palmyra Atoll
    UNHANDLED_COUNTRY(0x154,  XX)  // Rota Island
    UNHANDLED_COUNTRY(0x155,  XX)  // Saipan
    UNHANDLED_COUNTRY(0x15A,  XX)  // Tinian Island
    UNHANDLED_COUNTRY(0x52FA, XX)  // Midway Islands
    END_UNHANDLED_COUNTRIES(US)

    // Countries using the "default" engine set.
    UNHANDLED_COUNTRY(0x2,                 AG)  // Antigua and Barbuda
    UNHANDLED_COUNTRY(0x3,                 AF)  // Afghanistan
    UNHANDLED_COUNTRY(0x9,                 AO)  // Angola
    UNHANDLED_COUNTRY(0x12,                BB)  // Barbados
    UNHANDLED_COUNTRY(0x13,                BW)  // Botswana
    UNHANDLED_COUNTRY(0x16,                BS)  // Bahamas
    UNHANDLED_COUNTRY(0x17,                BD)  // Bangladesh
    UNHANDLED_COUNTRY(0x1B,                MM)  // Myanmar
    UNHANDLED_COUNTRY(0x1E,                SB)  // Solomon Islands
    UNHANDLED_COUNTRY(0x22,                BT)  // Bhutan
    UNHANDLED_COUNTRY(0x28,                KH)  // Cambodia
    UNHANDLED_COUNTRY(0x2A,                LK)  // Sri Lanka
    UNHANDLED_COUNTRY(0x32,                KM)  // Comoros
    UNHANDLED_COUNTRY(0x38,                CU)  // Cuba
    UNHANDLED_COUNTRY(0x3F,                DM)  // Dominica
    UNHANDLED_COUNTRY(0x45,                GQ)  // Equatorial Guinea
    UNHANDLED_COUNTRY(0x47,                ER)  // Eritrea
    UNHANDLED_COUNTRY(0x49,                ET)  // Ethiopia
    UNHANDLED_COUNTRY(0x4E,                FJ)  // Fiji
    UNHANDLED_COUNTRY(0x50,                FM)  // Micronesia
    UNHANDLED_COUNTRY(0x56,                GM)  // Gambia
    UNHANDLED_COUNTRY(0x58,                GE)  // Georgia
    UNHANDLED_COUNTRY(0x59,                GH)  // Ghana
    UNHANDLED_COUNTRY(0x5B,                GD)  // Grenada
    UNHANDLED_COUNTRY(0x65,                GY)  // Guyana
    UNHANDLED_COUNTRY(0x83,                KP)  // North Korea
    UNHANDLED_COUNTRY(0x85,                KI)  // Kiribati
    UNHANDLED_COUNTRY(0x8A,                LA)  // Laos
    UNHANDLED_COUNTRY(0x8E,                LR)  // Liberia
    UNHANDLED_COUNTRY(0x92,                LS)  // Lesotho
    UNHANDLED_COUNTRY(0x95,                MG)  // Madagascar
    UNHANDLED_COUNTRY(0x98,                MD)  // Moldova
    UNHANDLED_COUNTRY(0x9A,                MN)  // Mongolia
    UNHANDLED_COUNTRY(0x9C,                MW)  // Malawi
    UNHANDLED_COUNTRY(0xA0,                MU)  // Mauritius
    UNHANDLED_COUNTRY(0xA5,                MV)  // Maldives
    UNHANDLED_COUNTRY(0xAE,                VU)  // Vanuatu
    UNHANDLED_COUNTRY(0xAF,                NG)  // Nigeria
    UNHANDLED_COUNTRY(0xB2,                NP)  // Nepal
    UNHANDLED_COUNTRY(0xB4,                NR)  // Nauru
    UNHANDLED_COUNTRY(0xB5,                SR)  // Suriname
    UNHANDLED_COUNTRY(0xC2,                PG)  // Papua New Guinea
    UNHANDLED_COUNTRY(0xC3,                PW)  // Palau
    UNHANDLED_COUNTRY(0xC7,                MH)  // Marshall Islands
    UNHANDLED_COUNTRY(0xCC,                RW)  // Rwanda
    UNHANDLED_COUNTRY(0xCF,                KN)  // Saint Kitts and Nevis
    UNHANDLED_COUNTRY(0xD0,                SC)  // Seychelles
    UNHANDLED_COUNTRY(0xD5,                SL)  // Sierra Leone
    UNHANDLED_COUNTRY(0xD8,                SO)  // Somalia
    UNHANDLED_COUNTRY(0xDA,                LC)  // Saint Lucia
    UNHANDLED_COUNTRY(0xE7,                TO)  // Tonga
    UNHANDLED_COUNTRY(0xEC,                TV)  // Tuvalu
    UNHANDLED_COUNTRY(0xEF,                TZ)  // Tanzania
    UNHANDLED_COUNTRY(0xF0,                UG)  // Uganda
    UNHANDLED_COUNTRY(0xF8,                VC)  // Saint Vincent and the
                                                // Grenadines
    UNHANDLED_COUNTRY(0xFE,                NA)  // Namibia
    UNHANDLED_COUNTRY(0x103,               WS)  // Samoa
    UNHANDLED_COUNTRY(0x104,               SZ)  // Swaziland
    UNHANDLED_COUNTRY(0x107,               ZM)  // Zambia
    UNHANDLED_COUNTRY(0x12C,               AI)  // Anguilla
    UNHANDLED_COUNTRY(0x12D,               AQ)  // Antarctica
    UNHANDLED_COUNTRY(0x143,               XX)  // Guantanamo Bay
    UNHANDLED_COUNTRY(GEOID_NOT_AVAILABLE, XX)  // Unknown location
    default:                                    // Unhandled location
    END_UNHANDLED_COUNTRIES(default)
  }
}

}  // namespace

namespace TemplateURLPrepopulateData {

void RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kGeoIDAtInstall, -1);
}

int GetDataVersion() {
  return 19;  // Increment this if you change the above data in ways that mean
              // users with existing data should get a new version.
}

void GetPrepopulatedEngines(PrefService* prefs,
                            std::vector<TemplateURL*>* t_urls,
                            size_t* default_search_provider_index) {
  // TODO(pkasting): http://b/1225464 GeoID is not available on Win2k.  We'll
  // need to do something else there.
  const PrepopulatedEngine** engines;
  size_t num_engines;
  GetPrepopulationSetFromGeoID(prefs, &engines, &num_engines);
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
