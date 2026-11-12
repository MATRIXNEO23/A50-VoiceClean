# A50 VoiceClean

Progetto sperimentale **C++17 per Windows 10** dedicato al microfono **Astro A50 Gen 4**.

## Stato reale (v0.1)
È disponibile nel repository il codice del **primo prototipo offline**. Non è ancora un programma che acquisisce l'audio direttamente dalle cuffie, non crea un microfono virtuale e non modifica ancora la voce in tempo reale.

Il prototipo processa un **file WAV mono PCM a 16 bit** e può:
- regolare il guadagno di uscita;
- applicare un filtro passa-alto a circa 80 Hz per attenuare rombi e vibrazioni basse;
- utilizzare una soglia audio (noise gate) regolabile con tempo di tenuta e chiusura graduale.

Un noise gate **non sa riconoscere la voce**: alcuni rumori forti lo aprono. RNNoise e rilevamento vocale (VAD) sono passi successivi.

## Configurazione di riferimento
- Cuffie Astro A50 Gen 4: firmware `39964.43`
- Base station: firmware `40372.43`
- Driver microfono Windows: Microsoft `10.0.19041.1`
- Registrazione: mono, 16 bit, 16.000 Hz
- PC target: i3-2100 (senza AVX2), 10 GB RAM, Windows 10 64 bit

## Compilazione e test
Con Visual Studio C++ e CMake su Windows:
```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
Per elaborare una registrazione WAV esistente:
```powershell
.\build\Release\a50voiceclean.exe input.wav output.wav --gain-db 3 --threshold-db -38
```
Opzioni: `--no-gate` e `--no-highpass`. La pipeline è stata compilata e i test DSP sono passati in ambiente Linux; la compilazione su Windows va verificata tramite GitHub Actions. Il workflow carica un **prototipo da riga di comando** come artifact, non un installer pronto per il microfono.

## Prossime fasi
1. Acquisizione dal microfono reale con WASAPI, indicatori di livello e interfaccia C++ nativa.
2. Riduzione rumore con RNNoise, con conversione 16 kHz -> 48 kHz e misure di latenza/prestazioni.
3. VAD con soglia, pre-roll e rilascio per non tagliare consonanti o brevi pause.
4. Uscita verso un dispositivo audio virtuale selezionato dall'utente, utilizzabile con Jarvis/Discord, mantenendo intatto il driver Astro originale.
5. Effetto voce opzionale con regolazioni indipendenti di tonalità, bassi, timbro/formanti, brillantezza, mix e profili. La velocità in tempo reale va testata per la latenza; una voce femminile naturale non è garantita con DSP leggero.
6. Verifica CPU, RAM e dimensioni sul vero i3-2100. I valori discussi sono obiettivi indicativi, **non misurazioni**.

Software non affiliato a Logitech/ASTRO.
