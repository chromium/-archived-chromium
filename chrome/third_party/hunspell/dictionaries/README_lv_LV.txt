# Latvie�u valodas pareizrakst�bas p�rbaudes bibliot�ka (afiksu un v�rdn�cas fails) lieto�anai ar OpenOffice 1.0 un augst�k
# Latvian spelling dictionary (affix and dictionary files) for OpenOffice 1.0 and higher
#
# Copyright (C) 2002-2005 Janis Eisaks, jancs@dv.lv
#
# 
# �� bibliot�ka tiek licenc�ta ar Lesser General Public Licence (LGPL) 2.1 nosac�jumiem. 
# Licences nosac�jumi pievienoti fail� license.txt vai ieg�stami t�mek�a vietn�  http://www.gnu.org/copyleft/lesser.txt 
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
2. Uzst�d��ana
3. Interesentiem

=================
1. Version info 0.6.5
- korekcijas afiksos, kasattiecas uz �pa��bas v�rdu p�r�kaj�m un visp�r�kaj�m pak�p�m;
- b�tiski papildin�ts v�rdu kr�jums.

Zin�m�s probl�mas:
- atsevi��u I konjug�cijas darb�bas v�rdu formas.

=================

2. V�rd�cas uzst�d��ana

Ieteikums
 �oti silti iesaku uzst�d�t OO 2.0.2 versiju, jo taj� ir izlabotas k��das, kas attiecas uz latvie�u burtu (ISO 8859-13 kodu tabulas) interpret�ciju OO. Iepriek��j�s versij�s, gad�jum�, ja v�rds s�k�s ar k�du no latvie�u burtiem, tad tas tika atz�m�st k� nepareizs, ja atrad�s teikuma s�kum� (ar pirmo lielo burtu).


1. iesp�ja. Uzst�d��ana tie�saistes re��m�
 No izv�lnes File/Wizards/Install new dictionaries palaidiet att. vedni, izv�lieties Jums t�kamo ved�a valodu un sekojiet nor�d�jumiem. Baz latvie�u valodas pareizrakst�bas r�kiem J�s vienlaic�gi varat uzst�d�t papildus valodas vai atsvaidzin�t eso��s bibliot�kas.
 
 Ja kaut k�du iemeslu d�� nevarat izmantot 1. iesp�ju, ir
 
 2. iesp�ja. "Offline" uzst�d��ana
 Lejupiel�d�jiet p�d�jo modu�a versiju no openoffice-lv.sourceforge.net .
 P�c faila ieg��anas tas ir j�atpako direktorij� %Openoffice%\share\dict\ooo, kur %Openoffice% - direktorija, kur� veikta OpenOffice uzst�d��ana. Tur eso�ajam failam dictionary.lst ir j�pievieno sekojo�as rindas: 
 
 DICT lv LV lv_LV
 HYPH lv LV hyph_lv_LV

vai ar� j�izpilda win-lv_LV_add.bat (Windows gad�jum�) vai, Linux gd�jum�, j�izpilda komandu:

   sh <lin-lv_LV_add.sh

Lai izpild�tu 2. iesp�ju, Jums ir j�b�t ties�b�m rakst�t min�taj� katalog�. Ja t�du nav, varat uzst�d�t v�rdn�cu lok�li, sav� lietot�ja opciju katalog� (%OOopt%/user/wordbook).

 Offline uzstad��anai var izmantot ar� 1. iesp�j� min�to vedni, viss notiks l�dz�gi, tikai nepiecie�amaj�m modu�u pakotn�m b�s j�b�t uz lok�l� diska. J�piez�m� ka, piem�ram, SUSE gad�jum� min�tais vednis ir izgriezts �r� no OO un 2. iespeja ir vien�g�. Atsevi��i �is l�dzeklis ir ieg�stams http://lingucomponent.openoffice.org/dictpack.html.


 Ar to modu�u uzst�d��ana praktiski ir pabeigta; atliek vien�gi caur Options>Language settings>Writing aids iesl�gt vai izsl�gt nepiecie�amos modu�us un iestat�t dokumentu noklus�to valodu.
 

 Ja ir nepiecie�ama autom�tisk� pareizrakst�bas p�rbaude, zem Tools>Spellcheck j�ie�eks� AutoSpellcheck.
 



