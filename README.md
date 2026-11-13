# A50 VoiceClean

Software C++17 sperimentale per il microfono **Astro A50 Gen 4** su Windows 10 x64, indipendente da Logitech/ASTRO.

## Stato reale (12 novembre 2026)

Il codice comprende due prodotti distinti:

1. **Editor WAV v0.2**: `A50VoiceClean-GUI.exe`, modifica registrazioni WAV offline. Funziona indipendentemente dalle Astro. Include pitch/timbro/bassi/brillantezza/mix, velocità WAV, preview e preset. La v0.2 è un editor, **non un filtro microfono Windows**.
2. **Pipeline live sperimentale**: `A50VoiceClean-Control.exe` (pannello) + `A50VoiceClean-Live.exe` (motore), che acquisisce il microfono reale tramite WASAPI e invia l'audio elaborato a un'uscita audio virtuale come VB-CABLE. Il codice **compila su Windows e supera i test automatici**, ma **non è ancora stato collaudato sulle vere Astro A50 Gen 4**.

### Funzionalità implementate nel codice live

- Input WASAPI, scelta del dispositivo tramite **ID stabile**, aggiornamento elenco e fallimento sicuro se scollegato.
- Uscita WASAPI verso VB-CABLE, con controllo anti-feedback per evitare altoparlanti fisici.
- RNNoise a 48 kHz, conversione automatica dal microfono mono 16 kHz alla frequenza di uscita.
- VAD basato su probabilità RNNoise e soglia RMS: preroll, tenuta, fade, sensibilità modificabile.
- Guadagno, filtro passa alto, tono/pitch, timbro, bassi, brillantezza, mix; la trasformazione femminile è un **DSP sperimentale**, non un modello di clonazione vocale.
- Regolazioni di volume, sensibilità e timbro **durante** il flusso via memoria condivisa Windows.
- Preset Originale, Voce chiara, Voce femminile leggera e profili personalizzati in `%APPDATA%\A50VoiceClean\live_profiles.ini`.
- Meter dei livelli audio **prima e dopo** il filtro e probabilità di parlato, con aggiornamento 10 volte al secondo.
- Avvia/Ferma, spegnimento tramite evento Windows, arresto di sicurezza, log in `%LOCALAPPDATA%\A50VoiceClean\live.log`.

### Cosa NON è ancora confermato

- **Non** è stata verificata la qualità audio sul microfono A50 reale, né il riconoscimento delle consonanti in Jarvis.
- **Non** sono stati misurati CPU, RAM e latenza sul processore i3-2100: le vecchie stime di peso sono obiettivi, **non dati certificati**.
- La pipeline non crea un driver virtuale proprio: per usarla come microfono di Jarvis/Discord è necessaria un'uscita virtuale esterna (es. **VB-CABLE**). L'applicazione invia a **CABLE Input**; in Jarvis si sceglie **CABLE Output** come microfono.
- La voce femminile DSP può produrre artefatti e non offre correzione indipendente dei formanti; non garantisce una voce naturale.
- La **velocità del parlato in diretta non è regolabile**: una modifica continua del tempo senza tecnica adeguata accumulerebbe ritardo. Nell'editor WAV è regolabile.
- Cambiare la presenza stessa di RNNoise/VAD richiede Ferma e Avvia. I cursori volume/sensibilità/voce si aggiornano live.
- **Non è ancora una release completa** da consegnare come prodotto finito.

## Nuova anteprima audio live e installer unico (ancora da collaudare sulle Astro)

