// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test compact language detector
//
// Small version, covering these languages only:
// Arabic Bulgarian Catalan Chinese ChineseT Croatian Czech Danish Dutch
// English Estonian Finnish French German Greek Hebrew Hindi Hungarian
// Icelandic Indonesian Italian Japanese Korean Latvian Lithuanian Norwegian
// Polish Portuguese Romanian Russian Serbian Slovak Slovenian Spanish
// Swedish Tagalog Thai Turkish Ukrainian Vietnamese

// Additional single-language scripts recognized for free:
// Armenian Cherokee Dhivehi Georgian Gujarati Inuktitut Kannada Khmer
// Laothian Malayalam Oriya Punjabi Sinhalese Syriac Telugu Tamil
//

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/ext_lang_enc.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/unittest_data.h"

#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_commandlineflags.h"
#include "third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_google.h"

//DEFINE_bool(html, false, "Print language spans in HTML on stderr");
//DEFINE_bool(detail, false, "Print incoming text to stderr");
//DEFINE_bool(skipbig, false, "Skip BigInputTests");

// Test strings.
// These are all included here to make the unit test self-contained.
const char* kTeststr_en =
  "confiscation of goods is assigned as the penalty part most of the courts "
  "consist of members and when it is necessary to bring public cases before a "
  "jury of members two courts combine for the purpose the most important cases "
  "of all are brought jurors or";


// UTF8 constants. Use a UTF-8 aware editor for this file
const char* kTeststr_ks =
  "नेपाल एसिया "
  "मंज अख मुलुक"
  " राजधानी काठ"
  "माडौं नेपाल "
  "अधिराज्य पेर"
  "ेग्वाय "
  "दक्षिण अमेरि"
  "का महाद्वीपे"
  " मध् यक्षेत्"
  "रे एक देश अस"
  "् ति फणीश्वर"
  " नाथ रेणु "
  "फिजी छु दक्ष"
  "िण प्रशान् त"
  " महासागर मंज"
  " अख देश बहाम"
  "ास छु केरेबि"
  "यन मंज "
  "अख मुलुख राज"
  "धानी नसौ सम्"
  " बद्घ विषय ब"
  "ुरुंडी अफ्री"
  "का महाद्वीपे"
  " मध् "
  "यक्षेत्रे दे"
  "श अस् ति सम्"
  " बद्घ विषय";

// const char* kTeststr_ks =
//  \u0928\u0947\u092A\u093E\u0932\u0020\u090F\u0938\u093F\u092F\u093E\u0020
//  \u092E\u0902\u091C\u0020\u0905\u0916\u0020\u092E\u0941\u0932\u0941\u0915
//  \u0020\u0930\u093E\u091C\u0927\u093E\u0928\u0940\u0020\u0915\u093E\u0920
//  \u092E\u093E\u0921\u094C\u0902\u0020\u0928\u0947\u092A\u093E\u0932\u0020
//  \u0905\u0927\u093F\u0930\u093E\u091C\u094D\u092F\u0020\u092A\u0947\u0930
//  \u0947\u0917\u094D\u0935\u093E\u092F\u0020
//  \u0926\u0915\u094D\u0937\u093F\u0923\u0020\u0905\u092E\u0947\u0930\u093F
//  \u0915\u093E\u0020\u092E\u0939\u093E\u0926\u094D\u0935\u0940\u092A\u0947
//  \u0020\u092E\u0927\u094D\u0020\u092F\u0915\u094D\u0937\u0947\u0924\u094D
//  \u0930\u0947\u0020\u090F\u0915\u0020\u0926\u0947\u0936\u0020\u0905\u0938
//  \u094D\u0020\u0924\u093F\u0020\u092B\u0923\u0940\u0936\u094D\u0935\u0930
//  \u0020\u0928\u093E\u0925\u0020\u0930\u0947\u0923\u0941\u0020
//  \u092B\u093F\u091C\u0940\u0020\u091B\u0941\u0020\u0926\u0915\u094D\u0937
//  \u093F\u0923\u0020\u092A\u094D\u0930\u0936\u093E\u0928\u094D\u0020\u0924
//  \u0020\u092E\u0939\u093E\u0938\u093E\u0917\u0930\u0020\u092E\u0902\u091C
//  \u0020\u0905\u0916\u0020\u0926\u0947\u0936\u0020\u092C\u0939\u093E\u092E
//  \u093E\u0938\u0020\u091B\u0941\u0020\u0915\u0947\u0930\u0947\u092C\u093F
//  \u092F\u0928\u0020\u092E\u0902\u091C\u0020
//  \u0905\u0916\u0020\u092E\u0941\u0932\u0941\u0916\u0020\u0930\u093E\u091C
//  \u0927\u093E\u0928\u0940\u0020\u0928\u0938\u094C\u0020\u0938\u092E\u094D
//  \u0020\u092C\u0926\u094D\u0918\u0020\u0935\u093F\u0937\u092F\u0020\u092C
//  \u0941\u0930\u0941\u0902\u0921\u0940\u0020\u0905\u092B\u094D\u0930\u0940
//  \u0915\u093E\u0020\u092E\u0939\u093E\u0926\u094D\u0935\u0940\u092A\u0947
//  \u0020\u092E\u0927\u094D\u0020
//  \u092F\u0915\u094D\u0937\u0947\u0924\u094D\u0930\u0947\u0020\u0926\u0947
//  \u0936\u0020\u0905\u0938\u094D\u0020\u0924\u093F\u0020\u0938\u092E\u094D
//  \u0020\u092C\u0926\u094D\u0918\u0020\u0935\u093F\u0937\u092F