Uzman�bu OpenOffice 1.0.2, 1.1.x lietot�jiem!

Visai sist�mai kop�j�s v�rdn�cas �ai versijai atrodas katalog�

%OpenOffice.org%/share/dict/ooo

  1. variants.

  2. variants.
  Atver min�taj� katalog� eso�o failu DicOO.swx, piekr�t Makro komandu pielieto�anai
  un seko tur min�taj�m instrukcij�m.

  3. variants:
  K� aprakst�ts zem�k min�taj�s lap�s (ar� OO 1.01).
  http://www.ooodocs.org/dictinstall/nexthelp.html     (En)

  vai

  http://openoffice-lv.sourceforge.net (Lv)

  vai

  4. variants:
  viena no �eit uzskait�taj�m iesp�j�m:

  http://lingucomponent.openoffice.org/download_dictionary.html

3. Interesentiem

Dom�ju, ir j�piemin, ka Ingus R��is str�d� pie integr�cijas Mozilla b�z�tiem produktiem: 
http://3a3-interactive.net/2005/01/19/mozilla-latvian-spelling/


4. CHANGELOG

Version info 0.6.4
- b�tiskas izmai�as da�os afiksos, kuru d�� notika t�, k� notika
- izfiltr�ts un p�rbaud�ts dic fails

=================

Version info 0.6.3c
- da�i labojumi aff fail�
- papildin�ts dic fails

=================