- **Ascolta la mia voce**: scegli ASTRO A50 Game, premi Avvia e senti la voce trasformata nelle cuffie mentre muovi i cursori; funziona anche **senza** VB-CABLE per la sola prova locale. Volume separato, inizialmente 20%. Con VB-CABLE, il segnale filtrato continua a essere disponibile alle app mentre ascolti nelle cuffie.
- **Installer unico Windows 10/11 x64**: il workflow [Build Windows single installer](https://github.com/MATRIXNEO23/A50-VoiceClean/actions/workflows/installer.yml) produce `A50-VoiceClean-Unified-Setup-Windows-x64-PREVIEW`, contenente `A50-VoiceClean-Setup-Win10-Win11-x64.exe`. Il setup include il pacchetto originale **VB-CABLE** opzionale (donationware VB-Audio, [condizioni](https://vb-audio.com/Services/licensing.htm)), esegue la procedura di installazione **ufficiale** e può richiedere il riavvio.
- **Disinstallazione**: da Impostazioni > App, con scelta di aprire la rimozione del driver VB-CABLE e di eliminare facoltativamente profili personali e log; per sicurezza entrambi rimangono indipendenti e non vengono eliminati automaticamente.
- **Windows predefinito**: dopo aver installato VB-CABLE e riavviato, imposta una sola volta **CABLE Output** come dispositivo microfono predefinito e predefinito di comunicazione. Le app che scelgono un ingresso specifico potrebbero richiedere di selezionarlo manualmente.
- **Stato di prova**: build e test CI non certificano la qualità o la latenza reale su Astro A50 e i3-2100. Non installare una seconda volta sopra una versione precedente senza prima salvare eventuali profili importanti.

Maggiori dettagli: [ascolto diretto](docs/STEP_11_HEADPHONE_MONITOR.md) e [installer](docs/STEP_12_INSTALLER.md).

## Hardware di riferimento

- Astro A50 Gen 4: firmware cuffie `39964.43`, firmware base `40372.43`.
- Driver microfono Microsoft `10.0.19041.1`, PCM mono 16 bit a 16.000 Hz (configurazione rilevata).
- PC target: Intel Core i3-2100 (senza AVX2), 10 GB RAM, Windows 10 64 bit.
- Nessuna modifica ai driver ASTRO o al firmware.

## Compilazione e test automatici

Con Visual Studio C++ e CMake:

~~~powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

La build live scarica **durante la compilazione** un sorgente RNNoise Windows a commit fissato; l'applicazione risultante funziona **senza Internet**. La relativa licenza è in `third_party_licenses/RNNOISE_COPYING.txt`.

Il workflow [GitHub Actions](https://github.com/MATRIXNEO23/A50-VoiceClean/actions/workflows/build.yml) produce artifact distinti:
- `A50-VoiceClean-v0.2-Windows-x64`: editor WAV tradizionale;
- `A50-VoiceClean-Experimental-Live-RNNoise`: `A50VoiceClean-Control.exe`, `A50VoiceClean-Live.exe`, file di licenza (solo test di sviluppo);
- `A50-VoiceClean-WASAPI-Step1-Diagnostic`: diagnostica del microfono.

Non distribuire il secondo artifact come **release finale** fino al collaudo hardware.

## Piano di validazione reale

1. Connettere Astro A50 Gen 4 e verificare che appaiano nel pannello come **ASTRO A50 Voice**.
2. Installare/selezionare un driver audio virtuale (VB-CABLE) e controllare l'assenza di feedback.
3. Parlare, bisbigliare, produrre rumori ambientali; confrontare meter Input/Filtrato e ascoltare il risultato.
4. In Jarvis selezionare **CABLE Output** come microfono e provare dettatura/consonanti S/F.
5. Regolare guadagno, soglie VAD e modificatore vocale; salvare/ricaricare i profili.
6. Misurare CPU, RAM, latenza e glitch sul PC i3-2100; correggere eventuali regressioni.

Per il dettaglio e le checklist: [docs/STEP_01_WASAPI.md](docs/STEP_01_WASAPI.md) fino a [docs/STEP_10_DEVICE_ID.md](docs/STEP_10_DEVICE_ID.md).

La v0.2 WAV resta un componente distinto e non deve essere confusa con il filtro live sperimentale.