namespace {

class CompactLangDetTest : public testing::Test {
 protected:
  // Objects declared here can be used by all tests in the test case for Foo.

  // Detect language of plaintext src
  Language TestCompactLangDetPlain(const char* src) {
    bool is_plain_text = true;
    bool is_reliable;

    Language lang = CompactLangDet::DetectLanguage(src, strlen(src),
                                                   is_plain_text,
                                                   &is_reliable);
    return lang;
  }


  // Detect extended language of plaintext src
  Language TestExtCompactLangDetPlain(const char* src) {
    bool is_plain_text = true;
    Language language3[3];
    int percent3[3];
    int text_bytes;
    bool is_reliable;

    Language lang =  CompactLangDet::ExtDetectLanguageSummary(src, strlen(src),
                            is_plain_text,
                            language3,
                            percent3,
                            &text_bytes,
                            &is_reliable);
    return lang;
  }
};  // End class CompactLangDetTest.

}  // End namespace.

TEST_F(CompactLangDetTest, EasyTests) {
  EXPECT_EQ(ENGLISH, TestCompactLangDetPlain(kTeststr_en));
  EXPECT_EQ(HINDI, TestCompactLangDetPlain(kTeststr_hi_Deva));
}


TEST_F(CompactLangDetTest, FullTests) {
  // Do all the languages in all their scripts
  //// EXPECT_EQ(AFAR, TestCompactLangDetPlain(kTeststr_aa_Latn));
  //// EXPECT_EQ(ABKHAZIAN, TestCompactLangDetPlain(kTeststr_ab_Cyrl));
  //// EXPECT_EQ(AFRIKAANS, TestCompactLangDetPlain(kTeststr_af_Latn));
  //// EXPECT_EQ(AMHARIC, TestCompactLangDetPlain(kTeststr_am_Ethi));
  EXPECT_EQ(ARABIC, TestCompactLangDetPlain(kTeststr_ar_Arab));
  //// EXPECT_EQ(ASSAMESE, TestCompactLangDetPlain(kTeststr_as_Beng));
  //// EXPECT_EQ(AYMARA, TestCompactLangDetPlain(kTeststr_ay_Latn));
  // AZERBAIJANI Arab & Cyrl removed 2008.05.27. Just AZERBAIJANI Latn left
  //  EXPECT_EQ(AZERBAIJANI, TestCompactLangDetPlain(kTeststr_az_Arab));
  //  Missing data: az-Cyrl
  //// EXPECT_EQ(AZERBAIJANI, TestCompactLangDetPlain(kTeststr_az_Latn));

  //// EXPECT_EQ(BASHKIR, TestCompactLangDetPlain(kTeststr_ba_Cyrl));
  //// EXPECT_EQ(BELARUSIAN, TestCompactLangDetPlain(kTeststr_be_Cyrl));
  EXPECT_EQ(BULGARIAN, TestCompactLangDetPlain(kTeststr_bg_Cyrl));
  //// EXPECT_EQ(BIHARI, TestCompactLangDetPlain(kTeststr_bh_Deva));
  //// EXPECT_EQ(BISLAMA, TestCompactLangDetPlain(kTeststr_bi_Latn));
  //// EXPECT_EQ(BENGALI, TestCompactLangDetPlain(kTeststr_bn_Beng));
  //// EXPECT_EQ(TIBETAN, TestCompactLangDetPlain(kTeststr_bo_Tibt));
  //// EXPECT_EQ(BRETON, TestCompactLangDetPlain(kTeststr_br_Latn));
  EXPECT_EQ(SERBIAN, TestCompactLangDetPlain(kTeststr_bs_Cyrl));    // NOTE: Not BOSNIAN
  EXPECT_EQ(CROATIAN, TestCompactLangDetPlain(kTeststr_bs_Latn));   // NOTE: Not BOSNIAN

  EXPECT_EQ(CATALAN, TestCompactLangDetPlain(kTeststr_ca_Latn));
  EXPECT_EQ(CHEROKEE, TestCompactLangDetPlain(kTeststr_chr_Cher));
  //// EXPECT_EQ(CORSICAN, TestCompactLangDetPlain(kTeststr_co_Latn));
    // No CREOLES_AND_PIDGINS_ENGLISH_BASED
    // No CREOLES_AND_PIDGINS_FRENCH_BASED
    // No CREOLES_AND_PIDGINS_OTHER
    // No CREOLES_AND_PIDGINS_PORTUGUESE_BASED
  EXPECT_EQ(CZECH, TestCompactLangDetPlain(kTeststr_cs_Latn));
  //// EXPECT_EQ(WELSH, TestCompactLangDetPlain(kTeststr_cy_Latn));

  EXPECT_EQ(DANISH, TestCompactLangDetPlain(kTeststr_da_Latn));
  EXPECT_EQ(GERMAN, TestCompactLangDetPlain(kTeststr_de_Latn));
  EXPECT_EQ(DHIVEHI, TestCompactLangDetPlain(kTeststr_dv_Thaa));
  //// EXPECT_EQ(DZONGKHA, TestCompactLangDetPlain(kTeststr_dz_Tibt));

  EXPECT_EQ(GREEK, TestCompactLangDetPlain(kTeststr_el_Grek));
  EXPECT_EQ(ENGLISH, TestCompactLangDetPlain(kTeststr_en_Latn));
  //// EXPECT_EQ(ESPERANTO, TestCompactLangDetPlain(kTeststr_eo_Latn));
  EXPECT_EQ(SPANISH, TestCompactLangDetPlain(kTeststr_es_Latn));
  EXPECT_EQ(ESTONIAN, TestCompactLangDetPlain(kTeststr_et_Latn));
  //// EXPECT_EQ(BASQUE, TestCompactLangDetPlain(kTeststr_eu_Latn));

  //// EXPECT_EQ(PERSIAN, TestCompactLangDetPlain(kTeststr_fa_Arab));
  EXPECT_EQ(FINNISH, TestCompactLangDetPlain(kTeststr_fi_Latn));
  //// EXPECT_EQ(FIJIAN, TestCompactLangDetPlain(kTeststr_fj_Latn));
  //// EXPECT_EQ(FAROESE, TestCompactLangDetPlain(kTeststr_fo_Latn));
  EXPECT_EQ(FRENCH, TestCompactLangDetPlain(kTeststr_fr_Latn));
  //// EXPECT_EQ(FRISIAN, TestCompactLangDetPlain(kTeststr_fy_Latn));

  //// EXPECT_EQ(IRISH, TestCompactLangDetPlain(kTeststr_ga_Latn));
  //// EXPECT_EQ(SCOTS_GAELIC, TestCompactLangDetPlain(kTeststr_gd_Latn));
  //// EXPECT_EQ(GALICIAN, TestCompactLangDetPlain(kTeststr_gl_Latn));
  //// EXPECT_EQ(GUARANI, TestCompactLangDetPlain(kTeststr_gn_Latn));
  EXPECT_EQ(GUJARATI, TestCompactLangDetPlain(kTeststr_gu_Gujr));
  //// EXPECT_EQ(MANX, TestCompactLangDetPlain(kTeststr_gv_Latn));

 ////  EXPECT_EQ(HAUSA, TestCompactLangDetPlain(kTeststr_ha_Latn));
  EXPECT_EQ(HINDI, TestCompactLangDetPlain(kTeststr_hi_Deva));
  EXPECT_EQ(CROATIAN, TestCompactLangDetPlain(kTeststr_hr_Latn));     // NOTE: now CROATIAN
  //// EXPECT_EQ(HAITIAN_CREOLE, TestCompactLangDetPlain(kTeststr_ht_Latn));
  EXPECT_EQ(HUNGARIAN, TestCompactLangDetPlain(kTeststr_hu_Latn));
  EXPECT_EQ(ARMENIAN, TestCompactLangDetPlain(kTeststr_hy_Armn));

  //// EXPECT_EQ(INTERLINGUA, TestCompactLangDetPlain(kTeststr_ia_Latn));
  EXPECT_EQ(INDONESIAN, TestCompactLangDetPlain(kTeststr_id_Latn));
  //// EXPECT_EQ(INTERLINGUE, TestCompactLangDetPlain(kTeststr_ie_Latn));
  //// EXPECT_EQ(INUPIAK, TestCompactLangDetPlain(kTeststr_ik_Latn));
  EXPECT_EQ(ICELANDIC, TestCompactLangDetPlain(kTeststr_is_Latn));
  EXPECT_EQ(ITALIAN, TestCompactLangDetPlain(kTeststr_it_Latn));
  EXPECT_EQ(INUKTITUT, TestCompactLangDetPlain(kTeststr_iu_Cans));
  EXPECT_EQ(HEBREW, TestCompactLangDetPlain(kTeststr_iw_Hebr));

  EXPECT_EQ(JAPANESE, TestCompactLangDetPlain(kTeststr_ja_Hani));
  //// EXPECT_EQ(JAVANESE, TestCompactLangDetPlain(kTeststr_jw_Latn));

  EXPECT_EQ(GEORGIAN, TestCompactLangDetPlain(kTeststr_ka_Geor));
  //// EXPECT_EQ(KHASI, TestCompactLangDetPlain(kTeststr_kha_Latn));
  //// EXPECT_EQ(KAZAKH, TestCompactLangDetPlain(kTeststr_kk_Arab));
  //// EXPECT_EQ(KAZAKH, TestCompactLangDetPlain(kTeststr_kk_Cyrl));
  //// EXPECT_EQ(KAZAKH, TestCompactLangDetPlain(kTeststr_kk_Latn));
  //// EXPECT_EQ(GREENLANDIC, TestCompactLangDetPlain(kTeststr_kl_Latn));
  EXPECT_EQ(KHMER, TestCompactLangDetPlain(kTeststr_km_Khmr));
  EXPECT_EQ(KANNADA, TestCompactLangDetPlain(kTeststr_kn_Knda));
  EXPECT_EQ(KOREAN, TestCompactLangDetPlain(kTeststr_ko_Hani));
  //// EXPECT_EQ(KASHMIRI, TestCompactLangDetPlain(kTeststr_ks_Deva));
  // KURDISH Latn removed 2008.05.27. Just KURDISH Arab left
 ////  EXPECT_EQ(KURDISH, TestCompactLangDetPlain(kTeststr_ku_Arab));
  //  EXPECT_EQ(KURDISH, TestCompactLangDetPlain(kTeststr_ku_Latn));
  //// EXPECT_EQ(KYRGYZ, TestCompactLangDetPlain(kTeststr_ky_Arab));
  //// EXPECT_EQ(KYRGYZ, TestCompactLangDetPlain(kTeststr_ky_Cyrl));

  //// EXPECT_EQ(LATIN, TestCompactLangDetPlain(kTeststr_la_Latn));
  //// EXPECT_EQ(LUXEMBOURGISH, TestCompactLangDetPlain(kTeststr_lb_Latn));
  //// EXPECT_EQ(GANDA, TestCompactLangDetPlain(kTeststr_lg_Latn));
  //// EXPECT_EQ(LINGALA, TestCompactLangDetPlain(kTeststr_ln_Latn));
  EXPECT_EQ(LAOTHIAN, TestCompactLangDetPlain(kTeststr_lo_Laoo));
  EXPECT_EQ(LITHUANIAN, TestCompactLangDetPlain(kTeststr_lt_Latn));
  EXPECT_EQ(LATVIAN, TestCompactLangDetPlain(kTeststr_lv_Latn));

  //// EXPECT_EQ(MALAGASY, TestCompactLangDetPlain(kTeststr_mg_Latn));
  //// EXPECT_EQ(MAORI, TestCompactLangDetPlain(kTeststr_mi_Latn));
  //// EXPECT_EQ(MACEDONIAN, TestCompactLangDetPlain(kTeststr_mk_Cyrl));
  EXPECT_EQ(MALAYALAM, TestCompactLangDetPlain(kTeststr_ml_Mlym));
  //// EXPECT_EQ(MONGOLIAN, TestCompactLangDetPlain(kTeststr_mn_Cyrl));
  //// EXPECT_EQ(MOLDAVIAN, TestCompactLangDetPlain(kTeststr_mo_Cyrl));
  //// EXPECT_EQ(MARATHI, TestCompactLangDetPlain(kTeststr_mr_Deva));
  //// EXPECT_EQ(MALAY, TestCompactLangDetPlain(kTeststr_ms_Latn));
  //// EXPECT_EQ(MALAY, TestCompactLangDetPlain(kTeststr_ms_Latn2));
  //// EXPECT_EQ(MALAY, TestCompactLangDetPlain(kTeststr_ms_Latn3));
  //// EXPECT_EQ(MALTESE, TestCompactLangDetPlain(kTeststr_mt_Latn));
  //// EXPECT_EQ(BURMESE, TestCompactLangDetPlain(kTeststr_my_Latn));
  //// EXPECT_EQ(BURMESE, TestCompactLangDetPlain(kTeststr_my_Mymr));

  //// EXPECT_EQ(NAURU, TestCompactLangDetPlain(kTeststr_na_Latn));
  //// EXPECT_EQ(NEPALI, TestCompactLangDetPlain(kTeststr_ne_Deva));
  EXPECT_EQ(DUTCH, TestCompactLangDetPlain(kTeststr_nl_Latn));
  //// EXPECT_EQ(NORWEGIAN_N, TestCompactLangDetPlain(kTeststr_nn_Latn));
  EXPECT_EQ(NORWEGIAN, TestCompactLangDetPlain(kTeststr_no_Latn));

  //// EXPECT_EQ(OCCITAN, TestCompactLangDetPlain(kTeststr_oc_Latn));
  //// EXPECT_EQ(OROMO, TestCompactLangDetPlain(kTeststr_om_Latn));
  EXPECT_EQ(ORIYA, TestCompactLangDetPlain(kTeststr_or_Orya));

  EXPECT_EQ(PUNJABI, TestCompactLangDetPlain(kTeststr_pa_Guru));
  EXPECT_EQ(POLISH, TestCompactLangDetPlain(kTeststr_pl_Latn));
  //// EXPECT_EQ(PASHTO, TestCompactLangDetPlain(kTeststr_ps_Arab));
  EXPECT_EQ(PORTUGUESE, TestCompactLangDetPlain(kTeststr_pt_BR));     // NOTE: not PORTUGUESE_B
                                                                      // nor PORTUGUESE_P

  //// EXPECT_EQ(QUECHUA, TestCompactLangDetPlain(kTeststr_qu_Latn));

  //// EXPECT_EQ(RHAETO_ROMANCE, TestCompactLangDetPlain(kTeststr_rm_Latn));
  //// EXPECT_EQ(RUNDI, TestCompactLangDetPlain(kTeststr_rn_Latn));
  EXPECT_EQ(ROMANIAN, TestCompactLangDetPlain(kTeststr_ro_Latn));
  EXPECT_EQ(RUSSIAN, TestCompactLangDetPlain(kTeststr_ru_Cyrl));
  //// EXPECT_EQ(KINYARWANDA, TestCompactLangDetPlain(kTeststr_rw_Latn));

  //// EXPECT_EQ(SANSKRIT, TestCompactLangDetPlain(kTeststr_sa_Deva));
  //// EXPECT_EQ(SANSKRIT, TestCompactLangDetPlain(kTeststr_sa_Latn));
  //// EXPECT_EQ(SCOTS, TestCompactLangDetPlain(kTeststr_sco_Latn));
  //// EXPECT_EQ(SINDHI, TestCompactLangDetPlain(kTeststr_sd_Arab));
 ////  EXPECT_EQ(SANGO, TestCompactLangDetPlain(kTeststr_sg_Latn));
    // No SERBO_CROATIAN (sh)
  EXPECT_EQ(SINHALESE, TestCompactLangDetPlain(kTeststr_si_Sinh));
  //// EXPECT_EQ(LIMBU, TestCompactLangDetPlain(kTeststr_sit_NP));
  EXPECT_EQ(SLOVAK, TestCompactLangDetPlain(kTeststr_sk_Latn));
  EXPECT_EQ(SLOVENIAN, TestCompactLangDetPlain(kTeststr_sl_Latn));
  //// EXPECT_EQ(SAMOAN, TestCompactLangDetPlain(kTeststr_sm_Latn));
  //// EXPECT_EQ(SHONA, TestCompactLangDetPlain(kTeststr_sn_Latn));
  //// EXPECT_EQ(SOMALI, TestCompactLangDetPlain(kTeststr_so_Latn));
  //// EXPECT_EQ(ALBANIAN, TestCompactLangDetPlain(kTeststr_sq_Latn));
  EXPECT_EQ(SERBIAN, TestCompactLangDetPlain(kTeststr_sr_Cyrl));    // NOTE: now SERBIAN
  EXPECT_EQ(CROATIAN, TestCompactLangDetPlain(kTeststr_sr_Latn));   // NOTE: Not SERBIAN
  EXPECT_EQ(CROATIAN, TestCompactLangDetPlain(kTeststr_sr_ME_Latn));   // NOTE: not SERBIAN nor MONTENEGRIN
  //// EXPECT_EQ(SISWANT, TestCompactLangDetPlain(kTeststr_ss_Latn));
  //// EXPECT_EQ(SESOTHO, TestCompactLangDetPlain(kTeststr_st_Latn));
  //// EXPECT_EQ(SUNDANESE, TestCompactLangDetPlain(kTeststr_su_Latn));
  EXPECT_EQ(SWEDISH, TestCompactLangDetPlain(kTeststr_sv_Latn));
  //// EXPECT_EQ(SWAHILI, TestCompactLangDetPlain(kTeststr_sw_Latn));
  EXPECT_EQ(SYRIAC, TestCompactLangDetPlain(kTeststr_syr_Syrc));

  EXPECT_EQ(TAMIL, TestCompactLangDetPlain(kTeststr_ta_Taml));
  EXPECT_EQ(TELUGU, TestCompactLangDetPlain(kTeststr_te_Telu));
  // Tajik Arab removed 2008.05.27. Just Tajik Cyrl left
  //  EXPECT_EQ(TAJIK, TestCompactLangDetPlain(kTeststr_tg_Arab));
  //// EXPECT_EQ(TAJIK, TestCompactLangDetPlain(kTeststr_tg_Cyrl));
  EXPECT_EQ(THAI, TestCompactLangDetPlain(kTeststr_th_Thai));
  //// EXPECT_EQ(TIGRINYA, TestCompactLangDetPlain(kTeststr_ti_Ethi));
  //// EXPECT_EQ(TURKMEN, TestCompactLangDetPlain(kTeststr_tk_Cyrl));
  //// EXPECT_EQ(TURKMEN, TestCompactLangDetPlain(kTeststr_tk_Latn));
  EXPECT_EQ(TAGALOG, TestCompactLangDetPlain(kTeststr_tl_Latn));
  //// EXPECT_EQ(TSWANA, TestCompactLangDetPlain(kTeststr_tn_Latn));
  //// EXPECT_EQ(TONGA, TestCompactLangDetPlain(kTeststr_to_Latn));
  EXPECT_EQ(TURKISH, TestCompactLangDetPlain(kTeststr_tr_Latn));
  //// EXPECT_EQ(TSONGA, TestCompactLangDetPlain(kTeststr_ts_Latn));
  //// EXPECT_EQ(TATAR, TestCompactLangDetPlain(kTeststr_tt_Cyrl));
  //// EXPECT_EQ(TATAR, TestCompactLangDetPlain(kTeststr_tt_Latn));
  //// EXPECT_EQ(TWI, TestCompactLangDetPlain(kTeststr_tw_Latn));

  //// EXPECT_EQ(UIGHUR, TestCompactLangDetPlain(kTeststr_ug_Arab));
  //// EXPECT_EQ(UIGHUR, TestCompactLangDetPlain(kTeststr_ug_Cyrl));
  //// EXPECT_EQ(UIGHUR, TestCompactLangDetPlain(kTeststr_ug_Latn));
  EXPECT_EQ(UKRAINIAN, TestCompactLangDetPlain(kTeststr_uk_Cyrl));
  //// EXPECT_EQ(URDU, TestCompactLangDetPlain(kTeststr_ur_Arab));
  //// EXPECT_EQ(UZBEK, TestCompactLangDetPlain(kTeststr_uz_Arab));
  //// EXPECT_EQ(UZBEK, TestCompactLangDetPlain(kTeststr_uz_Cyrl));
  //// EXPECT_EQ(UZBEK, TestCompactLangDetPlain(kTeststr_uz_Latn));

  EXPECT_EQ(VIETNAMESE, TestCompactLangDetPlain(kTeststr_vi_Latn));
  //// EXPECT_EQ(VOLAPUK, TestCompactLangDetPlain(kTeststr_vo_Latn));

  //// EXPECT_EQ(WOLOF, TestCompactLangDetPlain(kTeststr_wo_Latn));

  //// EXPECT_EQ(XHOSA, TestCompactLangDetPlain(kTeststr_xh_Latn));

  //// EXPECT_EQ(YIDDISH, TestCompactLangDetPlain(kTeststr_yi_Hebr));
  //// EXPECT_EQ(YORUBA, TestCompactLangDetPlain(kTeststr_yo_Latn));

  // Zhuang Hani removed 2008.05.13. Just Zhuang Latn left
  //  EXPECT_EQ(ZHUANG, TestCompactLangDetPlain(kTeststr_za_Hani));
  //// EXPECT_EQ(ZHUANG, TestCompactLangDetPlain(kTeststr_za_Latn));
  EXPECT_EQ(CHINESE, TestCompactLangDetPlain(kTeststr_zh_Hani));
  EXPECT_EQ(CHINESE_T, TestCompactLangDetPlain(kTeststr_zh_TW));
  //// EXPECT_EQ(ZULU, TestCompactLangDetPlain(kTeststr_zu_Latn));
  // No TG_UNKNOWN_LANGUAGE
  // No UNKNOWN_LANGUAGE
}


