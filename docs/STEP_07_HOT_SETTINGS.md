# Passo 7 — Regolazioni in diretta senza riavviare

Il pannello Win32 e il processo audio WASAPI comunicano con una piccola **shared memory Windows**, con sequenza pari/dispari e accessi atomici interlocked. Non c'è polling su disco, rete, Python o server.

Le seguenti modifiche sono applicate durante l'elaborazione live:
- guadagno;
- sensibilità VAD (probabilità);
- soglia minima della voce;
- attivazione modificatore vocale;
- tonalità, timbro, bassi, brillantezza e intensità.

L'abilitazione/disabilitazione completa di RNNoise e del modulo VAD richiede ancora Ferma e Avvia per evitare reset improvvisi dei buffer. Non è stato aggiunto un controllo fittizio della velocità: il tempo del parlato live resta invariato.

## Test e limiti
- [ ] GitHub Actions Windows verde: compilazione e test della coerenza degli snapshot.
- [ ] Astro A50 reali: movimenti dei cursori mentre si parla, audio continuo senza click/drop.
- [ ] Confronto CPU/RAM sul vero i3-2100.
- [ ] Verifica di spegnimento, hotplug audio, arresto d'emergenza e recupero errori.
- [ ] Profili vocali e pulizia salvabili sul pannello live.
- [ ] Prova completa con CABLE Output come ingresso Jarvis.

Finché questi test reali non saranno eseguiti non va dichiarato un rilascio finale.
