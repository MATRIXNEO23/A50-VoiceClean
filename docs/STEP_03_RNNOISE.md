# Passo 3 — RNNoise sul microfono live (ancora da collaudare)

- Input Astro A50 Voice: WASAPI mono 16 kHz nella configurazione rilevata.
- Conversione a 48 kHz -> frame RNNoise 480 campioni (10 ms) -> conversione al mix format della periferica di uscita.
- Modulo attivato per impostazione predefinita. Per bypassarlo: \`A50VoiceClean-Live.exe --no-denoise\`.
- Vengono conservati i vecchi gate e guadagno; il VAD robusto separato e le trasformazioni vocali live arriveranno successivamente.

Dipendenza di compilazione: fork CMake di RNNoise per Windows \`jagger2048/rnnoise-windows\` fissato al commit \`d24c92c4f41eb786a18d25feea350cdf3319eee5\`. Codice RNNoise BSD-3-Clause e pesi secondo le rispettive condizioni di licenza upstream. Non richiede rete durante l'esecuzione, né AVX2.

## Criteri di completamento
- [ ] Test RNNoise su Windows CI (frame 480 campioni, nessun NaN).
- [ ] Compilazione release x64 senza AVX2.
- [ ] Confronto in ascolto reale con il microfono A50 e \`--no-denoise\`.
- [ ] Test rumori di fondo: ventole, televisione, voci lontane.
- [ ] Misura consumo CPU sul processore i3-2100 e latenza.
- [ ] Verifica percezione consonanti S/F e fine frase.
- [ ] Packaging corretto di eventuali file di licenza.

**Non è un prodotto finito**: software in prova con l'hardware reale ancora da verificare, non dichiarare come release. Non si può garantire la soppressione totale di tutte le altre voci.
