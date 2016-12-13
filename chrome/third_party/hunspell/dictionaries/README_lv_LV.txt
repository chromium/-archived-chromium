# Latvieðu valodas pareizrakstîbas pârbaudes bibliotçka (afiksu un vârdnîcas fails) lietoðanai ar OpenOffice 1.0 un augstâk
# Latvian spelling dictionary (affix and dictionary files) for OpenOffice 1.0 and higher
#
# Copyright (C) 2002-2005 Janis Eisaks, jancs@dv.lv
#
# 
# Ðî bibliotçka tiek licencçta ar Lesser General Public Licence (LGPL) 2.1 nosacîjumiem. 
# Licences nosacîjumi pievienoti failâ license.txt vai iegûstami tîmekïa vietnç  http://www.gnu.org/copyleft/lesser.txt 
# This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# Latvian affix table for OpenOffice 1.0 and higher

1. Version info
2. Uzstâdîðana
3. Interesentiem

=================
1. Version info 0.6.5
- korekcijas afiksos, kasattiecas uz îpaðîbas vârdu pârâkajâm un vispârâkajâm pakâpçm;
- bûtiski papildinâts vârdu krâjums.

Zinâmâs problçmas:
- atseviðíu I konjugâcijas darbîbas vârdu formas.

=================

2. Vârdîcas uzstâdîðana

Ieteikums
 Ïoti silti iesaku uzstâdît OO 2.0.2 versiju, jo tajâ ir izlabotas kïûdas, kas attiecas uz latvieðu burtu (ISO 8859-13 kodu tabulas) interpretâciju OO. Iepriekðçjâs versijâs, gadîjumâ, ja vârds sâkâs ar kâdu no latvieðu burtiem, tad tas tika atzîmçst kâ nepareizs, ja atradâs teikuma sâkumâ (ar pirmo lielo burtu).


1. iespçja. Uzstâdîðana tieðsaistes reþîmâ
 No izvçlnes File/Wizards/Install new dictionaries palaidiet att. vedni, izvçlieties Jums tîkamo vedòa valodu un sekojiet norâdîjumiem. Baz latvieðu valodas pareizrakstîbas rîkiem Jûs vienlaicîgi varat uzstâdît papildus valodas vai atsvaidzinât esoðâs bibliotçkas.
 
 Ja kaut kâdu iemeslu dçï nevarat izmantot 1. iespçju, ir
 
 2. iespçja. "Offline" uzstâdîðana
 Lejupielâdçjiet pçdçjo moduïa versiju no openoffice-lv.sourceforge.net .
 Pçc faila iegûðanas tas ir jâatpako direktorijâ %Openoffice%\share\dict\ooo, kur %Openoffice% - direktorija, kurâ veikta OpenOffice uzstâdîðana. Tur esoðajam failam dictionary.lst ir jâpievieno sekojoðas rindas: 
 
 DICT lv LV lv_LV
 HYPH lv LV hyph_lv_LV

vai arî jâizpilda win-lv_LV_add.bat (Windows gadîjumâ) vai, Linux gdîjumâ, jâizpilda komandu:

   sh <lin-lv_LV_add.sh

Lai izpildîtu 2. iespçju, Jums ir jâbût tiesîbâm rakstît minçtajâ katalogâ. Ja tâdu nav, varat uzstâdît vçrdnîcu lokâli, savâ lietotâja opciju katalogâ (%OOopt%/user/wordbook).

 Offline uzstadîðanai var izmantot arî 1. iespçjâ minçto vedni, viss notiks lîdzîgi, tikai nepiecieðamajâm moduïu pakotnçm bûs jâbût uz lokâlâ diska. Jâpiezîmç ka, piemçram, SUSE gadîjumâ minçtais vednis ir izgriezts ârâ no OO un 2. iespeja ir vienîgâ. Atseviðíi ðis lîdzeklis ir iegûstams http://lingucomponent.openoffice.org/dictpack.html.


 Ar to moduïu uzstâdîðana praktiski ir pabeigta; atliek vienîgi caur Options>Language settings>Writing aids ieslçgt vai izslçgt nepiecieðamos moduïus un iestatît dokumentu noklusçto valodu.
 

 Ja ir nepiecieðama automâtiskâ pareizrakstîbas pârbaude, zem Tools>Spellcheck jâieíeksç AutoSpellcheck.
 



