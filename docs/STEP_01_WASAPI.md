# Passo 1 — Acquisizione del microfono reale (WASAPI)

## Obiettivo e criteri di accettazione
Questo passo implementa una **registrazione diretta del microfono tramite WASAPI**, con identificazione dell'ingresso ASTRO A50 Voice, elenco dispositivi, livello in dBFS, uscita WAV mono 16 bit PCM sul disco. Non richiede una modifica del firmware o del driver ASTRO. Elabora l'audio in memoria costante (scrittura WAV in streaming).

Il passo è COMPLETO soltanto dopo:
- [x] Codice WASAPI disponibile nel repository.
- [ ] GitHub Actions Windows verde (compilazione + test senza hardware).
- [ ] Sulle vere Astro A50 Gen 4 l'elenco mostra il microfono corretto.
- [ ] Registra 10 secondi di parlato a 16 kHz o alla frequenza nativa effettiva.
- [ ] Il WAV contiene la voce senza interruzioni e il meter reagisce al livello del microfono.
- [ ] L'arresto rilascia correttamente il microfono, nessun errore di dispositivo.
- [ ] Consumi RAM/CPU misurati sul vero i3-2100.

## Prova sul Windows 10 dell'utente
Da PowerShell nella cartella contenente l'eseguibile:

~~~powershell
.\A50VoiceClean-MicTest.exe --list
.\A50VoiceClean-MicTest.exe --seconds 10 --output "prova-astro.wav"
~~~

Se il nome ASTRO A50 Voice non viene trovato, esegui il primo comando e specifica l'indice mostrato:

~~~powershell
.\A50VoiceClean-MicTest.exe --seconds 10 --device 0 --output "prova-astro.wav"
~~~

**Nota:** il valore 0 è un esempio; scegliere l'indice effettivamente elencato.

## Confini del passo
- Non modifica in tempo reale l'audio di Discord/Jarvis.
- Non applica ancora RNNoise o VAD.
- Non è il programma finito da distribuire all'utente.
- Non può essere collaudato con un dispositivo ASTRO fisico sui runner standard GitHub Actions.
- Lo scopo è mettere in sicurezza l'input WASAPI prima di collegarlo all'uscita audio e al DSP.

## Successivi passi (separati)
2. Uscita audio in tempo reale verso dispositivo selezionabile e controllo latenza (es. VB-CABLE).
3. RNNoise con resampling 16 kHz → 48 kHz → frequenza di output e VAD robusto.
4. Transform live con pitch, timbro, bassi, brillantezza, mix, preset e anteprima.
5. Integrazione GUI, avvio automatico e collaudo end-to-end su Jarvis.
