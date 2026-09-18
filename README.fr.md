<div align="center">

# ⚡ OptimizeKit — Kit d'optimisation Windows

**Une app .exe avec dashboard « liquid glass » style WormGPT + monitoring live + scripts .bat/.ps1.**

Monitoring live (CPU / RAM / GPU / disque / réseau) · FPS gaming · ping/latence · confidentialité · débbiottage · pilotes · logs — tout est réversible.

[⬇️ Télécharger v1.1.0 (release)](../../releases) · [README anglais](README.md)

</div>

---

## 📦 Téléchargement

| Release | Lien |
|---|---|
| **v1.1.0 (actuelle)** | https://github.com/cameleonnbss/OptimizeKit/releases/tag/v1.1.0 |
| Toutes les releases | https://github.com/cameleonnbss/OptimizeKit/releases |

`OptimizeKit.exe` est **100 % statique** (compilé MinGW-w64, ~5 Mo) : aucune DLL, aucune
installation, aucun compte. Il embarque un serveur HTTP et le dashboard DarkGPT (dossier
`web/` à côté de l'exe). Tu le poses où tu veux et tu lances.

## 🚀 Démarrage rapide

| Tu veux | Double-clique |
|---|---|
| Le dashboard (interface web, fenêtre autonome) | `OptimizeKit-user.bat` |
| **Tout le kit** (tous les tweaks, prompt UAC) | `OptimizeKit-admin.bat` |
| Le menu CLI à chiffres | `OptimizeKit-cli.bat` |
| Le moteur PowerShell pur (sans exe) | `PowerShell\OptimizeKit.ps1` |
| L'ancienne fenêtre native Direct2D | `OptimizeKit.exe --native` |

Ensuite choisis un profil et regarde l'onglet **Logs** : chaque action est écrite dans
`%LOCALAPPDATA%\OptimizeKit\OptimizeKit.log`, et chaque clé de registre touchée est
**sauvegardée en `.reg` avant modification**.

> 🛡️ **Sécurité** : chaque tweak a son « restaurer les valeurs Windows » (GUI + CLI),
> le moteur PowerShell a un mode `-Restore`, et les sauvegardes `.reg` sont dans
> `%LOCALAPPDATA%\OptimizeKit\`.

## 🖥️ Le dashboard (liquid glass, style WormGPT)

L'interface reprend le **design system DarkGPT** de WormGPT-desktop (cameleonnbss) : noir
profond + accent rouge exclusif `#ff3d57`, glassmorphism avec reflet spéculaire animé,
particules rouges qui suivent la souris, grille de fond + noise + vignette, polices Inter
et Space Mono. Servi par un serveur HTTP C++ embarqué sur `127.0.0.1:8765`, ouvert comme
fenêtre d'application autonome (msedge `--app`).

| Vue | Contenu |
|---|---|
| **Dashboard** | **Monitoring live** : CPU / RAM / GPU avec sparklines 60 s, débits disque & réseau, process/threads, top processus par CPU (2,5 s), résumé système complet, profils 1 clic |
| **Tweaks** | Les 30 tweaks avec badges ADMIN/USER, impact, filtres, multi-sélection apply/restore |
| **Gaming** | Profil gaming + 8 raccourcis latence (DVR, réseau, timer, HAGS, MPO, alim, souris, FSO) |
| **Privacy** | Profil privacy + 8 raccourcis (télémétrie, pub, activité, Bing, Copilot, Edge…) |
| **Drivers** | GPU + version du pilote (auto-détectée), pages constructeur, dxdiag, Gestionnaire de périphériques |
| **Network** | Test de latence ICMP + ping rapide de n'importe quel hôte |
| **Logs** | Journal d'activité complet, coloré, en direct |
| **About** | Crédits et emplacement des sauvegardes |

## 📊 Monitoring live

- **CPU %** (agrégé, PDH `Processor Information`) + % de la fréquence de base
- **RAM %** + octets utilisés/total (`GlobalMemoryStatusEx`)
- **GPU %** (PDH `GPU Engine`, tous les moteurs fusionnés) — marche sur NVIDIA / AMD / Intel
- **Disque** % occupé + lecture/écriture Mo/s, **Réseau** Ko/s montant/descendant
- **Top processus** par CPU avec RAM et PID (delta 250 ms)
- **Historique 60 secondes** en sparklines lumineuses, rafraîchi chaque seconde

## 🛠️ Les 30 tweaks

**Gaming / FPS / latence** — Game Mode · Game DVR & Game Bar off · HAGS (scheduling GPU
matériel) · MPO off (correctif stutter/flicker 24H2) · résolution du timer global 0,5 ms ·
stack réseau gaming (`TcpAckFrequency=1`, `TCPNoDelay`, `NetworkThrottlingIndex=0xFFFFFFFF`,
`SystemResponsiveness=0`, priorité MMCSS Games) · accélération souris off (1:1 brut) ·
effets visuels performance · menus instantanés · apps en arrière-plan off · Storage Sense ·
indexation off · SysMain off · HPET off · plan Ultimate Performance ·
`Win32PrioritySeparation=0x26` · préférence GPU performances élevées · fullscreen
optimizations off · services Xbox off.

**Confidentialité** — télémétrie off (`AllowTelemetry=0`, DiagTrack, dmwappush) · ID
publicitaire · historique d'activité / timeline · Bing hors recherche · expériences
personnalisées · tâches planifiées CEIP · Copilot supprimé · Edge en fond & startup boost off.

**Débbiottage** — suppression des apps MS Store (Clipchamp, News, Solitaire, Teams, Xbox…) ·
désinstallation OneDrive · trim du boot.

Mapping complet et annoté dans [`docs/SOURCES.md`](docs/SOURCES.md) — sourcé de
**WinUtil de Chris Titus (MIT)**, des docs Microsoft/Valve et de la communauté PC gaming.

## 💻 CLI — menus à chiffres

`OptimizeKit-cli.bat` (ou `OptimizeKit.exe --cli`) ouvre un menu adapté à tes privilèges :

```
-- ADMIN MODE (full power) --
  1. System information
  2. Apply GAMING profile (one click)
  3. Apply PRIVACY profile
  4. Apply FULL kit profile
  5. Select multiple tweaks (numbers, a/n/d)
  6. Apply or restore a single tweak
  7. GAME BOOST : boost a running process priority
  8. NETWORK : saved targets latency test
  9. CLEAN : junk cleanup + recycle bin
 10. DRIVERS menu
 11. Registry backup
 12. Activity log
  0. Exit
```

Commandes directes (scriptables) :

```bat
OptimizeKit.exe --info                 " résumé système
OptimizeKit.exe --list                 " tous les ids de tweaks
OptimizeKit.exe --profile gaming       " gaming | privacy | full
OptimizeKit.exe --apply  timer_high
OptimizeKit.exe --restore timer_high
OptimizeKit.exe --clean                " nettoyage des déchets
OptimizeKit.exe --ping 1.1.1.1
```

## 🧪 Moteur PowerShell (sans exe)

```powershell
powershell -ExecutionPolicy Bypass -File PowerShell\OptimizeKit.ps1 -User   " menu utilisateur
powershell -ExecutionPolicy Bypass -File PowerShell\OptimizeKit.ps1         " s'élève tout seul, menu admin
powershell -ExecutionPolicy Bypass -File PowerShell\OptimizeKit.ps1 -Silent " kit complet sans menus
powershell -ExecutionPolicy Bypass -File PowerShell\OptimizeKit.ps1 -Restore
```

Mêmes tweaks, mêmes logs (`OptimizeKit-PowerShell.log`), mêmes sauvegardes de registre,
menus à chiffres.

## 📁 Où tout est stocké

| Quoi | Où |
|---|---|
| Config (cibles ping, tweaks appliqués) | `%LOCALAPPDATA%\OptimizeKit\config.json` |
| Journal d'activité | `%LOCALAPPDATA%\OptimizeKit\OptimizeKit.log` |
| Log PowerShell | `%LOCALAPPDATA%\OptimizeKit\OptimizeKit-PowerShell.log` |
| Sauvegardes registre (auto, avant chaque modif) | `%LOCALAPPDATA%\OptimizeKit\backup_*.reg` |

Supprime le dossier pour repartir de zéro, ou lance `Uninstall-OptimizeKit.bat`.

## 🔨 Compiler depuis les sources

Prérequis : Windows 10/11 et [MinGW-w64](https://winlibs.com) (`g++` + `windres`) dans le PATH.

```bat
build.bat
```

Résultat : `dist\OptimizeKit.exe`. CMake fonctionne aussi (`cmake -B build && cmake --build build`).
La CI compile à chaque push (`.github/workflows/build.yml`).

## ⚠️ Avertissement

Les tweaks de registre modifient ton système. Le kit sauvegarde chaque clé touchée et peut
restaurer les valeurs par défaut tweak par tweak, mais **toi** seul décide : lis la
description d'un tweak avant de l'appliquer, et redémarre après HAGS / timer / plan
d'alimentation. Non affilié à Microsoft. Utilise à tes propres risques.

## 📜 Crédits & sources

- [ChrisTitusTech/winutil](https://github.com/ChrisTitusTech/winutil) (MIT) — base déblobattage & confidentialité
- Docs Microsoft — [HAGS](https://learn.microsoft.com/windows/win32/direct3d12/hardware-accelerated-gpu-scheduling), Game Bar/DVR, MPO `OverlayTestMode`
- Communauté — [TimerResolution-Optimization](https://github.com/insovs/TimerResolution-Optimization), MarkC mouse fix, guides de latence réseau
- [nlohmann/json](https://github.com/nlohmann/json) (MIT, header unique vendu dans le repo)
- Style d'interface inspiré de [WormGPT-desktop](https://github.com/cameleonnbss/WormGPT-desktop)

---

MIT © 2026 [cameleonnbss](https://github.com/cameleonnbss)
