# A50 VoiceClean

Software sperimentale **C++17** per le **Astro A50 Gen 4** su Windows 10. Non affiliato a Logitech/ASTRO.

## Stato reale — v0.2 (editor WAV, non microfono live)

Questa versione include interfaccia grafica Win32 e strumento CLI. Elabora **file WAV mono PCM16**. Non acquisisce ancora l'audio direttamente dalle cuffie, non crea un microfono virtuale e non cambia ancora la voce in tempo reale in Jarvis o Discord.

### Funzioni implementate

- Guadagno e soglia noise gate, passa-alto a 80 Hz.
- Modificatore opzionale senza AI: tonalita (-12/+12 semitoni), timbro/risonanza, bassi, brillantezza e intensita/mix.
- Velocita dei WAV 70–130%, con compensazione approssimativa della tonalita (**solo offline**).
- Anteprima dei primi 10 secondi, esportazione WAV modificato.
- Profili originali, chiaro, acuto e personalizzati salvati in AppData (A50VoiceClean/profiles.ini).

Il timbro cambia l'equalizzazione, **non** corregge i formanti come un modello AI. Il pitch shifter puo produrre artefatti. Una voce femminile naturale non e garantita. Il noise gate non distingue una parola da un rumore forte: **RNNoise e VAD restano da implementare**.

## Lavori in corso — passo 1: input WASAPI reale (non ancora validato su hardware)
Il sorgente `src/live_capture_win.cpp` aggiunge una utility di **diagnostica** Windows per enumerare i microfoni, individuare ASTRO A50 Voice e registrare direttamente 10 secondi su WAV con indicatore di livello in dBFS. Nella build viene chiamata `A50VoiceClean-MicTest.exe`.

La compilazione Windows è affidata a GitHub Actions; prima di dichiarare completato il passo sono indispensabili la verifica del workflow e un test sulle vere cuffie Astro A50 Gen 4. Questa utility non è una release per l'uso quotidiano e non sostituisce l'applicazione live finale. La checklist e le istruzioni sono in [docs/STEP_01_WASAPI.md](docs/STEP_01_WASAPI.md).

## Hardware di riferimento

- Astro A50 Gen 4, firmware cuffie 39964.43, firmware base 40372.43
- Driver Microsoft 10.0.19041.1, PCM16 mono 16.000 Hz
- PC target: i3-2100 (senza AVX2), 10 GB RAM, Windows 10 x64
- Non e richiesta alcuna modifica ai driver o al firmware.

## Download del prototipo

Apri la pagina [GitHub Actions](https://github.com/MATRIXNEO23/A50-VoiceClean/actions/workflows/build.yml), scegli un'esecuzione **verde** del workflow Build and test Windows voice editor e scarica l'artifact **A50-VoiceClean-v0.2-Windows-x64**.

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

1. Acquisizione live WASAPI da ASTRO A50 Voice, selezione input e meter audio.
2. Riduzione del rumore con RNNoise e conversione 16↔48 kHz.
3. Rilevamento vocale VAD, preroll, soglia/sensibilita, rilascio.
4. Uscita a microfono virtuale selezionabile per Jarvis, Discord e Windows (es. VB-CABLE, esterno).
5. Modificatore vocale live con regolazioni e profili. La velocita live necessita un progetto dedicato per evitare ritardi crescenti.
6. Prova reale su i3-2100: CPU, RAM, latenza, artefatti e qualita della voce.

I consumi discussi sono obiettivi indicativi, non misurazioni.