Uzmanîbu OpenOffice 1.0.2, 1.1.x lietotâjiem!

Visai sistçmai kopçjâs vârdnîcas ðai versijai atrodas katalogâ

%OpenOffice.org%/share/dict/ooo

  1. variants.

  2. variants.
  Atver minçtajâ katalogâ esoðo failu DicOO.swx, piekrît Makro komandu pielietoðanai
  un seko tur minçtajâm instrukcijâm.

  3. variants:
  Kâ aprakstîts zemâk minçtajâs lapâs (arî OO 1.01).
  http://www.ooodocs.org/dictinstall/nexthelp.html     (En)

  vai

  http://openoffice-lv.sourceforge.net (Lv)

  vai

  4. variants:
  viena no ðeit uzskaitîtajâm iespçjâm:

  http://lingucomponent.openoffice.org/download_dictionary.html

3. Interesentiem

Domâju, ir jâpiemin, ka Ingus Rûíis strâdâ pie integrâcijas Mozilla bâzçtiem produktiem: 
http://3a3-interactive.net/2005/01/19/mozilla-latvian-spelling/


4. CHANGELOG

Version info 0.6.4
- bûtiskas izmaiòas daþos afiksos, kuru dçï notika tâ, kâ notika
- izfiltrçts un pârbaudîts dic fails

=================

Version info 0.6.3c
- daþi labojumi aff failâ
- papildinâts dic fails

=================

