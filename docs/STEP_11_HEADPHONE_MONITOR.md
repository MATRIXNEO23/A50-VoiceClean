# Passo 11 — Ascolto della voce modificata nelle Astro A50

## Funzione

Nel pannello `A50VoiceClean-Control.exe` è disponibile **Ascolta la mia voce** e un controllo separato per il volume d'ascolto. L'uscita viene limitata alla periferica **ASTRO A50 Game**; non consente automaticamente altoparlanti generici per evitare feedback.

I cursori di tonalità, timbro, bassi, brillantezza, mix, guadagno e sensibilità erano già aggiornati via memoria condivisa Windows; ora puoi ascoltarne le variazioni **mentre parli**, senza riavvio.

### Senza VB-CABLE

Se Windows non ha un dispositivo VB-CABLE, selezionando *Ascolta la mia voce* e premendo **Avvia** è possibile una **prova locale in cuffia**: l'audio modificato va alle Astro A50 Game, ma **non** a un microfono virtuale utilizzabile da tutte le app. Questo rende possibile regolare la voce prima di installare un driver virtuale.

### Con VB-CABLE

Se **CABLE Input** è presente, il motore manda sempre il microfono elaborato al cavo virtuale. Se attivi l'ascolto crea un **secondo** flusso verso ASTRO A50 Game, senza cambiare il segnale inviato alle app. Il volume del monitor è indipendente. Per usare la voce elaborata in tutti i programmi che rispettano i predefiniti Windows, scegliere **CABLE Output** come dispositivo di registrazione predefinito e di comunicazione predefinito. Alcune app con un input specifico memorizzato dovranno essere riconfigurate.

### Sicurezza

- Volume iniziale monitor 20%, regolabile in tempo reale da 0–100%.
- Smussamento volume 15 ms per limitare clic durante il movimento del cursore.
- La periferica di ascolto è bloccata a **ASTRO A50 Game**, non altoparlanti aperti.
- Se nelle cuffie è attivo il sidetone hardware Astro, disattivarlo per non sentire due voci sovrapposte.
- La velocità live non è ancora modificabile senza introdurre latenza; per i WAV resta disponibile.
- VAD, RNNoise e conversione vocale richiedono ancora test su hardware reale.

## Collaudi necessari

- [ ] Build e test CI Windows/Linux.
- [ ] Astro A50 Gen 4 reali: apri finestra, premi Avvia **senza** VB-CABLE e senti la tua voce filtrata.
- [ ] Sposta ogni cursore mentre parli, senza click o interruzioni.
- [ ] Attiva/disattiva *Ascolta la mia voce* senza interrompere l'uscita verso VB-CABLE.
- [ ] Misura consumo RAM/CPU su i3-2100 e latenza reale.
- [ ] Verifica installazione e scelta di VB-CABLE su Windows 10/11.
- [ ] Disattiva Sidetone Astro nella configurazione cuffie per eliminare sovrapposizione audio.

La build CI non sostituisce le prove con hardware reale.
