<div align="center">

<img src="assets/icon256.png" width="88" alt="OptimizeKit"/>

# ⚡ OptimizeKit

**Windows Gaming Control Center — un exe C++20 compilé, une interface HTML/CSS/JS, chaque tweak réversible.**

[![Release](https://img.shields.io/github/v/release/cameleonnbss/OptimizeKit?style=flat-square&color=ff3d57)](../../releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4?style=flat-square)](https://github.com/cameleonnbss/OptimizeKit)
[![Language](https://img.shields.io/badge/C%2B%2B-20-00599c?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![UI](https://img.shields.io/badge/UI-HTML%20%2F%20CSS%20%2F%20JS-e34f26?style=flat-square&logo=html5&logoColor=white)](web/index.html)
[![License](https://img.shields.io/badge/license-MIT-3dd68c?style=flat-square)](LICENSE)
[![Size](https://img.shields.io/badge/exe-~6.5%20Mo%20statique-ff9800?style=flat-square)](#-t%C3%A9l%C3%A9chargement)

**Dashboard en fenêtre bureau** · **Score gaming** · **77 tweaks réversibles** · **Panneau firmware (SecureBoot/TPM/VT)** · **Moteur de mise à jour des pilotes** · **Process Reducer (EcoQoS)** · **Scan sécurité** · **DiskScope** · **5 centres de tuning** · **Base de 300 jeux + 106 jaquettes** · **Centre réseau** · **Benchmark type AnTuTu** · **Catalogue de 79 outils** · **Palette Ctrl+K** · **12 thèmes** · **EN/FR**

`C++20 / Win32 / WebView2 / cpp-httplib` · `zéro dépendance d'exécution` · `sans installation` · `sans driver, sans injection`

[⬇️ **Télécharger v2.9.0**](../../releases/tag/v2.9.0) · [Quoi de neuf](#-quoi-de-neuf-dans-la-290) · [Démarrage rapide](#-d%C3%A9marrage-rapide) · [Captures](#-captures) · [Sécurité](#%EF%B8%8F-s%C3%A9curit%C3%A9-dabord) · [**English**](README.md) · **Français**

</div>

---

## 🆕 Quoi de neuf dans la 2.9.0

| | |
|---|---|
| **L'interface v2.6 est de retour** | Le dashboard web liquid-glass redevient **l'** interface : double-clic → la même UI HTML/CSS/JS s'ouvre dans une vraie fenêtre bureau (cadre WebView2 embarqué). Sans WebView2, l'app ouvre `msedge --app` — la même fenêtre sans chrome façon WormGPT — puis le navigateur par défaut. Aucun basculement silencieux vers une autre UI. |
| **Binaire compilé + interface web, style WormGPT-desktop** | Un seul exe C++20 statique embarque son propre serveur HTTP (loopback uniquement) et tout le dashboard : interface HTML/CSS/JS, langage compilé en dessous. Exactement l'architecture demandée. |
| **Highlights sur le dashboard** | Une rangée vivante des modules les plus récents — identité firmware, Secure Boot, âge des pilotes, jeux détectés, Process Reducer, Scan sécurité, DiskScope, Benchmark — alimentée par les vraies données de la machine, un clic ouvre chaque vue. |
| **Catégories de packs en avant** | La page Packs s'ouvre sur ★ **In the spotlight** avec les quatre presets phares (Esport, Low latency, Fast clean boot, Privacy lock-down) plus des chips par catégorie. |
| **Deux launchers CLI, comme demandé** | `OptimizeKit.bat` (sans admin, n'élève jamais) et `OptimizeKit-Admin.bat` (propose l'UAC une fois, menu admin complet). Les deux font tourner les menus numérotés **sans l'exe** via le moteur PowerShell — `/exe` opte pour la CLI compilée. |
| **Nouvelles actions rapides** | ⚑ Mode Esport et ⛭ Firmware rejoignent Scan / Quick Optimize / Clean junk sur la carte héro. |
| **Paramètres rafraîchis** | Les cartes Firmware & pilotes et Mises à jour prennent la bordure d'accent ; le réglage d'alerte firmware se recharge correctement. |
| **Captures refaites** | Chaque vue re-capturée depuis l'app en direct (v2.9, données réelles de la machine). |

### Toujours depuis 2.8.0 / 2.7.0

| | |
|---|---|
| **Panneau Firmware & plateforme** | BIOS vendeur/version, carte mère, mode de boot, **Secure Boot**, **TPM**, VT-x/SVM + hyperviseur, modern standby vs S3, HPET, WPBT, dynamic tick / TSC, niveau de dump kernel, redémarrage en attente — lecture seule |
| **Moteur de mise à jour des pilotes** | Âges réels depuis le driver store, périphériques en erreur, **déclenchement du scan Windows Update**, rescan PnP, pages vendeurs |
| **77 tweaks réversibles** (28 alignés sur WinUtil en 2.7, 24 de plus dans le moteur en 2.8) | Widgets, Localisation, Services-en-Manuel + tuning svchost, Delivery Optimization, Fonctions grand public, **bloc WPBT**, bloc Razer, Notifications, IPv4/IPv6/Teredo, nettoyage disque + trim WinSxS, hibernation off, BSoD verbeux, chemins longs, Game Mode (Win11), debloat Edge & Brave, horloge UTC, point de restauration… |

### Toujours depuis 2.6.0 / 2.5.0 / 2.4.0

Process Reducer (mode éco / EcoQoS), Scan sécurité (ports, persistance, patterns ransomware), DiskScope (doublons, gros fichiers), jaquettes Steam/Epic, base de jeux (~300 titres, tous les stores + tous les disques), icônes par lot, Game Library avec sets par genre, 5 centres de tuning, palette Ctrl+K, 12 thèmes complets, pastille ⚡ BOOST, wizard de premier lancement avec choix de thème.

Tout le détail dans [CHANGELOG.md](CHANGELOG.md).

## 📸 Captures

Captures prises depuis l'app en direct (v2.9, données réelles, aucun mockup).

| Dashboard — avec Highlights | Tweaks |
|---|---|
| ![Dashboard](docs/screenshots/dashboard.png) | ![Tweaks](docs/screenshots/tweaks.png) |

| Gaming Center | Jeux — vraies icônes |
|---|---|
| ![Gaming Center](docs/screenshots/gaming.png) | ![Games](docs/screenshots/games.png) |

| Centre réseau | Stockage |
|---|---|
| ![Network](docs/screenshots/network.png) | ![Storage](docs/screenshots/storage.png) |

| Firmware — état plateforme en direct | Thèmes — 12 packs |
|---|---|
| ![Firmware](docs/screenshots/bios.png) | ![Themes](docs/screenshots/themes.png) |

| Outils — 79 lanceurs | |
|---|---|
| ![Tools](docs/screenshots/tools.png) | |

## 📦 Téléchargement

| Version | Lien |
|---|---|
| **v2.9.0 (actuelle)** | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.9.0 |
| v2.8.0 | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.8.0 |
| v2.7.0 | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.7.0 |
| v2.6.0 | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.6.0 |
| Toutes les versions | https://github.com/cameleonnbss/OptimizeKit/releases |
| Changelog | [CHANGELOG.md](CHANGELOG.md) |

`OptimizeKit.exe` est totalement **statique** (MinGW-w64, ~6,5 Mo) : pas de runtime, pas de DLL, pas d'installation. Toute l'interface voyage dans l'exe ; un dossier `web/` optionnel à côté sert simplement le même dashboard depuis une édition fraîche.

## ⚡ Démarrage rapide

| Vous voulez | Faites ça |
|---|---|
| **L'app** (défaut) | Double-clic sur `dist\OptimizeKit.exe` — le dashboard s'ouvre dans sa propre fenêtre bureau |
| **L'app, fenêtre navigateur** | `OptimizeKit.exe --web 8765` puis http://127.0.0.1:8765 (loopback uniquement) |
| **CLI sans admin** | Lancez `OptimizeKit.bat` — menus numérotés, tweaks HKCU, rapports |
| **CLI en admin** | Lancez `OptimizeKit-Admin.bat` — UAC une fois, puis tous les tweaks, nettoyage, restauration générale |
| Flags moteur directs | `OptimizeKit.bat -Status`, `-Apply id1,id2`, `-Profile gaming`, `-RestoreAll`, `-Silent` |
| Fenêtre Direct2D native | `OptimizeKit.exe --native` |

La fenêtre **est l'app** : exactement la même interface liquid-glass que le navigateur — thèmes, palette Ctrl+K, bibliothèque de jeux, panneau firmware, moteur pilotes, benchmark, 77 tweaks — servie par le serveur embarqué et rendue dans un cadre bureau dédié avec barre de titre sombre et titre dynamique.

> 🛡️ **Sécurité** : chaque tweak a une **restauration en un clic vers la valeur Windows par défaut** (UI web + UI native + CLI + PowerShell `-RestoreAll`), et les sauvegardes `.reg` vivent dans `%LOCALAPPDATA%\OptimizeKit\`.

## 🎮 Ce qu'il y a dedans

- **Dashboard** — état machine en direct, flags gaming, profils rapides, **Highlights** (nouveau), benchmark en un clic
- **Tweaks** — les **77 tweaks** avec apply/restore instantané, filtres, badges admin, étoiles d'impact
- **Gaming Center** — profils en un clic + MODE ESPORT + 12 interrupteurs rapides + table priorité/processus
- **Packs** — bundles de presets avec ★ catégories en avant et chips par catégorie
- **Firmware (guide BIOS)** — état plateforme en direct + cartes BIOS guidées (lecture seule ; l'app n'écrit jamais le firmware)
- **Stockage** — DiskScope : disques, cibles de nettoyage sûres, doublons, plus gros fichiers, tailles de dossiers
- **Réseau** — autotuning TCP / RSS / MTU, profils, moniteur de latence, reset DNS vers DHCP
- **Scan sécurité** — antivirus, pare-feu, ports en écoute + propriétaires, persistance au démarrage, correctifs en un clic
- **Process Reducer** — table CPU/RAM en direct, mode éco (EcoQoS), fin, balayage, restauration complète
- **Jeux / Bibliothèque** — détection tous stores avec vraies jaquettes, icônes par lot, boost par genre
- **Outils** — 79 lanceurs Windows ; **Thèmes** — 12 packs complets + accent perso ; palette **Ctrl+K** ; bascule EN/FR

Sans l'exe, les deux launchers `.bat` exposent le même ensemble via le moteur PowerShell : 71 tweaks avec état, profils gaming/privacy/debloat/full, centre réseau, nettoyage, rapport firmware, pilotes, restauration générale — dans les menus admin et non-admin.

## 🖥️ CLI

Deux launchers, un moteur — sans l'exe :

```
OptimizeKit.bat                    menu CLI, SANS admin (n'élève jamais)
OptimizeKit-Admin.bat              menu CLI, propose l'UAC une fois (menu admin complet)
OptimizeKit.bat /exe               mêmes menus via la CLI C++ compilée (si présente)

OptimizeKit.bat -Status            états des tweaks, lecture seule
OptimizeKit.bat -Apply id1,id2     appliquer des tweaks précis
OptimizeKit.bat -Profile gaming|privacy|debloat|full
OptimizeKit.bat -Silent            profil gaming, sans questions
OptimizeKit.bat -RestoreAll        retour aux défauts Windows
OptimizeKit.bat -Firmware          BIOS / SecureBoot / TPM (lecture seule)
OptimizeKit.bat -Drivers           infos GPU/pilotes + pages vendeurs
OptimizeKit.bat -Network           latence + benchmark DNS
OptimizeKit.bat -Cleanup           nettoyage des déchets
```

L'exe lui-même :

```
OptimizeKit.exe                    fenêtre bureau : dashboard embarqué (défaut)
OptimizeKit.exe --native           fenêtre Direct2D native
OptimizeKit.exe --web [port]       sert le dashboard web, sans fenêtre (loopback)
OptimizeKit.exe --cli              menu CLI numéroté (user ou admin)
OptimizeKit.exe --profile gaming|privacy|full|clean
OptimizeKit.exe --apply <tweak-id>       OptimizeKit.exe --restore <tweak-id>
OptimizeKit.exe --list             lister les ids de tweaks
OptimizeKit.exe --clean            nettoyage des déchets
OptimizeKit.exe --info             résumé système
OptimizeKit.exe --firmware         rapport BIOS / SecureBoot / TPM / options kernel
OptimizeKit.exe --drvupdate        âges pilotes + scan Windows Update
OptimizeKit.exe --ping <hôte>      test de latence
```

## 🏗️ Sous le capot

```
┌────────────────────────────────────────────────────────┐
│  OptimizeKit.exe (C++20 statique, ~6,5 Mo)             │
│                                                        │
│  ┌──────────────┐  ┌────────────────────────────────┐  │
│  │ Fenêtre Win32│  │  serveur HTTP embarqué         │  │
│  │ (cadre       │──│  (cpp-httplib, 127.0.0.1 seul) │  │
│  │  WebView2)   │  │  sert l'UI embarquée + le JSON │  │
│  └──────────────┘  └───────────────┬────────────────┘  │
│                                    │                   │
│  ┌────────────────────────────────┬┴─────────────────┐ │
│  │  Dashboard HTML / CSS / JS     │  noyau C++        │ │
│  │  (embarqué dans l'exe)         │  tweaks · monitor │ │
│  │  12 thèmes · Ctrl+K · EN/FR    │  firmware · jeux  │ │
│  └────────────────────────────────┴───────────────────┘ │
└────────────────────────────────────────────────────────┘
```

- **Langage compilé** : chaque action passe par des modules C++ (tweaks, monitor, firmware, jeux, reducer, security, diskscope, drivers) — pas de Python, pas de Node, pas de runtime.
- **Interface web** : le dashboard est du HTML/CSS/JS brut, embarqué dans l'exe et éditable dans `web/` pour le développement.
- **Inventaire firmware** : identité SMBIOS (WMI), machine à états Secure Boot UEFI, TPM (Win32_Tpm avec repli PnP hors élévation), état de virtualisation et hyperviseur actif, modern standby vs S3, options de boot gérées par `bcdedit`. Strictement en **lecture seule**.
- **Tweaks visibles par le kernel** : bloc d'exécution WPBT, politique HPET, dynamic tick, mode MSI GPU/NIC, affinité d'interruptions, `Win32PrioritySeparation`, tâche MMCSS gaming, seuil de split svchost — chemins documentés uniquement, tout réversible.
- **Mises à jour pilotes** : âges réels depuis les enregistrements `Control\Class`, périphériques en erreur, puis **scan pilotes de Windows Update lui-même** (`UsoClient`, l'orchestrateur supporté) ou page vendeur. Rien ne se télécharge dans votre dos.

## 🛡️ Sécurité d'abord

- **Chaque tweak est réversible** — l'interrupteur OFF restaure la valeur exacte par défaut de Windows.
- **Sauvegardes registre** avant chaque changement : `%LOCALAPPDATA%\OptimizeKit\backup_*.reg`.
- **Descriptions honnêtes** — pas de promesses FPS magiques ; la page RAM trim explique même pourquoi « vider le standby » n'est pas « de la RAM en plus » ; le panneau firmware affiche « unknown » plutôt que d'inventer des valeurs.
- **Le Gaming Mode** photographie votre plan d'alimentation et restaure tout à la sortie.
- **Rien ne tourne au boot**, aucun service installé ; l'exe n'agit que sur demande. Le serveur n'écoute que sur 127.0.0.1 — jamais sur le réseau.
- Sources curées depuis [WinUtil de Chris Titus Tech](https://github.com/ChrisTitusTech/winutil) (MIT), la documentation Microsoft et la communauté PC gaming — chaque tweak affiche son origine.

## 🏗️ Compiler depuis les sources

```
git clone https://github.com/cameleonnbss/OptimizeKit
cd OptimizeKit
build.bat          rem MinGW-w64 g++ 13+ (winlibs / MSYS2)
```

Résultat : `dist\OptimizeKit.exe` (+ `dist\web\` optionnel). Ou utilisez le CMakeLists fourni avec n'importe quel toolchain MinGW. Régénérez les icônes avec `python tools/make_icon.py` (Pillow) ; `tools/embed_web.py` ré-embarque les fichiers du dashboard modifiés.

## 📄 Licence

MIT — voir [LICENSE](LICENSE). Tweaks curés depuis WinUtil (MIT), la doc Microsoft et la communauté.

<div align="center">
<b>Si OptimizeKit vous a fait gagner du temps, une ⭐ sur le repo aide beaucoup.</b>
</div>
