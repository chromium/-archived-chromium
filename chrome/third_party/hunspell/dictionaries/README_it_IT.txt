Version 2.2 (21/05/2005) (dd/mm/yyyy)

For English Readers: please see the text at the end of this document

**********************************
***********  Italiano  ***********
**********************************

Il dizionario italiano

SOMMARIO

1. Licenza
2. Avvertenze importanti (leggere assolutamente)
3. Modalit� d'installazione in OpenOffice.org
4. Integrazione con altri prodotti
5. Altri correttori ortografici che fanno uso di questo dizionario
6. Ringraziamenti


**********
1. Licenza

Il file affix e il dizionario italiano per il correttore ortografico MySpell usato da OpenOffice.org sono rilasciati da Davide Prina davideprina<at>yahoo<dot>com (sostituire <at> con @ e <dot> con . per il contatto via email) sotto i termini e le condizioni della GNU General Public License (GPL) versione 2.0 o successiva. Una volta accettata la licenza per l'uso, la distribuzione e la modifica di questo prodotto, l'accettante dovr� rispettare tutti i termini e le condizioni riportate nella licenza scelta.

La copia della licenza applicabile a questo lavoro � disponibile presso il sito del progetto GNU:

http://www.gnu.org/


************************************************
2. Avvertenze importanti (leggere assolutamente)

A partire da OpenOffice.org 1.1, il dizionario italiano per la correzione ortografica � direttamente integrato nella suite e perci� non necessita pi� di alcuna installazione manuale. L'unico problema � che non � aggiornato all'ultima versione e quindi � consigliato effettuare l'aggiornamento manuale o automatico ad ogni nuova versione rilasciata, come � indicato di seguito.

� opportuno verificare che il dizionario sia attivo, selezionando dalla barra dei men� Strumenti->Opzioni->Impostazione Lingua->Lingue e controllando che sotto lingue standard/Occidentale, l'opzione di scelta "Italiano (Italia)" sia selezionata e abbia al suo fianco un segno di spunta sormontato dalle lettere ABC.

Nel caso il correttore non risultasse attivo, � sufficiente scegliere dalla barra dei men� Strumenti->Opzioni->Impostazione Lingua->Linguistica e dopo avere premuto sul pulsante "Modifica" della finestra di dialogo, attivarlo seleziondo l'apposita voce.

Se tuttavia il correttore non risultasse ancora attivo o nell'eventualit� che si voglia aggiornare la versione del dizionario installata � possibile utilizzare le procedure di installazione indicate oltre.


*****************************************************************************
3. Modalit� d'installazione (procedura corretta a partire dalla version 641c)

Esistono principalmente due modalit� per installare il dizionario italiano :

a) in automatico, grazie a due comodi installer creati da membri della comunit� che si occupano di tutte le fasi dell'installazione.

Nota: non � detto che l'aggiornamento automatico prelevi l'ultima versione del dizionario, in alcuni casi potrebbe addirittura installare una versione pi� vecchia rispetto a quella presente sul proprio sistema

La versione per MS Windows si trova su 

http://www.ooodocs.org/dictinstall/

Mentre la versione per Linux � reperibile presso

http://www.deadletterdrop.worldonline.co.uk/OOo/

oppure

http://ooodi.sourceforge.net/

b) manualmente
I nuovi dizionari sono scaricabili da qui: http://sourceforge.net/projects/linguistico/

