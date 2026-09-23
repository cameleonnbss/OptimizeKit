<div align="center">

<img src="assets/icon256.png" width="88" alt="OptimizeKit"/>

# ⚡ OptimizeKit

**Centre de contrôle Gaming Windows — un seul exe, le dashboard complet dans une vraie fenêtre bureau (sans navigateur), chaque tweak réversible.**

[![Release](https://img.shields.io/github/v/release/cameleonnbss/OptimizeKit?style=flat-square&color=ff3d57)](../../releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4?style=flat-square)](https://github.com/cameleonnbss/OptimizeKit)
[![Language](https://img.shields.io/badge/C%2B%2B-20-00599c?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![License](https://img.shields.io/badge/license-MIT-3dd68c?style=flat-square)](LICENSE)
[![Size](https://img.shields.io/badge/exe-~6,6%20Mo%20statique-ff9800?style=flat-square)](#-t%C3%A9l%C3%A9chargement)

**Dashboard en fenêtre d'app (interface web, sans navigateur)** · **Score gaming** · **77 tweaks réversibles** · **CLI autonome (sans exe)** · **Panneau firmware (SecureBoot/TPM/VT)** · **Moteur de mise à jour des pilotes** · **5 centres de réglage** · **Base de ~300 jeux + 106 jaquettes** · **Centre réseau** · **Benchmark style AnTuTu** · **Catalogue de 79 outils** · **Palette Ctrl+K** · **12 thèmes** · **EN/FR**

`C++20 / Win32 / Direct2D` · `zéro dépendance` · `sans installation` · `aucun driver, aucune injection, aucun Edge`

[⬇️ **Télécharger v2.8.0**](../../releases/tag/v2.8.0) · [Nouveautés](#-nouveaut%C3%A9s-de-la-280) · [Démarrage](#-d%C3%A9marrage-rapide) · [Captures](#-captures-d%C3%A9cran) · [Sécurité](#%EF%B8%8F-s%C3%A9curit%C3%A9-dabord) · [English](README.md)

</div>

---

## 🆕 Nouveautés de la 2.8.0

| | |
|---|---|
| **Le dashboard web EST la fenêtre de l'app** | Double-clic → le dashboard embarqué s'ouvre dans une vraie fenêtre bureau — **identique au pixel près à l'interface navigateur** (thèmes, palette Ctrl+K, bibliothèque de jeux), sans fenêtre Edge, sans onglets, sans barre d'adresse. Si le runtime WebView2 manque, l'app retombe silencieusement sur la fenêtre native Direct2D au lieu d'ouvrir un navigateur. `--native` force la D2D. |
| **Fenêtre améliorée** | Le titre suit l'onglet du dashboard (Firmware, Tweaks…), F5 / Ctrl+R rechargent, clic droit pour copier activé. |
| **Launchers CLI autonomes** | `OptimizeKit.bat` est un menu de modules complet et `OptimizeKit-cli.bat` tourne partout — **sans exe** : tout passe par le moteur PowerShell (71 tweaks, profils, réseau, nettoyage, firmware, drivers, restauration générale), avec menus admin ET non-admin. |
| **24 nouveaux tweaks PowerShell — 71 au total** | Le moteur rattrape le catalogue C++ : Widgets, Localisation, Services en manuel + tuning svchost, Delivery Optimization, Consumer features, recherche du Store, fin de tâche sur la taskbar, **blocage WPBT**, blocage Razer, Notifications, IPv4/IPv6/Teredo, nettoyage disque + WinSxS, hibernation off, BSoD verbeux, chemins longs, Game Mode (Win11), débloat Edge & Brave, horloge UTC, point de restauration — chacun avec application, restauration et état live. |
| **Rapport firmware dans le moteur** | `-Firmware` / menu 7 : BIOS, Secure Boot, TPM, VT-x, hyperviseur, redémarrage en attente — strictement en lecture seule. |
| **Captures fraîches** | Chaque vue du dashboard re-capturée depuis l'appli en direct avec de vraies données machine. |

### Toujours valable depuis la 2.7.0

| | |
|---|---|
| **Panneau firmware & plateforme (natif + `--firmware`)** | BIOS fournisseur/version, carte mère, mode boot, **Secure Boot**, **TPM**, VT-x/SVM + hyperviseur, modern standby vs S3, HPET, WPBT, dynamic tick / TSC, niveau de dump kernel, redémarrage en attente |
| **Moteur de mise à jour des pilotes (natif + `--drvupdate`)** | Âge réel des pilotes, périphériques en erreur, **scan Windows Update**, rescan PnP, pages constructeurs |
| **28 tweaks C++ — 77 au total** | Alignés sur le catalogue actuel de WinUtil, tout réversible |

### Toujours valable depuis la 2.6.0

| | |
|---|---|
| **Réducteur de processus** | Table live CPU/RAM — mode éco (priorité basse + EcoQoS), kill, nettoyage, restauration complète |
| **Analyse sécurité** | Antivirus, pare-feu par profil, chaque port en écoute et son propriétaire, persistance au démarrage, motifs ransomware — correctifs en un clic |
| **Outils DiskScope** | Doublons, cibles de nettoyage, tailles de dossiers |
| **Vraies jaquettes** | Pochettes Steam/Epic des titres les plus joués |

Détail complet dans [CHANGELOG.md](CHANGELOG.md).

## 📸 Captures d'écran

Captures prises depuis l'appli en direct (v2.8, vraies données machine, zéro mockup).

| Dashboard | Tweaks |
|---|---|
| ![Dashboard](docs/screenshots/dashboard.png) | ![Tweaks](docs/screenshots/tweaks.png) |

| Gaming Center | Jeux — vraies icônes |
|---|---|
| ![Gaming Center](docs/screenshots/gaming.png) | ![Games](docs/screenshots/games.png) |

| Centre réseau | Stockage |
|---|---|
| ![Network](docs/screenshots/network.png) | ![Storage](docs/screenshots/storage.png) |

| Outils — 79 lanceurs | Thèmes — 12 packs |
|---|---|
| ![Tools](docs/screenshots/tools.png) | ![Themes](docs/screenshots/themes.png) |

## 📦 Téléchargement

| Release | Lien |
|---|---|
| **v2.8.0 (actuelle)** | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.8.0 |
| v2.7.0 | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.7.0 |
| v2.6.0 | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.6.0 |
| Toutes les releases | https://github.com/cameleonnbss/OptimizeKit/releases |
| Journal des versions | [CHANGELOG.md](CHANGELOG.md) |

`OptimizeKit.exe` est un **seul binaire statique** (MinGW-w64, ~6,6 Mo) : aucune installation. Le runtime WebView2 (préinstallé sur Windows 10/11) fournit le rendu de la fenêtre ; s'il manque, l'interface native Direct2D prend le relais — aucun navigateur n'est jamais lancé.

## ⚡ Démarrage rapide

| Vous voulez | Faites ça |
|---|---|
| **L'appli** (par défaut) | Double-cliquez `dist\OptimizeKit.exe` — le dashboard s'ouvre dans sa propre fenêtre bureau, sans navigateur |
| **La CLI complète, sans exe** | Lancez `OptimizeKit.bat` (menu de modules) ou `OptimizeKit-cli.bat` (menus numérotés, admin + user) |
| Moteur PowerShell direct | `powershell -File PowerShell\OptimizeKit.ps1` ou `OptimizeKit.bat -Status` |
| Fenêtre native Direct2D | `OptimizeKit.exe --native` |
| Dashboard web headless | `OptimizeKit.exe --web 8765` (loopback uniquement) |

La fenêtre par défaut **est l'application** : exactement la même interface liquid-glass que le dashboard navigateur — thèmes, palette Ctrl+K, bibliothèque de jeux, panneau firmware, moteur pilotes, benchmark, 77 tweaks — rendue dans une fenêtre dédiée avec barre de titre sombre et titre dynamique.

> 🛡️ **Sécurité** : chaque tweak possède une **restauration vers la valeur Windows d'origine** en un clic (web + natif + CLI + PowerShell `-RestoreAll`), et les sauvegardes `.reg` vivent dans `%LOCALAPPDATA%\OptimizeKit\`.

## 🎮 Ce qu'il y a dedans (fenêtre de l'app = dashboard web)

- **Dashboard** — état live de la machine, indicateurs gaming, profils rapides, benchmark en un clic
- **Tweaks** — les **77 tweaks** avec application/restauration instantanée, filtres, badges admin, étoiles d'impact
- **Gaming Center** — profils un clic + 12 interrupteurs rapides + table de priorité/kill des processus
- **Firmware (guide BIOS)** — identité BIOS, Secure Boot, TPM, VT, mode veille, état HPET/WPBT/tick, redémarrage en attente (lecture seule ; l'appli n'écrit jamais dans le firmware)
- **Stockage** — disques avec détection de bus NVMe/SATA, cibles de nettoyage sûres, doublons, plus gros fichiers
- **Réseau** — TCP autotuning / RSS / MTU, profils, moniteur de latence avec cibles personnalisées
- **Analyse sécurité** — antivirus, pare-feu, ports en écoute + propriétaires, persistance au démarrage, correctifs un clic
- **Réducteur de processus** — table live CPU/RAM, mode éco (EcoQoS), kill, nettoyage, restauration complète
- **Jeux / Bibliothèque** — détection multi-store avec vraies jaquettes, icônes en lot, boost
- **Outils** — 79 lanceurs Windows ; **Thèmes** — 12 packs complets + accent perso ; palette **Ctrl+K**

Sans l'exe, `OptimizeKit.bat` / `OptimizeKit-cli.bat` exposent le même ensemble via le moteur PowerShell : 71 tweaks avec état, profils gaming/privacy/debloat/full, centre réseau, nettoyage, rapport firmware, pilotes, restauration générale — en menus admin et non-admin.

## 🖥️ CLI

`OptimizeKit-cli.bat` donne les menus numérotés (user ou admin). Flags directs :

```
OptimizeKit.exe                    dashboard web dans une fenêtre bureau (par défaut)
OptimizeKit.exe --native           fenêtre dashboard native Direct2D
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

OptimizeKit.bat                    menu complet de modules, moteur PowerShell (sans exe)
OptimizeKit.bat -Status            état des tweaks, lecture seule
OptimizeKit.bat -Tweaks            application par numéro / r<N> restaure
OptimizeKit.bat -Network           latence + benchmark DNS
OptimizeKit.bat -Cleanup           nettoyage des fichiers temporaires
OptimizeKit.bat -Firmware          BIOS / SecureBoot / TPM (lecture seule)
OptimizeKit.bat -Drivers           infos GPU/pilotes + pages constructeurs
OptimizeKit.bat -Profile gaming|privacy|debloat|full
OptimizeKit.bat -Silent            profil gaming sans questions
OptimizeKit.bat -RestoreAll        retour aux valeurs Windows
OptimizeKit-cli.bat /exe           mêmes menus via la CLI C++ (si compilée)
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
