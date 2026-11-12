# A50 VoiceClean v0.2 — checklist di collaudo

## Test automatici
- [x] DSP e trasformazioni WAV: build e test GCC su Linux locali
- [ ] Build e test MSVC Windows GitHub Actions
- [ ] Interfaccia Win32: apre un WAV mono PCM16
- [ ] Anteprima dieci secondi: audio originale vs modificato
- [ ] Esportazione WAV in cartella con caratteri accentati
- [ ] Cambio preset Originale / Chiara / Acuta
- [ ] Persistenza profilo dopo riavvio

## Test hardware da eseguire su i3-2100
- [ ] Affidabilita CPU, RAM e dimensioni file
- [ ] A50 Gen 4 16.000 Hz mono 16bit
- [ ] Nessun fruscio/fischio, clip o distorsione indesiderata
- [ ] Nessuna regressione v0.1 gate / passa-alto

**Non pronto per Jarvis live:** mancano WASAPI, RNNoise e microfono virtuale.