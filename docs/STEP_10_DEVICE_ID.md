# Passo 10 — Identificazione stabile delle periferiche WASAPI

La GUI ora conserva l'ID Windows effettivo di ogni endpoint audio e lo passa al processo di cattura/routing tramite \`--input-id\` e \`--output-id\`, anziché affidarsi agli indici di elenco che possono cambiare.

La CLI risolve gli ID contro l'elenco dei dispositivi attivi. Se uno degli ID è scomparso o non corrisponde più, **non** esegue automaticamente il routing verso un'altra periferica; restituisce errore. Resta attivo il controllo di sicurezza contro le uscite su altoparlanti fisici.

La CLI manuale supporta ancora \`--input N\`, \`--output N\` e l'autoselezione ASTRO/VB-CABLE, per retrocompatibilità.

## Test
- [ ] Windows CI verde, compilazione e test CLI.
- [ ] Avviare/fermare la pipeline sulle Astro reali.
- [ ] Scollegare/ricollegare cuffie e base station e premere Aggiorna in GUI.
- [ ] Verificare che la pipeline non si riavvii su una periferica sbagliata.
- [ ] Simulare rimozione di VB-CABLE: fallimento sicuro, nessun feedback.

Passo da validare con hardware reale prima del rilascio definitivo.
