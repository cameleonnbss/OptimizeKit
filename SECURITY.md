# 🔒 Politique de sécurité — OptimizeKit

Dernière mise à jour : 23 septembre 2026 · Concerne OptimizeKit **v2.7.0**

## Notre position en matière de sécurité

OptimizeKit est un **outil de configuration système**, pas un optimiseur magique. Nos règles :

1. **Chaque action est réversible.** Chaque tweak stocke la valeur Windows d'origine et la restaure en un clic (UI native, UI web ou CLI). Le moteur PowerShell a `-RestoreAll`.
2. **Sauvegarde avant modification.** Avant chaque application, l'exe exporte les clés concernées vers `%LOCALAPPDATA%\OptimizeKit\backup_*.reg`.
3. **Dépôt verrouillé.** Le CI MinGW-w64 construit l'exe ; aucun binaire tier n'est jamais exécuté pendant le build.
4. **Pas de composants navigateur.** La fenêtre par défaut est du Direct2D pur. Même le dashboard web optionnel se lie uniquement au loopback (127.0.0.1) — jamais exposé au réseau, jamais lancé automatiquement au boot.
5. **Pas de services, pas d'auto-start.** OptimizeKit n'installe aucun service et n'enregistre aucune tâche planifiée. Il n'agit que quand vous demandez.
6. **Pas de pilotes, pas de kernel drivers.** Tout passe par des APIs Windows documentées (registre, WMI, `SetupAPI`, services API, powercfg), les mêmes que le Panneau de configuration.

## Éléments firmware (BIOS / UEFI / TPM)

- Le panneau firmware est **strictement en lecture seule**. OptimizeKit n'écrit jamais dans NVRAM/variables UEFI, ne touche jamais aux clés de secure boot, ne flashe jamais un BIOS.
- Les paramètres kernel exposés (`dynamic tick`, `platform tick`, `TSC sync`, dump level) sont des clés `bcdedit`/registre standard documentées, chaque option possède une restauration vers la valeur par défaut de Windows.
- Le blocage WPBT ajoute `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\DisableWpbtExecution = 1` — une politique Microsoft documentée qui empêche la table WPBT du vendeur de lancer du code au boot.
- Les cartes de guide BIOS dans le dashboard web pointent vers les pages constructeurs officielles. Nous n'hébergeons jamais des firmwares.

## Mises à jour de pilotes

Le moteur de pilotes suit le même principe d'opt-in :

- Il **rapporte** : âge du pilote (date de la clé `DriverDate` de `Control\Class`), périphériques en erreur (codes CM_PROB), correspondances constructeur.
- Il **déclenche** : scan de pilotes Windows Update via `UsoClient` (l'orchestrateur officiel de l'appli Paramètres) ou un rescan PnP. Ou ouvre la page de téléchargement du constructeur.
- Il ne télécharge jamais un binaire de pilote lui-même. Aucun serveur tiers, aucun proxy, aucune injection de DLL.

## Réseau

- Le serveur web embarqué écoute sur **127.0.0.1 uniquement**, sauf si vous passez explicitement `--web <port>` ; l'exe natif n'ouvre aucun socket.
- Aucune donnée télémétrique n'est collectée, stockée, ni transmise. Pas de rapport de crash.
- Les requêtes réseau (ping, DNS) ne partent que vers les cibles que vous choisissez dans l'UI.

## Divulgation responsable

Une faille ? **Ne créez pas d'issue publique.** Ouvrez un [Security Advisory](https://github.com/cameleonnbss/OptimizeKit/security/advisories/new) privé ou contactez le mainteneur via le profil GitHub. Réponse visée : 72 h.

## Correspondance des versions supportées

| Version | Supportée |
|---|---|
| 2.7.x | ✅ |
| < 2.7 | ⚠️ Mise à jour recommandée |

## Archives

Les releases sont sur la page [Releases](../../releases). Le hash SHA256 de chaque asset est publié dans la release elle-même (via le workflow CI).
