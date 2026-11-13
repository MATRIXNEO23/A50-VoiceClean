# Passo 13 — Chiarezza del parlato a basso volume (preview)

## Sintomo osservato
Su ASTRO A50 Gen 4, con parlato naturale/basso, volume debole e alcune
consonanti poco intelligibili, alzare il guadagno statico non basta e
può amplificare rumore/saturare. Il riconoscimento dipende anche dal
motore STT della singola applicazione (Lumi/Jarvis).

## Correzioni al percorso audio
- **Gate nascosto eliminato:** la pipeline live non usa più di default
  il vecchio gate a soglia -38 dBFS, che poteva tagliare parlato piano
  persino quando VAD era disattivato.
- **VAD disattivato come impostazione iniziale** nel pannello. Resta
  disponibile per ambienti rumorosi, ma può attenuare consonanti deboli.
- **Parlato piano: più chiaro**, abilitato inizialmente, **disattivabile
  in tempo reale** senza modificare le impostazioni di Windows:
  - miscela controllata del segnale pulito RNNoise e della voce originale,
    soprattutto durante il parlato, per recuperare articolazione;
  - lieve attenuazione dei bassi sotto ~210 Hz contro la voce rimbombante;
  - lieve enfasi ampia tra ~1.6 e ~4.3 kHz per la comprensibilità;
  - guadagno automatico con presenza vocale, massimo **~+16,9 dB**,
    mantenimento ~300 ms attraverso consonanti brevi, ritorno a unità
    durante le pause; limite picchi ~-0,9 dBFS;
  - zero reti/API, zero memoria dinamica per frame, nessuna istruzione AVX2.
- **Pulizia RNNoise** 0–100%, iniziale **80%**, regolabile in diretta:
  100% privilegia soppressione rumore, valori inferiori permettono
  maggior recupero consonanti ma anche più rumore nelle frasi.
- Nuovi controlli salvabili in profili INI con retrocompatibilità dei
  vecchi file: clarity (0/1), denoise_strength (0–100).
- **Interfaccia Windows ridimensionabile e massimizzabile**, scroll
  verticale/orizzontale e rotellina, evitando controlli irraggiungibili
  su schermi meno alti di 794 px.

## Configurazione di partenza consigliata
1. Input VoiceClean: **Microfono ASTRO A50 Voice**.
2. Output VoiceClean: **CABLE Input** (verso il microfono virtuale).
3. Windows uscita: **Cuffie ASTRO A50 Game**, sempre (audio Lumi/musica).
4. Windows ingresso / input Lumi: **CABLE Output**.
5. RNNoise **ON**, VAD **OFF**, trasformazione **OFF** (prima diagnosi).
6. Parlato piano **ON**, pulizia RNNoise **80%**, guadagno manuale
   inizialmente **0 dB**: l'AGC lavora già; evitare di partire da +12
   o +24 dB perché il segnale si può saturare prima della pulizia.
7. Se rimane rumore, aumentare pulizia verso **90–100%**; se le
   consonanti restano smorzate, ridurla verso **60–70%**, ascoltando
   anche come le parole vengono trascritte dall'STT.

## Limiti e convalida
Questo è un DSP euristico leggero, **non** un sistema di correzione
automatica dei difetti di articolazione, né un nuovo modello di STT.
Non può recuperare informazione vocale che il microfono non cattura.
L'AGC non deve aumentare il rumore quando RNNoise non identifica voce:
un test hardware resta essenziale. Le prove CI verificano la build,
la stabilità numerica, il controllo picchi e la persistenza dei profili,
non la qualità soggettiva né il tasso d'errore di Lumi.

## Checklist reale
- [ ] Parlato basso, medio e forte: ascolto e registrazione campioni;
      nessuna consonante troncata.
- [ ] Confronto «Parlato piano» ON/OFF e pulizia 60/80/100%.
- [ ] Parole con S, F, T, C; trascrizione Lumi/Jarvis comparata.
- [ ] Nessun fruscio sollevato in pause o clip a voce forte.
- [ ] CPU e latenza su i3-2100, 10 GB RAM, Windows 10.
- [ ] Output Windows resta Astro A50 Game anche con monitor aperto.
- [ ] Finestra massimizzata e scrollbar su schermo 1366x768.
- [ ] Profili precedenti caricabili, e nuovi profili salvabili/ricaricabili.
