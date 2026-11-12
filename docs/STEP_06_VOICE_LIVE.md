# Passo 6 — Modifica della voce in tempo reale (DSP leggero)

La pipeline per il microfono ASTRO A50 Voice applica:
1. Guadagno e passa alto;
2. RNNoise a 48 kHz, se attivato;
3. VAD, se attivato;
4. Modifica vocale DSP a 48 kHz, **dopo** RNNoise/VAD, per non disturbare il riconoscimento del parlato;
5. Ricampionamento verso l'uscita VB-CABLE.

Nel pannello **A50VoiceClean-Control.exe**: casella *Attiva trasformazione*, e cursori indipendenti:
- Tonalità: -12/+12 semitoni.
- Timbro/risonanza: -100/+100%.
- Bassi: -12/+12 dB.
- Brillantezza: -12/+12 dB.
- Intensità/mix: 0–100%.

Valori iniziali indicativi (non garanzia di femminilizzazione naturale): tono +4, timbro +35%, bassi -3 dB, brillantezza +2 dB, mix 100%.

## Limitazioni
- Il pitch shifter è un DSP granulare leggero, non un modello di voice conversion e **non** modifica indipendentemente i formanti vocali. Potrebbero esserci artefatti e un timbro artificiale.
- Modifiche ai cursori: si applicano al successivo **Avvia**; non sono ancora hot-reload.
- La **velocità** (tempo) resta invariata in una conversazione continua. Tentare un rallentamento continuo crea ritardo crescente: nel progetto la velocità è modificabile solo nell'editor WAV offline.
- Nessun installer o prodotto completo viene dichiarato pronto prima dei test sul PC e sulle cuffie reali.

## Collaudi
- [ ] CI Windows/Linux verde: verifica DSP a 48 kHz e bypass.
- [ ] Test in ascolto reale dei cursori su Astro A50 Gen4.
- [ ] Verifica nessun clipping, latenza anomala o interferenza con VAD.
- [ ] Misure CPU/RAM e confronto audio originale/filtrato/trasformato.
- [ ] Profili salvati e selezionabili nel pannello live.
- [ ] Slider in hot-reload (senza interrompere la trasmissione) per impostazioni sicure.