Version info 0.6.3a
- izlabots aff fails (neuzman�bas k��da, kas, iesp�jams, neatst�ja ietekmi uz
  darb�bu
- papildin�ts dic fails

=================
 
Version info 0.6.3
- izlaboti un optimiz�ti da�i afiksi;
- manu�li izfiltr�ts v�rdu pamatformu kr�jums, tuvinot re�lai dz�vei;
- koplekt� iek�auts ar� J��a Vilima izstr�d�tais v�rdu p�rnes�js v. 0.2 (skat. hyph-lasimani.txt);

V�rdn�c� joproj�m iztr�kst �is un tas, t�p�c visi ir m��i aicin�ti s�t�t neatpaz�tos v�rdus man (adrese - skat aug��) vai ierosin�jumus par p�rnes�ju - J�nim Vilimam.

Turpm�kajos pl�nos ietilpst v�rdn�cas papildin��ana un, iesp�jams, darb�bas v�rdu afiksu p�rstr�de, ja eksperimenti apstiprin�s aizdomas par k��d�m.

=================
Version info 0.6
- piln�gi p�rveidota afiksu faila strukt�ra, sadalot loc�jumus
  pa skait�iem un pamazin�m�m form�m ar nol�ku uzlabot pareizo
  (dz�v� pielietojamo) formu kod��anu. Ir izdar�ti ar� vair�ki
  afiksu labojumi.


V�rdn�c� 0.6 joproj�m nav atrodami:
- darb�bas v�rdi, kuri eksist� tikai refleks�vaj�s form�s
- I konjug�cijas darb�bas v�rdu refleks�v�s formas
- I konjug�cijas darb�bas v�rdu divdabju sufiksi - o�s o�ais ams amais
- I konjug�cijas darb�bas v�rdu divdabju sufiksi - is dams damais
- lietv�rdi-daudzskaitlinieki

Visi, kas lieto �o "br�numu", ir laipni aicin�ti s�t�t man neatpaz�to
v�rdu sarakstus (txt vai swx form�tos). 
Viside�l�k� forma - neatpaz�tais v�rds visos tam rakstur�gajos loc�jumos.

Entuziasti var nodarboties ar�dzan ar specializ�to v�rdn�cu veido�anu, 
kuras var piesl�gt lieto�anai OpenOffice paral�li pamatv�rdn�cai.

=================
Verson info 0.5.5
- pievienota visa 1. konjug�cija....
- lietv�rdu pamazin�mo formu afiksi

V�rdn�c� 0.5.5 nav atrodami:
- darb�bas v�rdi, kuri eksist� tikai refleks�vaj�s form�s
- lietv�rdi-daudzskaitlinieki
- v�l �is un tas

Verson info 0.5sr2
- pievienoti 1. konjug�cijas darb�bas v�rd� n�kotnes formas

V�rdn�c� 0.5sr2 nav atrodami:
- darb�bas v�rdi, kuri eksist� tikai refleks�vaj�s form�s
- lietv�rdi-daudzskaitlinieki
- v�l �is un tas (piem. pamazin�m�s formas)

Verson info 0.5
- pievienota divdabju veido�ana un loc��ana
- izlabota k��da I deklin�cijas loc��an� (piem. ku�is)
- izlabota k��da V deklin�cijas loc��an� (piem. roze)
- papildin�ta v�rdn�ca (nedaudz)

V�rdn�c� 0.5 nav atrodami:
- 1. konjug�cijas darb�bas v�rdi
- darb�bas v�rdi, kuri eksist� tikai refleks�vaj�s form�s
- lietv�rdi-daudzskaitlinieki
- v�l �is un tas (piem. pamazin�m�s formas)

Version info 0.4.1
- izlabots glucks I deklin�cijas lietv�rdu loc��an�
- pievienoti skait�a v�rdi

V�rdn�c� nav atrodami:
- 1. konjug�cijas darb�bas v�rdi
- darb�bas v�rdi, kuri eksist� tikai refleks�vaj�s form�s
- lietv�rdi-daudzskaitlinieki
- divdabji, 
- v�l �is un tas


Version info: 0.4
-kori��tas 1. un 4. deklin�cija lietv�rdu v�rdn�cas
-ieviesti afiksi �pa��bas v�rdiem
-pievienoti apst�k�a v�rdi un �pa��bas v�rdi

V�rdn�c� nav atrodami:
- 1. konjug�cijas darb�bas v�rdi
- darb�bas v�rdi, kuri eksist� tikai refleks�vaj�s form�s
- lietv�rdi-daudzskaitlinieki
- divdabji, 
- skait�a v�rdi
- v�l �is un tas


Version info: 0.3.1
-izlabots fails .dic, II deklin�cijas atsl�ga (suns, akmens)
-pievienoti 1. un 6. deklin�cijas lietv�rdi

V�rdn�c� nav atrodami:
- 1. konjug�cijas darb�bas v�rdi
- �pa��bas v�rdi, divdabji, apst�k�a v�rdi, 
- darb�bas v�rdi, kuri eksist� tikai refleks�vaj�s form�s
- skait�a v�rdi
- v�l �is un tas


Version info: 0.3

V�rdn�c� nav atrodami:
- 1. un 6. deklin�cijas lietv�rdi
- 1. konjug�cijas darb�bas v�rdi
- �pa��bas v�rdi, divdabji, apst�k�a v�rdi, 
- darb�bas v�rdi, kuri eksist� tikai refleks�vaj�s form�s
- skait�a v�rdi
- v�l �is un tas

Version info: 0.3a
izlabots fails .dic, kur� bija zudu�as atsl�gas IV deklin�cijai

Version info: 0.3

V�rdn�c� nav atrodami:
- 1. un 6. deklin�cijas lietv�rdi
- 1. konjug�cijas darb�bas v�rdi
- �pa��bas v�rdi, divdabji, apst�k�a v�rdi, 
- darb�bas v�rdi, kuri eksist� tikai refleks�vaj�s form�s
- skait�a v�rdi
- v�l �is un tas

V�rd�cas uzst�d��anai - skat. :

http://www.ooodocs.org/dictinstall/nexthelp.html   (En)

vai

http://www.integoplus.lv/opensource/openoffice.html  (Lv)
