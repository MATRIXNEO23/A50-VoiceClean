# Passo 12 — Installer unico Windows 10/11 x64

## Obiettivo

L'installer `A50-VoiceClean-Setup-Win10-Win11-x64.exe` integra:
- `A50VoiceClean-Control.exe` — pannello Win32;
- `A50VoiceClean-Live.exe` — motore audio;
- licenza del filtro RNNoise;
- **opzionalmente**, il pacchetto originale completo **VB-CABLE** di VB-Audio Software, attualmente `VBCABLE_Driver_Pack46.zip`.

Il driver non è un modulo di Jarvis, Lumi o VoiceClean: appare in Windows come dispositivo audio virtuale. È un software separato di VB-Audio, donationware, [soggetto ai termini di distribuzione del produttore](https://vb-audio.com/Services/licensing.htm), con attribuzione e possibilità di sostenere il progetto.

## Installazione e disinstallazione

1. L'installer aggiunge VoiceClean a Programmi installati e crea i collegamenti nel menu Start.
2. **Controllo automatico anti-duplicazione:** tramite WMI (`Win32_PnPEntity`), il setup verifica la presenza del dispositivo driver **VB-Audio Virtual Cable**. Se è già installato, deseleziona il componente e non avvia il setup ufficiale, anche se la selezione viene cambiata manualmente. Il controllo viene ripetuto immediatamente prima di qualsiasi avvio. Il nome del driver PnP viene usato perché i nomi degli endpoint audio possono essere personalizzati.
3. Se il driver non è presente e scegli VB-CABLE, l'installer apre **il programma ufficiale VB-Audio** con autorizzazioni amministratore: clicca **Install Driver** e riavvia Windows quando richiesto. Non forza un'installazione non autorizzata o senza conferma. Se la verifica WMI fallisce, l'installer non avvia automaticamente il programma del driver: puoi gestirlo manualmente tramite il pacchetto ufficiale incluso.
4. Dopo il riavvio, per l'utilizzo con tutte le app che usano il microfono predefinito di Windows: apri `mmsys.cpl` dal collegamento «Audio Windows» e imposta **CABLE Output** come ingresso predefinito e di comunicazione. L'app invia la voce filtrata a **CABLE Input**. **L'uscita Windows (riproduzione) deve invece rimanere ASTRO A50 Game**, per continuare a sentire Lumi, musica e giochi. Dopo l'installazione del driver, Windows 10/11 potrebbe impostare automaticamente CABLE Input come uscita predefinita: controllala e ripristina manualmente le Astro se necessario.
5. La disinstallazione di VoiceClean è disponibile in Impostazioni > App. Alla rimozione, puoi **mantenere** VB-CABLE per altri programmi oppure aprire il suo installer ufficiale per scegliere **Remove Driver** e riavviare. I profili salvati in AppData vengono **conservati per impostazione predefinita**, ma il programma di disinstallazione offre una scelta esplicita per eliminare anche profili e log.

## Limiti

Non esiste una API Windows ufficiale stabile per impostare arbitrariamente il microfono predefinito a livello di sistema: per evitare interventi non documentati si offre un collegamento alla finestra audio di Windows. Alcune app che hanno selezionato un microfono specifico continueranno a usarlo finché non lo cambi.

Questa è una **build di prova**. Servono test di installazione, aggiornamento, disinstallazione, rollback e qualità/latency reali sulle Astro A50 prima di definirla una release.

## Collaudo
- [ ] GitHub Actions Windows: test e compilazione dell'installer Inno Setup.
- [x] Il pacchetto originale VB-CABLE Pack46 viene verificato con SHA-256 fissato (BC44E8974191A5A2427D6C979E2DAB08DBB09925F3F435DFA2F0BA3E45B6EB90).
- [ ] Verificare la firma del driver durante l’installazione reale su Windows 10/11.
- [ ] Windows 10 x64, Windows 11 x64: installazione e rimozione senza alterare driver Astro.
- [ ] Riavvio del PC dopo gestione del driver.
- [ ] VoiceClean ascolto live con e senza VB-CABLE.
- [ ] Con VB-CABLE già installato (anche con endpoint rinominati): il setup non apre il programma VB-Audio, né sostituisce il driver.
- [ ] Su PC senza VB-CABLE: l'opzione rimane disponibile e il setup ufficiale parte solo su richiesta dell'utente.
- [ ] Con WMI non disponibile: niente avvio automatico del programma del driver; gestione manuale documentata.
- [ ] Dopo installazione del driver: output Windows su Astro A50 Game, input Windows su CABLE Output; Lumi e musica udibili anche con monitor microfono attivo.
- [ ] Selezionare CABLE Output predefinito, testare più applicazioni.
- [ ] Controllo risorse su i3-2100 e test accensione/spegnimento.
