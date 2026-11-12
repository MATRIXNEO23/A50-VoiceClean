# A50 VoiceClean

Prototipo sperimentale **C++17 / Windows 10** per il microfono delle cuffie **Astro A50 Gen 4**.

## Obiettivi
- Acquisizione microfono ASTRO A50 Voice (mono, PCM 16 bit, 16 kHz sulla configurazione di test).
- Uscita audio selezionabile, inclusa l'opzione di un dispositivo virtuale come **VB-CABLE**.
- Riduzione di ronzii bassi con filtro passa-alto; noise gate con soglia e tempo di rilascio regolabili.
- **Cambio voce leggero**, opzionale: innalzamento del tono e regolazione di timbro con DSP tradizionale, senza reti neurali.
- Livello d'ingresso e guadagno regolabili. Nessuna modifica ai driver ASTRO.

> **Stato**: prototipo da compilare e collaudare. Il noise gate **non distingue con certezza la voce dai rumori** e non sostituisce un vero Voice Activity Detector. Il filtro passa-alto non equivale a RNNoise. La modalità femminile è una sperimentazione DSP: non garantisce un risultato naturale. L'integrazione RNNoise, un VAD più robusto e le prove di latenza saranno fasi successive.

## Configurazione nota
- Headset: ASTRO A50 Gen 4, firmware `39964.43`
- Base: firmware `40372.43`
- Driver microfono: Microsoft `10.0.19041.1`
- Registrazione: mono, 16 bit, 16.000 Hz
- PC di riferimento: Intel Core i3-2100, Windows 10 a 64 bit

## Compilazione
Richiede Visual Studio 2022 con *Desktop development with C++* e CMake.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Eseguibile: `build/Release/A50VoiceClean.exe`. Un workflow GitHub Actions crea l'eseguibile come artifact in ogni push a `main`.

## Utilizzo
1. Collega le ASTRO A50 e apri il programma.
2. Seleziona il microfono **ASTRO A50 Voice** e un dispositivo di uscita (cuffie per prova, o **CABLE Input** per inviare l'audio a VB-CABLE).
3. Premi **Avvia** e regola **Guadagno**, **Soglia del gate**, **Tono**. La casella **Voce femminile (sperimentale)** attiva l'effetto.
4. Se usi VB-CABLE, seleziona **CABLE Output** come microfono in Jarvis/Discord.

**Attenzione**: se mandi l'uscita agli altoparlanti e il microfono li sente, può verificarsi un forte effetto eco o fischio. Parti con il volume basso. Durante l'uso con un microfono virtuale mantieni la frequenza/formato compatibili: alcuni driver virtuali lavorano solo a 48 kHz e richiederanno una successiva funzione di conversione o la selezione di un formato supportato.

Il progetto non è affiliato a Logitech/ASTRO.