Scompattate il file compresso (.zip, tar.gz, bzip,...) nella directory/cartella corretta; tale directory dipende dal sistema operativo usato e dalla propria versione di OOo (per visualizzare la propria versione � sufficiente aprire un documento di OOo, aprire il men� ? e selezionare la voce "Informazioni su OpenOffice.org ...)

Per Debian GNU/Linux: /usr/share/myspell/dict

Per le versioni ufficiali Sun:
* per le versioni maggiori o uguale alla 1.0.1 la directory su windows �: <Directory di installazione di OpenOffice.org>/share/dict/ooo
* per le versioni inferiori alla versione 1.0.1 la directory �: <Directory di installazione di OpenOffice.org>/user/wordbook/

Se il vostro sistema operativo � differente e/o non riuscite a trovare la directory in cui effettuare l'installazione, allora � possibile cercare il file dictionary.lst; la directory dove trovare questo file � quella in cui dovete installare il dizionario.

Aprite in un qualsiasi editor di testo il file dictionary.lst che troverete nella cartella citata precedentemente, in modo da poter inserire il codice della lingua e della regione che vi interessa. 

Per esempio: 
nel vostro editor di testo aggiungete questa linea al file dictionary.lst:

DICT it IT it_IT 

ATTENZIONE: la riga sopra indicata non deve iniziare con il simbolo #, altrimenti l'istruzione � commentata e di conseguenza non � eseguita. � indispensabile che siano rispettati i caratteri maiuscoli e minuscoli come riportato nell'esempio, altrimenti non funziona. Inoltre � consigliabile controllare che tale riga sia presente una sola volta nel file, altrimenti il dizionario verr� caricato pi� volte (una volta per ogni riga presente) causando uno spreco di memoria RAM e l'inconveniente di fornire pi� volte gli stessi suggerimenti alle parole errate.

Quindi salvate il file con le vostre modifiche. I significati dei vari campi sono:

Campo 1: Tipo di stringa: "DICT" , al momento � l'unica voce disponibile.
Campo 2: Codice linguistico "it" o "de" o "en" ... (vedi Codici Linguistici ISO)
Campo 3: Codice Regionale "IT" o"DE" o "US" ... (vedi Codici Regionali ISO)
Campo 4: Il nome del dizionario da utilizzare "it_IT" o "it_CH" o ... (senza aggiungere le estensioni .aff o .dic dei files. Questa voce � particolarmente utile se vogliamo creare i nostri dizionari specializzati. Per esempio it_INFORMATICA o it_DIRITTO. Sar� sufficiente aggiungere un'altra linea al file dictionary.lst cambiando solo il nome del dizionario e il correttore ortografico di OpenOffice.org controller� i vostri documenti usando in successione tutti i dizionari elencati e presenti nella cartella wordbook per la lingua italiana).

Ora � necessario chiudere tutte le applicazione di OpenOffice.org attive, compreso il quick start. Questo passo serve per permettere ad OOo di vedere le modifiche apportate e di utilizzarle.

Successivamente :

1) Aprite un nuovo documento di OpenOffice.org e scegliete dalla barra dei men� il comando:
Strumenti->Opzioni->Impostazioni Lingua->Lingue (Tools->Options->Language settings->Languages nella versione inglese)
2) Impostate nell'elenco a discesa che vedete sulla destra la lingua italiana quale linguaggio di default per i vostri documenti
3)selezionando l'elemento Linguistica (Writing Aids)che troverete nella sezione Impostazioni Lingua (Language Settings) e premendo il tasto di modifica della sezione Moduli Linguistici Disponibili (Available language modules) collocato sulla destra della finestra di dialogo, potrete accedere alla sezione che vi permetter� di impostare il correttore in italiano. Sar� sufficiente scegliere la nostra lingua dall'elenco a discesa che mostra tutti i dizionari disponibili nella Suite, facendo attenzione a selezionare con un segno di spunta, la funzione di correzione ortografica nel riquadro immediatamente sottostante l'elenco a discesa.

Attenzione: La denominazione dei comandi di men� o delle finestre pu� subire un cambiamento o la loro collocazione pu� essere modificata nel passaggio da una versione ad un'altra. 

Attenzione: gli utenti di OpenOffice.org per MS Windows, devono riavviare anche il quickstarter della Suite, la piccola icona con i gabbiani (versione 1.0) che dovrebbe apparire sulla System Tray nell'angolo in basso a destra dello schermo, proprio a fianco dell'orologio di sistema. � sufficiente cliccare col tasto destro del mouse sull'icona e scegliere esci o exit dal men� di contesto. Successivamente, si dovr� effettuare l'installazione del dizionario, mantenendo disattivato il quickstarter durante tutta la procedura, chiudere il documento dal quale si sono modificate le opzioni generali e riaprirne uno nuovo per verificare il funzionamento del correttore italiano. 

