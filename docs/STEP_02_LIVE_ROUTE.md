# Passo 2 — routing live WASAPI (ramo di sviluppo)

**Scopo:** collegare il microfono Astro A50 Voice a un'uscita WASAPI, tipicamente VB-CABLE/CABLE Input, **senza modificare i driver Astro**.

Il modulo \`src/live_route_win.cpp\` usa WASAPI condiviso per leggere il microfono e scrivere l'audio verso un endpoint selezionato. Applica gli attuali filtri C++ (guadagno, passa alto, gate), converte la frequenza di campionamento con interpolazione lineare e usa un buffer con limite di 200 ms per evitare ritardi crescenti.

## Criteri verificabili
- [x] Codice per enumerare ingressi e uscite e scegliere ASTRO A50 Voice.
- [x] Pipeline WASAPI microfono -> DSP -> uscita.
- [x] Protezione contro feedback acustico: predefinito solo uscite virtuali; override esplicito.
- [x] Resampling 16k/48k e conversione PCM/float secondo mix format.
- [ ] CI Windows verde, test CLI senza hardware.
- [ ] Collaudo reale con ASTRO A50 Gen 4 e VB-CABLE.
- [ ] Misura latenza, memoria, CPU, presenza di glitch e deriva sul PC i3-2100.

## Come si proverà (SOLO dopo CI verde)
Serve un dispositivo virtuale come VB-CABLE già installato. Non avviare la pipeline verso gli altoparlanti per evitare fischi/feedback.

In PowerShell:
~~~powershell
.\A50VoiceClean-Live.exe --list
.\A50VoiceClean-Live.exe --seconds 30 --no-gate
~~~

Il microfono viene cercato come **ASTRO A50 Voice**, e il dispositivo di uscita come **CABLE Input**. Per forzare un indice da \`--list\`:

~~~powershell
.\A50VoiceClean-Live.exe --input 0 --output 2 --seconds 30
~~~

Gli indici 0 e 2 sono solo esempi e **vanno sostituiti con quelli realmente mostrati**. Per verificare l'audio pulito in un'app, seleziona **CABLE Output** come dispositivo microfono. Premi Ctrl+C per terminare una sessione senza durata.

## Limiti
Questa **non è la versione finale**: il routing live è ancora da collaudare sul PC reale, il filtro attuale è un semplice gate (non RNNoise e non VAD robusto), e le regolazioni voce femminile attualmente funzionano soltanto nell'editor WAV. L'anteprima grafica, gestione dispositivi e profili live vengono dopo la validazione dell'audio pass-through.

Niente app pubblicata come pronta fino ai test end-to-end con Jarvis.
