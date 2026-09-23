<div align="center">

<img src="assets/icon256.png" width="88" alt="OptimizeKit"/>

# ⚡ OptimizeKit

**Centre de contrôle Gaming Windows — un seul exe, vraie fenêtre native (sans navigateur), chaque tweak réversible.**

[![Release](https://img.shields.io/github/v/release/cameleonnbss/OptimizeKit?style=flat-square&color=ff3d57)](../../releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4?style=flat-square)](https://github.com/cameleonnbss/OptimizeKit)
[![Language](https://img.shields.io/badge/C%2B%2B-20-00599c?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![License](https://img.shields.io/badge/license-MIT-3dd68c?style=flat-square)](LICENSE)
[![Size](https://img.shields.io/badge/exe-~6,6%20Mo%20statique-ff9800?style=flat-square)](#-t%C3%A9l%C3%A9chargement)

**Fenêtre native D2D** · **Score gaming** · **77 tweaks réversibles** · **Optimisation intelligente** · **Panneau firmware (SecureBoot/TPM/VT)** · **Moteur de mise à jour des pilotes** · **5 centres de réglage** · **Base de ~300 jeux + 106 jaquettes** · **Centre réseau** · **Benchmark style AnTuTu** · **Catalogue de 79 outils** · **Palette Ctrl+K** · **12 thèmes** · **EN/FR**

`C++20 / Win32 / Direct2D` · `zéro dépendance` · `sans installation` · `aucun driver, aucune injection, aucun Edge`

[⬇️ **Télécharger v2.7.0**](../../releases/tag/v2.7.0) · [Nouveautés](#-nouveaut%C3%A9s-de-la-270) · [Démarrage](#-d%C3%A9marrage-rapide) · [Captures](#-captures-d%C3%A9cran) · [Sécurité](#%EF%B8%8F-s%C3%A9curit%C3%A9-dabord) · [English](README.md)

</div>

---

## 🆕 Nouveautés de la 2.7.0

| | |
|---|---|
| **Fenêtre native d'abord** | Double-clic → le dashboard liquid-glass Direct2D s'ouvre dans une vraie fenêtre Win32. **Aucun Edge, aucun WebView2, aucun navigateur, aucune application externe** — un exe statique, une fenêtre. (Le dashboard web reste dispo via `--app`.) |
| **28 nouveaux tweaks — 77 au total** | Alignés sur le catalogue actuel de [WinUtil](https://github.com/ChrisTitusTech/winutil) : Widgets, Localisation, Services en manuel (+ `SvcHostSplitThresholdInKB`), Delivery Optimization, suggestions du Store, point de restauration, menu fin de tâche, **blocage WPBT**, blocage auto-install Razer, IPv4/IPv6/Teredo, nettoyage disque + WinSxS, hibernation, BSoD verbeux, chemins longs, débloat Edge & Brave, UTC pour le dual-boot… tout réversible |
| **Panneau Firmware & plateforme** | État live en lecture seule : BIOS (fournisseur/version/date), carte mère, mode de boot (UEFI/Legacy), **Secure Boot**, **TPM**, VT-x/SVM + hyperviseur actif, modern standby vs S3, HPET, WPBT, dynamic tick / platform tick / TSC (bcdedit), niveau de dump kernel, redémarrage en attente — aussi en CLI avec `--firmware` |
| **Moteur de mise à jour des pilotes** | Âge réel de chaque pilote depuis le magasin (GPU/audio/réseau/chipset, bruit filtré), périphériques en erreur, **déclenchement du scan Windows Update** (le même orchestrateur que l'appli Paramètres), rescan PnP, pages constructeurs — aussi en CLI avec `--drvupdate` |
| **Nouveaux onglets natifs** | Firmware, Stockage (type de bus NVMe/SATA, plus gros fichiers, nettoyages un clic), Démarrage (toggles réversibles), Paramètres (persistés dans `config.json`) |
| **Nouvelle icône** | Éclair en dégradé sur tuile sombre, générée par `tools/make_icon.py` |

### Toujours valable depuis la 2.6.0

| | |
|---|---|
| **Réducteur de processus** | Table live CPU/RAM — mode éco (priorité basse + EcoQoS), kill, nettoyage, restauration complète |
| **Analyse sécurité** | Antivirus, pare-feu par profil, chaque port en écoute et son propriétaire, persistance au démarrage, motifs ransomware — correctifs en un clic |
| **Outils DiskScope** | Doublons, cibles de nettoyage, tailles de dossiers |
| **Vraies jaquettes** | Pochettes Steam/Epic des titres les plus joués |

Détail complet dans [CHANGELOG.md](CHANGELOG.md).

## 📸 Captures d'écran

| Gaming Center | Dashboard |
|---|---|
| ![Gaming Center](docs/screenshots/gaming.png) | ![Dashboard](docs/screenshots/dashboard.png) |

| Tweaks — interrupteurs ON/OFF instantanés | Jeux — vraies icônes |
|---|---|
| ![Tweaks](docs/screenshots/tweaks.png) | ![Games](docs/screenshots/games.png) |

| Centre réseau | Scanner PC |
|---|---|
| ![Network](docs/screenshots/network.png) | ![Scan](docs/screenshots/scan.png) |

| Benchmark — score style AnTuTu | Packs de thèmes (Ocean) |
|---|---|
| ![Benchmark](docs/screenshots/bench.png) | ![Theme Ocean](docs/screenshots/theme-ocean.png) |

| Outils — 79 lanceurs | Stockage |
|---|---|
| ![Tools](docs/screenshots/tools.png) | ![Storage](docs/screenshots/storage.png) |

## 📦 Téléchargement

| Release | Lien |
|---|---|
| **v2.7.0 (actuelle)** | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.7.0 |
| v2.6.0 | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.6.0 |
| Toutes les releases | https://github.com/cameleonnbss/OptimizeKit/releases |
| Journal des versions | [CHANGELOG.md](CHANGELOG.md) |

`OptimizeKit.exe` est **entièrement statique** (MinGW-w64, ~6,6 Mo) : aucun runtime, aucune DLL, aucune installation, aucun composant navigateur. Un dossier `web/` optionnel à côté de l'exe active le dashboard web complet au lancement avec `--app`.

## ⚡ Démarrage rapide

| Vous voulez | Faites ça |
|---|---|
| **L'appli, nativement** (par défaut) | Double-cliquez `dist\OptimizeKit.exe` — une vraie fenêtre s'ouvre, rien d'autre |
| Le dashboard web dans une fenêtre dédiée | `OptimizeKit.bat` option 5, ou `dist\OptimizeKit.exe --app` |
| Moteur PowerShell direct (sans exe) | `powershell -File PowerShell\OptimizeKit.ps1` ou `OptimizeKit.bat -Status` |
| Dashboard web headless | `OptimizeKit.exe --web 8765` (loopback uniquement) |

La fenêtre par défaut **est l'application** : Direct2D + DirectWrite dessinent tout, avec le panneau firmware, le moteur pilotes, le benchmark et les 77 tweaks. Aucun port à ouvrir, aucun navigateur lancé.

> 🛡️ **Sécurité** : chaque tweak possède une **restauration vers la valeur Windows d'origine** en un clic (UI native + web + CLI), le moteur PowerShell a `-RestoreAll`, et les sauvegardes `.reg` vivent dans `%LOCALAPPDATA%\OptimizeKit\`.

## 🎮 Ce qu'il y a dedans (fenêtre native)

- **Dashboard** — état live de la machine, indicateurs gaming, profils rapides, benchmark en un clic
- **Tweaks** — les **77 tweaks** avec application/restauration instantanée, filtres, badges admin, étoiles d'impact
- **Gaming** — profils un clic + 12 interrupteurs rapides + table de priorité/kill des processus
- **Firmware** — identité BIOS, Secure Boot, TPM, VT, mode veille, état HPET/WPBT/tick, dump kernel, redémarrage en attente (lecture seule ; l'appli n'écrit jamais dans le firmware)
- **Stockage** — disques avec détection de bus NVMe/SATA et barres d'usage, scan des plus gros fichiers, nettoyages TEMP/WinSxS
- **Démarrage** — clés Run HKCU/HKLM + dossiers Startup, activation/désactivation réversible en un clic
- **Paramètres** — confirmation des actions, sauvegarde auto, gestion DNS, restauration générale — persistés dans `config.json`
- **Privacy / Pilotes / Réseau / Logs / À propos** — interrupteurs de confidentialité, rapport pilotes + scan Windows Update, moniteur de latence avec cibles personnalisées, journal d'activité

Le **dashboard web** optionnel ajoute le set complet de modules : Smart Optimize (plans classés par objectif), bibliothèque de jeux (106 jaquettes + base de ~300 titres), packs, cinq centres de réglage, Réducteur de processus, Analyse sécurité, outils DiskScope, diagnostics, historique de benchmark, guide BIOS, thèmes (12 packs), palette de commandes.

## 🖥️ CLI

`OptimizeKit-cli.bat` donne les menus numérotés (user ou admin). Flags directs :

```
OptimizeKit.exe                    dashboard D2D natif (par défaut)
OptimizeKit.exe --app              dashboard web dans une fenêtre WebView2 (comportement 2.6)
OptimizeKit.exe --web [port]       sert le dashboard web, sans fenêtre
OptimizeKit.exe --profile gaming|privacy|full|clean
OptimizeKit.exe --apply <tweak-id>
OptimizeKit.exe --restore <tweak-id>
OptimizeKit.exe --list             liste des ids de tweaks
OptimizeKit.exe --clean            nettoyage des fichiers temporaires
OptimizeKit.exe --info             résumé système
OptimizeKit.exe --firmware         rapport BIOS / SecureBoot / TPM / options kernel
OptimizeKit.exe --drvupdate        rapport d'âge des pilotes + scan Windows Update
OptimizeKit.exe --ping <hôte>      test de latence
```

## 🎛️ Firmware, kernel & pilotes — « plus proche de la machine »

OptimizeKit est du Win32/C++ pur et se comporte comme un outil système :

- **Inventaire firmware** : identité SMBIOS (WMI), machine à états UEFI Secure Boot, TPM (namespace Win32_Tpm avec repli PnP pour les sessions non-élevées), état de virtualisation y compris l'hyperviseur actif, politique modern standby vs S3, options gérées par `bcdedit` (dynamic tick, platform tick, platform clock, TSC). Strictement **en lecture seule** — les changements firmware restent un travail humain, guidé par les cartes BIOS.
- **Tweaks visibles par le kernel** : blocage d'exécution WPBT (empêche le firmware du vendeur de lancer du code au boot), politique HPET, dynamic tick, mode MSI GPU/NIC, affinité d'interruption, `Win32PrioritySeparation`, tâche MMCSS gaming, seuil de split svchost — le tout par des chemins registre/services documentés, tout réversible.
- **Mises à jour de pilotes** : l'appli donne l'âge réel de chaque pilote installé (depuis les enregistrements `Control\Class` — la même source que le Gestionnaire de périphériques), liste les périphériques en erreur, puis déclenche **le scan de pilotes de Windows Update lui-même** (`UsoClient`, l'orchestrateur officiel) ou ouvre la page constructeur. Rien n'est téléchargé ni installé dans votre dos.

## 🛡️ Sécurité d'abord

- **Chaque tweak est réversible** — interrupteur OFF = valeur Windows d'origine exacte.
- **Sauvegardes registre** avant toute modification : `%LOCALAPPDATA%\OptimizeKit\backup_*.reg`.
- **Descriptions honnêtes** — aucune promesse de FPS magique ; la page RAM explique même pourquoi « vider la standby » n'est pas « de la RAM en plus » ; le panneau firmware affiche « unknown » plutôt que d'inventer une valeur.
- **Gaming Mode** photographie votre plan d'alimentation et restaure tout à la sortie.
- **Rien ne tourne au démarrage**, aucun service installé ; l'exe n'agit que sur demande.
- Sources triées sur le volet : [WinUtil de Chris Titus Tech](https://github.com/ChrisTitusTech/winutil) (MIT), documentation Microsoft et la communauté PC gaming — chaque tweak affiche son origine.

## 🏗️ Compiler depuis les sources

```
git clone https://github.com/cameleonnbss/OptimizeKit
cd OptimizeKit
build.bat          rem MinGW-w64 g++ 13+ (winlibs / MSYS2)
```

Résultat : `dist\OptimizeKit.exe` + `dist\web\`. Ou utilisez le CMakeLists fourni avec toute toolchain MinGW. Régénérez les icônes avec `python tools/make_icon.py` (Pillow).

## 📄 Licence

MIT — voir [LICENSE](LICENSE). Tweaks issus de WinUtil (MIT), de la documentation Microsoft et du savoir de la communauté.

<div align="center">
<b>Si OptimizeKit vous a fait gagner du temps, une ⭐ sur le repo aide beaucoup.</b>
</div>
