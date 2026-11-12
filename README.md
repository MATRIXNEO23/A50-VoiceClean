# A50 VoiceClean

Software sperimentale **C++17** per le **Astro A50 Gen 4** su Windows 10. Non affiliato a Logitech/ASTRO.

## Stato reale — ramo principale v0.2, sviluppo acquisizione WASAPI

La v0.2 del ramo principale resta un editor WAV. Nel ramo di lavoro **feature/wasapi-microphone-capture-20261112** è presente il primo passo verso l'uso live: selezione ingresso da Windows, acquisizione WASAPI condivisa, livello audio in GUI e comandi Avvia/Ferma. Il flusso è acquisito in memoria e *non* è ancora filtrato, riprodotto o esposto come microfono virtuale. L'acquisizione non modifica driver o volume di sistema. Il corretto funzionamento sulla Astro A50 reale deve ancora essere verificato sul PC target.

L'editor WAV, la CLI e i profili restano disponibili. I moduli live successivi (RNNoise, VAD, uscita virtuale, cambio voce live) **non sono finiti**.

### Funzioni implementate

- Guadagno e soglia noise gate, passa-alto a 80 Hz.
- Modificatore opzionale senza AI: tonalita (-12/+12 semitoni), timbro/risonanza, bassi, brillantezza e intensita/mix.
- Velocita dei WAV 70–130%, con compensazione approssimativa della tonalita (**solo offline**).
- Anteprima dei primi 10 secondi, esportazione WAV modificato.
- Profili originali, chiaro, acuto e personalizzati salvati in AppData (A50VoiceClean/profiles.ini).

Il timbro cambia l'equalizzazione, **non** corregge i formanti come un modello AI. Il pitch shifter puo produrre artefatti. Una voce femminile naturale non e garantita. Il noise gate non distingue una parola da un rumore forte: **RNNoise e VAD restano da implementare**.

## Hardware di riferimento

- Astro A50 Gen 4, firmware cuffie 39964.43, firmware base 40372.43
- Driver Microsoft 10.0.19041.1, PCM16 mono 16.000 Hz
- PC target: i3-2100 (senza AVX2), 10 GB RAM, Windows 10 x64
- Non e richiesta alcuna modifica ai driver o al firmware.

## Download del prototipo

Apri la pagina [GitHub Actions](https://github.com/MATRIXNEO23/A50-VoiceClean/actions/workflows/build.yml). La build stabile v0.2 del ramo principale resta **A50-VoiceClean-v0.2-Windows-x64**. Gli artifact del ramo WASAPI, se prodotti, sono compilazioni di sviluppo per test e non una release completa.

Estrai lo ZIP e apri **A50VoiceClean-GUI.exe**. Non e un installer. Se non esiste ancora una build verde, gli eseguibili Windows non sono verificati.

Per provarlo:
1. Crea una registrazione di prova WAV mono PCM16 (consigliati 16 kHz).
2. Apri il WAV con il programma, spunta Modifica voce e seleziona uno dei preset.
3. Personalizza i cursori tonalita, timbro, bassi, brillantezza, intensita e velocita.
4. Premi Ascolta 10 s oppure Salva WAV. Per conservare i valori usa Salva profilo.

Attenzione: abbassa il volume delle cuffie prima delle prove.

## Build locale

Con Visual Studio C++ e CMake su Windows:

~~~powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

Output atteso: build/Release/A50VoiceClean-GUI.exe e build/Release/a50voiceclean.exe.

Esempio CLI:

~~~powershell
.\build\Release\a50voiceclean.exe input.wav output.wav --voice --pitch 5 --resonance 0.4 --bass-db -3 --brightness-db 2 --mix 1 --speed 1
~~~

Supporta anche --gain-db, --threshold-db, --no-gate e --no-highpass. DSP compilabile e testabile anche su Linux; GUI solo su Windows.

## Roadmap — non ancora implementata

1. Acquisizione live WASAPI, selezione input e meter audio: **codice aggiunto sul ramo di sviluppo; prova hardware ancora necessaria**.
2. Riduzione del rumore con RNNoise e conversione 16↔48 kHz.
3. Rilevamento vocale VAD, preroll, soglia/sensibilita, rilascio.
4. Uscita a microfono virtuale selezionabile per Jarvis, Discord e Windows (es. VB-CABLE, esterno).
5. Modificatore vocale live con regolazioni e profili. La velocita live necessita un progetto dedicato per evitare ritardi crescenti.
6. Prova reale su i3-2100: CPU, RAM, latenza, artefatti e qualita della voce.

I consumi discussi sono obiettivi indicativi, non misurazioni.
