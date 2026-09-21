<div align="center">

<img src="assets/icon256.png" width="88" alt="OptimizeKit"/>

# ⚡ OptimizeKit

**Centre de contrôle Gaming Windows — un seul exe, dashboard liquid-glass, chaque tweak réversible.**

[![Release](https://img.shields.io/github/v/release/cameleonnbss/OptimizeKit?style=flat-square&color=ff3d57)](../../releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4?style=flat-square)](https://github.com/cameleonnbss/OptimizeKit)
[![Language](https://img.shields.io/badge/C%2B%2B-20-00599c?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![License](https://img.shields.io/badge/license-MIT-3dd68c?style=flat-square)](LICENSE)

**Monitoring live** · **Score gaming** · **Optimisation intelligente** · **49 tweaks réversibles** · **5 centres de réglage** · **Base de ~300 jeux + 106 jaquettes** · **Centre réseau** · **Benchmark style AnTuTu** · **Catalogue de 79 outils** · **Palette Ctrl+K** · **12 thèmes complets** · **EN/FR** · **Journal d'activité complet**

`C++20 / Win32` · `serveur HTTP embarqué` · `zéro dépendance` · `sans installation` · `aucun driver, aucune injection`

[⬇️ **Télécharger v2.6.0**](../../releases/tag/v2.6.0) · [Nouveautés](#-nouveautés-de-la-260) · [Démarrage](#-démarrage-rapide) · [Captures](#-captures-décran) · [Sécurité](#%EF%B8%8F-sécurité-dabord) · [English](README.md)

</div>

---

## 🆕 Nouveautés de la 2.6.0

| | |
|---|---|
| **Réducteur de processus** | Table live CPU/RAM de tout ce qui tourne — mode éco par processus (priorité basse + power throttling), kill, nettoyage du fond, restauration complète |
| **Analyse sécurité** | Uniquement des preuves réelles : antivirus + âge des signatures, pare-feu par profil, chaque port en écoute et son propriétaire, connexions sortantes avec IP distantes, persistance au démarrage, fichiers à motifs ransomware — chaque constat a son correctif en un clic |
| **Outils DiskScope** | Doublons, cibles de nettoyage et tailles de dossiers, construits avec les assets du projet [DiskScope-CLI](https://github.com/cameleonnbss/DiskScope-CLI) |
| **Vraies jaquettes** | La bibliothèque récupère les jaquettes des titres Steam et Epic les plus joués depuis les CDN des stores — la plupart des tuiles montrent la vraie pochette |
| **Interface peaufinée** | Le sélecteur de couleur d'accent en haut à gauche est pleinement fonctionnel ; transitions et survols animés partout |

### Toujours valable depuis la 2.5.0

| | |
|---|---|
| **Base de jeux intégrée** | ~300 titres PC connus avec leurs exe, alias et genres — un jeu installé hors de tout store est reconnu, nommé et reçoit son set suggéré |
| **Tout détecter** | Six nouveaux scans (Battle.net, Ubisoft, EA, itch.io, clés utilisateur, chemins Xbox) **et un balayage de chaque disque fixe** ; un Steam installé sur `D:` est trouvé via le registre |
| **Icônes partout** | Une seule passe extrait l'icône de **chaque** jeu détecté depuis son propre exe — plus seulement les douze premiers |
| **Plein de boutons** | Lancer, booster, gaming mode, extraire l'icône, ouvrir le dossier, copier le chemin, preset de bibliothèque, tout booster, toutes les icônes, aléatoire, raccourcis de store |
| **Bibliothèque ×2** | Jaquettes + base fusionnées, filtres genre/installés/jaquettes, cinq tris, badge `INSTALLED` avec la vraie icône |

### Toujours valable depuis la 2.4.0

| | |
|---|---|
| **Bibliothèque de jeux** | 106 jaquettes **+ la base intégrée de ~300 titres**, classées par genre — tu choisis un titre, tu vois exactement le set qui sera appliqué, puis applique ou restaure en un clic |
| **5 centres de réglage** | Latence d'entrée · Rendu & FPS · Charge de fond · Énergie & thermique · Débloat & démarrage — état du catalogue + cartes mesurées sur ta machine, marquées `· LIVE` |
| **Palette Ctrl+K** | Atteindre n'importe quel module, tweak, outil, pack ou jeu ; les tweaks s'appliquent depuis la palette avec leur état affiché |
| **12 thèmes** | Thèmes complets avec mémoire d'accent par thème, présentés en galerie — Magma, Khadafi, Carbon, Discord, Fusion, Acid, Ocean, Matrix, Violet, Gold, Steel, Rose |
| **⚡ BOOST** | Une pilule dans la barre du haut : le set compétitif, réversible en un clic |
| **Démarrage plus rapide** | Lecture du catalogue et du réseau en parallèle (splash ~2× plus rapide) |

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

| Benchmark — score style AnTuTu | Outils — 79 lanceurs |
|---|---|
| ![Benchmark](docs/screenshots/bench.png) | ![Outils](docs/screenshots/tools.png) |

| Packs de thèmes (Ocean) | Splash |
|---|---|
| ![Theme Ocean](docs/screenshots/theme-ocean.png) | ![Splash](docs/screenshots/dashboard.png) |

## 📦 Téléchargement

| Release | Lien |
|---|---|
| **v2.6.0 (actuelle)** | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v2.6.0 |
| Toutes les releases | https://github.com/cameleonnbss/OptimizeKit/releases |

`OptimizeKit.exe` est **entièrement statique** (MinGW-w64, ~6 Mo) : aucun runtime, aucune DLL, aucune installation. Il embarque un serveur HTTP et le dashboard liquid-glass (dossier `web/` livré à côté).

## ⚡ Démarrage rapide

| Vous voulez | Double-cliquez |
|---|---|
| **Le menu** (tout, exe optionnel) | `OptimizeKit.bat` |
| **Le dashboard** (UI web dans une fenêtre dédiée) | `OptimizeKit.bat` option 5, ou `dist\OptimizeKit.exe` |
| Moteur PowerShell direct (sans exe) | `powershell -File PowerShell\OptimizeKit.ps1` ou `OptimizeKit.bat -Status` |
| Dashboard web headless | `OptimizeKit.exe --web 8765` |

Le dashboard s'ouvre sur une **séquence de démarrage animée** (détection matériel → état Windows → catalogue de tweaks → mesure réseau), puis arrive sur le **Gaming Center** avec le score gaming de votre machine.

> 🛡️ **Sécurité** : chaque tweak possède une **restauration vers la valeur Windows d'origine** en un clic (UI web + CLI), le moteur PowerShell a `-Restore`, et les sauvegardes `.reg` vivent dans `%LOCALAPPDATA%\OptimizeKit\`.

## 🎮 Gaming Center

La nouvelle page d'accueil note votre machine de 0 à 100 selon l'état réel des réglages importants pour le jeu, puis regroupe chaque interrupteur par catégorie :

- **FPS & rendu** — HAGS, Game Mode, fullscreen optimizations, MPO, préférence GPU
- **Latence & input** — timer 0,5 ms, stack réseau gaming (TcpAckFrequency/NoDelay/MMCSS), accélération souris, délai des menus, Win32PrioritySeparation
- **Charge en arrière-plan** — Game DVR, applications en arrière-plan, SysMain, indexation, services Xbox Live
- **Alimentation & thermique** — plan Ultimate Performance, HPET

Chaque interrupteur est un vrai toggle : clic → snapshot registre → application → confirmation animée. Re-cliquez pour restaurer la valeur Windows. Les badges de catégorie affichent `OPTIMIZED / 3/5 / STOCK` d'un coup d'œil.

## 🔧 Le dashboard (liquid-glass style WormGPT)

Noir profond + couleur d'accent au choix (6 thèmes dans la barre du haut, persistés), glassmorphisme avec reflet spéculaire animé, particules suivant la souris, Inter & Space Mono. Servi par un serveur HTTP C++ embarqué sur `127.0.0.1:8765`, ouvert en fenêtre d'application sans chrome.

| Vue | Contenu |
|---|---|
| **Dashboard** | CPU / RAM / GPU / disque live avec sparklines 60 s, débit réseau, processus top (2,5 s) |
| **Gaming Center** | Anneau de score gaming, cartes par catégorie, interrupteurs groupés, Gaming Mode |
| **Tweaks** | Les 49 tweaks en cartes interrupteur : application / restauration instantanée, recherche, filtres, badges ADMIN/USER, barres d'impact, compteur live dans la sidebar |
| **Jeux** | Neuf stores + balayage de chaque disque fixe, base de jeux intégrée pour nommer et classer ce qui est trouvé, vraies icônes extraites des exécutables, lancer/afficher en un clic, boost par jeu (IFEO), tout booster, Gaming Mode et raccourcis de store |
| **Scan PC** | Scan complet : fichiers temporaires, état des tweaks, réseau, plan d'alimentation, démarrage, pilotes, jeux — chaque résultat a son bouton Apply |
| **Optimize** | Profils un clic (SAFE / GAMING / PRIVACY / RESTORE ALL) avec snapshot → application → vérification |
| **Réseau** | État des adaptateurs (autotuning, RSC, RSS, DNS, MTU), profils, **slider MTU** avec préréglages (Ethernet/PPPoE/VPN/Jumbo), moniteur de latence, ping rapide |
| **RAM** | Graphe d'utilisation, stats committed/cached, top consommateurs, purge honnête de la standby list (avec l'explication de ce qu'elle fait vraiment) |
| **Stockage** | Disques avec détection du bus NVMe/SATA, barres d'utilisation, fichiers les plus gros |
| **Démarrage** | Clés Run HKCU/HKLM + dossiers Startup, désactivation en un clic (réversible via stash) |
| **Benchmark** | Score style AnTuTu **sur ~4000** (1000 = machine de référence grand public) : CPU multi/single-cœur, bande passante mémoire, lecture NVMe/HDD, latence — sous-scores pondérés avec barres, deltas entre runs, historique de 20 runs. Le wizard de premier lancement propose un benchmark pour comparer avant/après optimisation |
| **Thèmes** | 12 thèmes complets présentés en galerie, mémoire d'accent par thème, accent personnalisé, particules et effet glitch |
| **Catalogue d'outils** | **79 outils Windows** dans 9 catégories recherchables (Système, Performance, Gaming, Réseau, Sécurité, Stockage, Affichage, Paramètres, Power user) — toutes les consoles, applets du panneau de configuration et pages ms-settings, en un clic |
| **Logs** | Journal d'activité coloré avec filtre live |
| **Paramètres** | Couleur d'accent, particules, kill list du Gaming Mode, gestion DNS — persistés dans `config.json` |

## 🕹️ Détection des jeux

Scanner multi-provider — pas de verrouillage sur un seul store :

| Provider | Source |
|---|---|
| **Steam** | registre `SteamPath`/`InstallPath` → `libraryfolders.vdf` → `appmanifest_*.acf` (toutes bibliothèques, tous disques) |
| **Epic** | `%PROGRAMDATA%\Epic\EpicGamesLauncher\Data\Manifests\*.item` |
| **Riot** | `HKLM\SOFTWARE\Riot Games, Inc.\*` (VALORANT, LoL…) |
| **GOG** | `HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\*` |
| **Battle.net** | `HKLM\SOFTWARE[(WOW6432Node)\]Blizzard Entertainment\*` → `InstallLocation` (WoW, Overwatch, Diablo…) |
| **Ubisoft** | `…\Ubisoft\Launcher\Installs\*` → `InstallDir` |
| **EA** | `…\Electronic Arts\EA Games\*` → `Install Dir` |
| **itch.io** | `%APPDATA%\itch\apps\*` |
| **Registre** | Clés Uninstall (machine **et** utilisateur) sous Epic/Riot/GOG/Battle.net/Blizzard/Ubisoft/EA/Xbox/Rockstar/Amazon/itch/Wargaming |
| **Tous les disques** | `\Games`, `\Game`, `\Jeux`, `\GOG Games`, `\Epic Games`, `\Riot Games`, `\XboxGames`, `\Battle.net`, `\Program Files (x86)`, `\SteamLibrary\steamapps\common`… — seuls les dossiers reconnus par la base intégrée sont ouverts |

**Bibliothèque** : la grille fusionne les jaquettes et la base. Les titres sans jaquette reçoivent une vignette générée au lieu de casser la grille, les jeux installés sont badgés `INSTALLED` avec leur vraie icône extraite et peuvent être lancés depuis la bibliothèque (filtres genre / installés / jaquettes, cinq tris, bouton Surprise me).

Chaque exe candidat est comparé à la **base de jeux intégrée** (`src/core/gamedb.h`, ~300 titres avec exe + alias + genre) : un jeu reçoit son vrai nom et sa famille même sans manifest de launcher. Puis : profils par jeu (priorité haute persistante via IFEO), gaming mode par jeu, `POST /api/games/launch`, `POST /api/games/boost-all`, et extraction des vraies icônes (`SHDefExtractIconW` → cache PNG, en lot via `POST /api/games/icons`).

## 🖥️ CLI

`OptimizeKit-cli.bat` donne les menus numérotés (user ou admin). Flags directs :

```
OptimizeKit.exe --native            dashboard Direct2D natif
OptimizeKit.exe --web [port]        dashboard web sans ouvrir le navigateur
OptimizeKit.exe --profile gaming|privacy|full|clean
OptimizeKit.exe --apply <tweak-id>
OptimizeKit.exe --restore <tweak-id>
OptimizeKit.exe --list              liste des ids de tweaks
OptimizeKit.exe --clean             nettoyage des fichiers temporaires
OptimizeKit.exe --info              résumé système
```

## 🛡️ Sécurité d'abord

- **Chaque tweak est réversible** — interrupteur OFF = valeur Windows d'origine exacte.
- **Sauvegardes registre** avant toute modification : `%LOCALAPPDATA%\OptimizeKit\backup_*.reg`.
- **Descriptions honnêtes** — aucune promesse de FPS magique ; la page RAM explique même pourquoi « vider la standby » n'est pas « de la RAM en plus ».
- **Gaming Mode** photographie votre plan d'alimentation et restaure tout à la sortie.
- **Rien ne tourne au démarrage**, aucun service installé ; l'exe n'agit que sur demande.
- Sources triées sur le volet : [WinUtil de Chris Titus Tech](https://github.com/ChrisTitusTech/winutil) (MIT), documentation Microsoft et la communauté PC gaming — chaque tweak affiche son origine dans le CLI.

## 🏗️ Compiler depuis les sources

```
git clone https://github.com/cameleonnbss/OptimizeKit
cd OptimizeKit
build.bat          rem MinGW-w64 g++ 13+ (winlibs / MSYS2)
```

Résultat : `dist\OptimizeKit.exe` + `dist\web\`. Ou utilisez le CMakeLists fourni avec tout toolchain MinGW.

## 📄 Licence

MIT — voir [LICENSE](LICENSE). Tweaks issus de WinUtil (MIT), de la documentation Microsoft et du savoir de la communauté.

<div align="center">
<b>Si OptimizeKit vous a fait gagner du temps, une ⭐ sur le repo aide beaucoup.</b>
</div>
