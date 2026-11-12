# Passo 9 — Meter audio originale/filtrato e probabilità di voce

A50VoiceClean-Control.exe mostra due indicatori live:
- **Input:** segnale originale ricevuto dalle Astro A50 Voice, prima dell'elaborazione.
- **Filtrato:** segnale inviato a VB-CABLE dopo elaborazione, inclusi RNNoise, VAD e modifica vocale.

I livelli sono espressi in dBFS e visualizzati con scala -70 ... 0 dBFS (barra 0 ... 1000). Accanto compare la probabilità di parlato restituita da RNNoise. Questo indicatore **non certifica l'identità del parlante**.

La telemetria è condivisa tra processo audio e GUI tramite memoria Windows atomica, senza file temporanei o rete. Aggiornamento GUI 10 volte/s.

## Checklist
- [ ] CI Windows verde e test IPC.
- [ ] Su Astro A50 reali, meter Input reagisce alla voce e ai rumori presenti.
- [ ] Meter Filtrato è silenzioso durante i rumori non vocali, salvo limiti del VAD.
- [ ] Nessun ulteriore click/glitch dovuto alla telemetria.
- [ ] Test consumo CPU su i3-2100.
- [ ] Confronto audio reale Jarvis prima e dopo il filtro.

Non pubblicare una release definitiva senza il collaudo fisico.
