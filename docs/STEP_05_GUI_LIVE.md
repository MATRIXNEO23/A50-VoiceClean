# Passo 5 — Interfaccia Windows per audio live

**A50VoiceClean-Control.exe** è un pannello di controllo Win32 nativo, con:
- selezione del microfono ASTRO A50 Voice e dell'uscita virtuale VB-CABLE;
- cursori guadagno, probabilità minima VAD e livello minimo della voce;
- RNNoise e VAD attivabili/disattivabili separatamente;
- pulsanti Avvia / Ferma;
- arresto ordinato tramite evento Windows, con chiusura forzata di sicurezza in caso di blocco;
- log della pipeline in \`%LOCALAPPDATA%\A50VoiceClean\live.log\`.

Il pannello avvia \`A50VoiceClean-Live.exe\` dalla **stessa cartella**. I cursori vengono applicati all'**avvio del flusso**: per aggiornarli occorre premere Ferma e poi Avvia. Regolazioni live senza riavvio e voce femminile live sono obiettivi successivi.

## Criteri prima di dichiararla pronta
- [ ] CI Windows verde e tutti i target compilati.
- [ ] Avvio, stop e riavvio reale su Astro A50 Gen 4 e Windows 10 x64.
- [ ] VB-CABLE selezionato e CABLE Output utilizzabile come ingresso in Jarvis.
- [ ] Nessun eco, glitch, feedback o microfono che resta occupato dopo Ferma.
- [ ] Soglie VAD, pulizia e comprensibilità testate su voce reale.
- [ ] Carico CPU/RAM e latenza misurati sul PC i3-2100 senza AVX2.
- [ ] Gestione guasti e log verificati.

Il codice resta **sperimentale**: senza i test sul PC reale non è una release completa. La vecchia GUI v0.2 per i WAV è distinta da questo pannello.