Questo � tutto, il correttore ortografico in italiano dovrebbe funzionare in modo adeguato.


**********************************
4. Integrazione con altri prodotti

Il dizionario italiano pu� essere usato con vari programmi.
Qui di seguito ne vengono riportati alcuni:
* OpenOffice.org: in modo nativo
  Caratteristiche: controllo ortografico durante la digitazione o su richiesta
  http://it.openoffice.org

* Mozilla-ThunderBird: in modo nativo
  Caratteristiche: controllo ortografico solo su richiesta)
  http://www.mozilla.org/products/thunderbird/
* Mozilla con il client di posta: in modo nativo
  Caratteristiche: controllo ortografico solo su richiesta
  http://www.mozilla.org
* Mozilla-FireFox: con l'estensione SpellBound
  Caratteristiche: controllo ortografico durante la digitazione o su richiesta
  Nota: il dizionario � installato in una directory differente
  http://spellbound.sourceforge.net/
* Mozilla-ThunderBird: con l'estensione Thunderbird In-Line SpellChecker
  Caratteristiche: controllo ortografico durante la digitazione
  http://www.supportware.net/mozilla/#ext8
Nota: � possibile scaricarsi i dizionari anche dal sito di mozilla (ma non � detto che siano aggiornati all'ultima versione): http://www.mozdev.org/

* ...


******************************************************************
5. Altri correttori ortografici che fanno uso di questo dizionario

Questo dizionario pu� essere usato dai seguenti programmi di correzione ortografica:
* MySpell. http://lingucomponent.openoffice.org/
* Aspell. http://aspell.net/


************************************************
6. Ringraziamenti (aggiornato al 20 Luglio 2003)

Gli utenti di un elaboratore di testi, come di qualunque altra applicazione software che utilizzi un correttore ortografico, spesso sottovalutano il lavoro necessario per produrre questo strumento fondamentale. I problemi sono molti: la lingua italiana non � statica, ma in continua evoluzione; semplici errori di digitazione in un dizionario digitale possono portare a gravi conseguenze sul funzionamento del correttore; ci� che si usa correntemente nella lingua parlata non sempre � corretto in quella scritta.

Proprio per ovviare a tali difficolt�, i volontari della Comunit� OpenOffice.org hanno partecipato attivamente al controllo di qualit� del contenuto dell'attuale dizionario per la correzione ortografica. Con questa sezione si vuole rendere merito a tutti coloro che hanno collaborato in questa attivit�.

� possibile vedere l'elenco dei volontari all'interno del file thanks.txt


**********************************
***********  English  ************
**********************************

The Italian dictionary.

INDEX

1. License
2. Warnings (to read absolutely)
3. How to install it in OpenOffice.org
4. Others product that can use this dictionary
5. Other spell checking program that can use this dictionary
6. Thanks


**********
1. License

The Italian dictionary and affix file for the MySpell spell-checker used by OpenOffice.org are released by Davide Prina davideprina<at>yahoo<dot>com (please change <at> with @ and <dot> with . in order to contact the authors) under the terms and conditions of the GNU General Public License (GPL) version 2.0 or above. The user, distributor and/or programmer after accept this license when he/she uses, distributes and/or modifies these files. He/she must agree with every term and condition included in the chosen license.

A copy of the licenses applicable to this work is available at:

http://www.gnu.org/


********************************
2. Warnings (to read absolutely)

Warning: Since the release of OpenOffice.org 1.1 the Italian dictionary is directly integrated into the applications and so it doesn't need any manual installation. Probably in OpenOffice.org there is not the last version so it is better to install it as show below. It is a good norm to verify that the dictionary has been activate. You can do so simply by selecting from the menu bar Tools->Options->Language Settings->Languages and by checking that the item "Italian (italy)" is selected and a slash with the letters ABC is displayed to its side.

If the dictionary is not activated yet, you can perform this action by selecting from the menu bar Tools->Options->Language Settings->Writing Aids and by clicking on the "Edit..." button you will find in the dialog window. In the new dialog that will appear, you can activate the dictionary.

In the case the dictionary doesn't still work or you wish to upgarde the version of your installed dictionary you can follow the procedures explained below.


**************************************
3. How to install it in OpenOffice.org

Note: new dictionary versions are avaiable from here: http://sourceforge.net/projects/linguistico/

a)	To install these dictionaries in OOo build 641c and greater:
  A.	Unzip the dictionary files, *.aff and *.dic, 
		into your <OpenOffice.org>\user\wordbook\*.*  directory

