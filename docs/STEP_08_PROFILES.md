# Passo 8 — Profili di microfono e voce salvabili

Il pannello Win32 **A50VoiceClean-Control.exe** gestisce tre profili di partenza: Originale, Voce chiara e Voce femminile leggera. Il risultato naturale della trasformazione **non è garantito**; valori modificabili dall'utente.

Le regolazioni personalizzate possono essere salvate, richiamate e sovrascritte con conferma. Il percorso è \`%APPDATA%\A50VoiceClean\live_profiles.ini\`. Non richiede rete, cloud o driver ASTRO aggiuntivi.

Un profilo include: guadagno, probabilità VAD, soglia minima, pitch, timbro, bassi, brillantezza, intensità, checkbox RNNoise, checkbox VAD e modificatore voce.

**Controlli del rilascio**
- [ ] CI Windows verde e test INI.
- [ ] Prova fisica salvataggio/caricamento dopo riavvio su Windows 10.
- [ ] Verifica caricamento profili mentre il motore è attivo.
- [ ] Test end-to-end con Astro A50 e CABLE Output su Jarvis.
- [ ] Misurare consumo RAM, CPU e latenza sul vero i3-2100.

Il codice è un componente sperimentale finché non sono stati completati i test su hardware.
