# 🤝 Contribuer à OptimizeKit

Merci de contribuer ! Ce projet est un outil C++20/Win32 (fenêtre Direct2D native + dashboard web optionnel), conçu pour rester **sans dépendance, réversible et honnête**. Ces règles gardent la base de code saine.

## 📋 Avant de commencer

- **Fork & branche** : `git checkout -b feat/ma-feature` depuis `main`.
- **MinGW-w64 requis** : g++ 13+ (recommandé : [winlibs](https://winlibs.com/) ou MSYS2). Pas de dépendance MSVC — le projet compile avec `-static`.
- **Un tweak = une PR** : gardez les changements ciblés et testés sur votre propre machine.

## 🏗️ Architecture rapide

```
src/
  app/       main.cpp (CLI, bootstrap fenêtre native D2D par défaut)
  ui/        ui.cpp — rendu Direct2D + DirectWrite, onglets (Dashboard, Tweaks,
             Gaming, Firmware, Storage, Startup, Settings…)
  core/      chaque module une paire .h/.cpp :
             tweaks.cpp (catalogue 77 tweaks + apply/restore), firmware.cpp,
             drvupdate.cpp, security.cpp, diskscope.cpp, reducer.cpp,
             netprofile.cpp, storage.cpp, ram.cpp, startup.cpp, engine.cpp…
  server/    server.cpp — dashboard web optionnel (httplib, loopback uniquement)
web/         dashboard web optionnel (vanilla JS, embarqué dans l'exe via embed_web.py)
resources/   .rc (icône, manifest, version), manifest requireAdministrator
tools/       make_icon.py (PIL → .ico), import_gamelogos.py
PowerShell/  moteur de tweaks OptimizeKit.ps1 (alternative sans exe)
```

**Règles du projet**

1. **Réversibilité obligatoire** : tout ce qui écrit dans le registre, un service ou un fichier doit avoir un chemin de restauration. Dans `tweaks.cpp`, chaque entrée du catalogue porte les valeurs d'origine (`OriginalValue`) et une branche restore ; testez les deux directions sur votre machine.
2. **Pas de dépendance externe** : pas de DLL tierce, pas de WebView2 dans le chemin par défaut, pas de téléchargement à l'exécution. Le web embarqué est du JS vanilla, compilé dans l'exe.
3. **Pas d'APIs non documentées** : registre, WMI, SetupAPI, services, powercfg, bcdedit — uniquement des interfaces publiques Microsoft.
4. **Honnêteté des descriptions** : pas de « +30 FPS ». Dites ce que le tweak change réellement, et si l'effet est marginal, dites-le. Marquez les risques (`CAUTION`) et les nécessités admin.
5. **Le firmware est en lecture seule** : jamais d'écriture NVRAM/UEFI. Les guides BIOS pointent vers les pages constructeurs.
6. **Formatage** : C++20, 4 espaces, accolades K&R, noms en lower_snake_case pour les fonctions. Les chaînes UI vont dans les tableaux i18n (`web/app.js`) et les labels natifs.

## ✅ Checklist avant PR

- [ ] `build.bat` compile sans warning nouveau.
- [ ] Vous avez testé **apply ET restore** du tweak sur une vraie machine Windows 10/11 (ou un VM).
- [ ] Le tweak apparaît dans `--list` avec un id stable.
- [ ] Si touché au web : `node --check web/app.js` passe, et les endpoints répondent en mode `--web`.
- [ ] Si touché au natif : la fenêtre D2D s'ouvre, tous les onglets se rendent, aucun crash en changeant d'onglet.
- [ ] CHANGELOG.md mis à jour (section Unreleased).
- [ ] Pas de changement de version dans votre PR — le mainteneur bump la version au release.

## 🐛 Signaler un bug

Ouvrez une issue avec :

1. Version d'OptimizeKit (menu À propos) et build (statique/CI).
2. Windows 10 ou 11, build exact (`winver`), session admin ou non.
3. Étapes précises de reproduction, et ce qui était attendu.
4. Pour un tweak : l'id du tweak (`--list`), le résultat apply et le résultat restore.
5. Si possible, la sortie de `OptimizeKit.exe --info` (sans données personnelles).

**Ne postez jamais** de logs contenant des chemins personnels, noms de machines ou clés — anonymisez avant.

## 💡 Idées de features bien reçues

- Nouveaux tweaks documentés (avec valeur de restauration !)
- Meilleurs détails firmware (SMBIOS brut, journaux TPM)
- Export/import des profils de configuration
- Traductions (l'i18n web est en place, ajout de nouvelles langues)

## 📜 Licence

En contribuant, vous acceptez que vos contributions soient sous licence MIT du projet.