Attention: since OpenOffice.org 1.0.1 the right folder for the dictionary is <OpenOffice.org>\share\dict\ooo

  B.	Edit the dictionary.lst file that is in that same directory 
		using any text editor to register a dictionary for a specific 
		locale (the same dictionary can be registered for multiple locales).

For example: 
To add the it_IT.zip dictionary under the Italian (Italy) locale to OOo you would:
cd <OpenOffice.org641>\user\wordbook (or <OpenOffice.org>\share\dict\ooo)
unzip it_IT.zip  


b)		And then, using any text editor, add the following line to dictionary.lst:
		DICT it IT it_IT

This line tells OOo to register the affix file it_IT.aff and the wordlist it_IT.dic 
to the locale it IT, which is Italian (Italy). The specific fields of this line are:

Field 1: Entry Type "DICT" is the only supported entry type so far
Field 2: Language code from Locale "en" or "it" or "pt" ...
				(see {ISO Language Code} page)
Field 3: Country code from Locale "IT"  ... 
				(see {ISO Country Code} page)
Field 4: Root name of Dictionary "italian" or "it_it"  ... 
				(do not add the .aff or .dic extensions to the name)


c) Start up OpenOffice.org. and go to:

Tools->Options->LanguageSettings->WritingAids

Hit "Edit" and use the pull down menu to select your locale, Italy, 
and then make sure to check the MySpell SpellChecker for that locale. 

That it! Your dictionary is installed and registered for that language.


**********************************************
4. Others product that can use this dictionary

The Italian dictionary can be used with many programs.
Here is some programs that can use this dictionary:

* OpenOffice.org
  spell checking when you type or upon request
  http://it.openoffice.org

* Mozilla-ThunderBird
  spell checking only upon request
  http://www.mozilla.org/products/thunderbird/
* Mozilla con il client di posta: in modo nativo
  spell checking only upon request
  http://www.mozilla.org
* Mozilla-FireFox: with SpellBound extension
  spell checking when you type or upon request 
  Note: the dictionary is installed under a different directory
  http://spellbound.sourceforge.net/
* Mozilla-ThunderBird: with "Thunderbird In-Line SpellChecker" extension
  spell checking when you type
  http://www.supportware.net/mozilla/#ext8
Note: you can download dictionary at mozilla site: http://www.mozdev.org/

* ...
		      

************************************************************
5. Other spell checking program that can use this dictionary

This dictionary can be used with different spell checking programs:
* MySpell. http://lingucomponent.openoffice.org/
* Aspell. http://aspell.net/


*********
6. Thanks

The users of a word processor, like of whatever application that utilizes a spell checker, often underestimate the work needed to produce this basic tool. There are a lot of problems: the Italian language is not static, but always evolving; simple misspelled words in a digital dictionary can cause several problems to the spell checker; what is commonly used in the spoken language, it is not always right in the written one.

Just to overcome such difficulties, the volunteers of the OpenOffice.org Community have actively took part into the quality check of this dictionary content. With this section, we want to give the right credit to all people who have collaborated in this activity.

You can read the volunteers names in the file thanks.txt