Version info 0.6.3a
- izlabots aff fails (neuzmanîbas kïûda, kas, iespçjams, neatstâja ietekmi uz
  darbîbu
- papildinâts dic fails

=================
 
Version info 0.6.3
- izlaboti un optimizçti daþi afiksi;
- manuâli izfiltrçts vârdu pamatformu krâjums, tuvinot reâlai dzîvei;
- koplektâ iekïauts arî Jâòa Vilima izstrâdâtais vârdu pârnesçjs v. 0.2 (skat. hyph-lasimani.txt);

Vârdnîcâ joprojâm iztrûkst ðis un tas, tâpçc visi ir mîïi aicinâti sûtît neatpazîtos vârdus man (adrese - skat augðâ) vai ierosinâjumus par pârnesçju - Jânim Vilimam.

Turpmâkajos plânos ietilpst vârdnîcas papildinâðana un, iespçjams, darbîbas vârdu afiksu pârstrâde, ja eksperimenti apstiprinâs aizdomas par kïûdâm.

=================
Version info 0.6
- pilnîgi pârveidota afiksu faila struktûra, sadalot locîjumus
  pa skaitïiem un pamazinâmâm formâm ar nolûku uzlabot pareizo
  (dzîvç pielietojamo) formu kodçðanu. Ir izdarîti arî vairâki
  afiksu labojumi.


Vârdnîcâ 0.6 joprojâm nav atrodami:
- darbîbas vârdi, kuri eksistç tikai refleksîvajâs formâs
- I konjugâcijas darbîbas vârdu refleksîvâs formas
- I konjugâcijas darbîbas vârdu divdabju sufiksi - oðs oðais ams amais
- I konjugâcijas darbîbas vârdu divdabju sufiksi - is dams damais
- lietvârdi-daudzskaitlinieki

Visi, kas lieto ðo "brînumu", ir laipni aicinâti sûtît man neatpazîto
vârdu sarakstus (txt vai swx formâtos). 
Visideâlâkâ forma - neatpazîtais vârds visos tam raksturîgajos locîjumos.

Entuziasti var nodarboties arîdzan ar specializçto vârdnîcu veidoðanu, 
kuras var pieslçgt lietoðanai OpenOffice paralçli pamatvârdnîcai.

=================
Verson info 0.5.5
- pievienota visa 1. konjugâcija....
- lietvârdu pamazinâmo formu afiksi

Vârdnîcâ 0.5.5 nav atrodami:
- darbîbas vârdi, kuri eksistç tikai refleksîvajâs formâs
- lietvârdi-daudzskaitlinieki
- vçl ðis un tas

Verson info 0.5sr2
- pievienoti 1. konjugâcijas darbîbas vârdû nâkotnes formas

Vârdnîcâ 0.5sr2 nav atrodami:
- darbîbas vârdi, kuri eksistç tikai refleksîvajâs formâs
- lietvârdi-daudzskaitlinieki
- vçl ðis un tas (piem. pamazinâmâs formas)

Verson info 0.5
- pievienota divdabju veidoðana un locîðana
- izlabota kïûda I deklinâcijas locîðanâ (piem. kuìis)
- izlabota kïûda V deklinâcijas locîðanâ (piem. roze)
- papildinâta vârdnîca (nedaudz)

Vârdnîcâ 0.5 nav atrodami:
- 1. konjugâcijas darbîbas vârdi
- darbîbas vârdi, kuri eksistç tikai refleksîvajâs formâs
- lietvârdi-daudzskaitlinieki
- vçl ðis un tas (piem. pamazinâmâs formas)

Version info 0.4.1
- izlabots glucks I deklinâcijas lietvârdu locîðanâ
- pievienoti skaitïa vârdi

Vârdnîcâ nav atrodami:
- 1. konjugâcijas darbîbas vârdi
- darbîbas vârdi, kuri eksistç tikai refleksîvajâs formâs
- lietvârdi-daudzskaitlinieki
- divdabji, 
- vçl ðis un tas


Version info: 0.4
-koriìçtas 1. un 4. deklinâcija lietvârdu vârdnîcas
-ieviesti afiksi îpaðîbas vârdiem
-pievienoti apstâkïa vârdi un îpaðîbas vârdi

Vârdnîcâ nav atrodami:
- 1. konjugâcijas darbîbas vârdi
- darbîbas vârdi, kuri eksistç tikai refleksîvajâs formâs
- lietvârdi-daudzskaitlinieki
- divdabji, 
- skaitïa vârdi
- vçl ðis un tas


Version info: 0.3.1
-izlabots fails .dic, II deklinâcijas atslçga (suns, akmens)
-pievienoti 1. un 6. deklinâcijas lietvârdi

Vârdnîcâ nav atrodami:
- 1. konjugâcijas darbîbas vârdi
- îpaðîbas vârdi, divdabji, apstâkïa vârdi, 
- darbîbas vârdi, kuri eksistç tikai refleksîvajâs formâs
- skaitïa vârdi
- vçl ðis un tas


Version info: 0.3

Vârdnîcâ nav atrodami:
- 1. un 6. deklinâcijas lietvârdi
- 1. konjugâcijas darbîbas vârdi
- îpaðîbas vârdi, divdabji, apstâkïa vârdi, 
- darbîbas vârdi, kuri eksistç tikai refleksîvajâs formâs
- skaitïa vârdi
- vçl ðis un tas

Version info: 0.3a
izlabots fails .dic, kurâ bija zuduðas atslçgas IV deklinâcijai

Version info: 0.3

Vârdnîcâ nav atrodami:
- 1. un 6. deklinâcijas lietvârdi
- 1. konjugâcijas darbîbas vârdi
- îpaðîbas vârdi, divdabji, apstâkïa vârdi, 
- darbîbas vârdi, kuri eksistç tikai refleksîvajâs formâs
- skaitïa vârdi
- vçl ðis un tas

Vârdîcas uzstâdîðanai - skat. :

http://www.ooodocs.org/dictinstall/nexthelp.html   (En)

vai

http://www.integoplus.lv/opensource/openoffice.html  (Lv)