TEST_F(CompactLangDetTest, ExtendedTests) {
  // Do the extended languages, with them not-allowed then allowed
  // These turn out to be extraordinarily sensitive forms of garbage bytes
  //// EXPECT_EQ(ENGLISH, TestCompactLangDetPlain(kTeststr_tlh_Latn));
  //// EXPECT_EQ(X_KLINGON, TestExtCompactLangDetPlain(kTeststr_tlh_Latn));

  //// EXPECT_EQ(ENGLISH, TestCompactLangDetPlain(kTeststr_zzp_Latn));
  //// EXPECT_EQ(X_PIG_LATIN, TestExtCompactLangDetPlain(kTeststr_zzp_Latn));

  //// EXPECT_EQ(ENGLISH, TestCompactLangDetPlain(kTeststr_xx_Bugi));
  //// EXPECT_EQ(X_BUGINESE, TestExtCompactLangDetPlain(kTeststr_xx_Bugi));

  //// EXPECT_EQ(ENGLISH, TestCompactLangDetPlain(kTeststr_xx_Goth));
  //// EXPECT_EQ(X_GOTHIC, TestExtCompactLangDetPlain(kTeststr_xx_Goth));

  // Next three now removed permanently from probability tables (May 2008)
  //  (used to be X_BORK_BORK_BORK, X_ELMER_FUDD, X_HACKER).
  //
  // Small changes in probability tables may cause these non-texts to
  // change detection result. If that happens, cross-check that
  // the new result is not because of a bug, then change the expected values.
  EXPECT_EQ(ENGLISH, TestCompactLangDetPlain(kTeststr_zzb_Latn));
  EXPECT_EQ(ENGLISH, TestExtCompactLangDetPlain(kTeststr_zzb_Latn));

  EXPECT_EQ(ENGLISH, TestCompactLangDetPlain(kTeststr_zze_Latn));
  EXPECT_EQ(ENGLISH, TestExtCompactLangDetPlain(kTeststr_zze_Latn));

  EXPECT_EQ(ENGLISH, TestCompactLangDetPlain(kTeststr_zzh_Latn));
  EXPECT_EQ(ENGLISH, TestExtCompactLangDetPlain(kTeststr_zzh_Latn));
}
