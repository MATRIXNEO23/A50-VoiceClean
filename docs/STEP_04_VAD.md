# Passo 4 — VAD intelligente, regolabile (in sviluppo)

Combina due criteri per decidere quando trasmettere la voce:
1. Probabilità di parlato restituita da RNNoise.
2. Livello RMS minimo della voce filtrata (soglia in dBFS).

Conferma iniziale 20 ms; pre-roll 60 ms per preservare le consonanti iniziali; tenuta 250 ms per non tagliare le pause; dissolvenza 5/60 ms. Diversamente dal gate basato solo su volume, un colpo rumoroso non apre automaticamente il microfono se RNNoise non lo classifica come parlato.

## Parametri della diagnostica CLI
- \`--vad-prob 0.65\` = probabilità minima RNNoise.
- \`--voice-min-db -46\` = livello minimo voce.
- \`--no-vad\` = disattiva il filtro VAD (la sola pulizia RNNoise resta attiva).
- \`--no-denoise\` = bypass RNNoise; disattiva anche VAD intelligente.
- Se VAD è attivo, il vecchio amplitude-only gate viene disabilitato automaticamente per non tagliare le consonanti.

## Criteri
- [ ] CI Windows/Linux verde con test VAD.
- [ ] Confronto reale Astro A50: parole normali, bisbigli, rumori improvvisi.
- [ ] Verificare consonanti S/F all'inizio e alla fine della frase.
- [ ] Test con altre persone: la sola probabilità di parlato NON è biometria; una voce vicina può essere rilevata.
- [ ] Latenza di preroll misurata sul PC i3-2100.
- [ ] GUI con cursori di sensibilità, volume e soglia.

Non è un'app pronta: senza test reale non considerare completato il passo né pubblicare una versione definitiva.
