// OptimizeKit - built-in game database.
//
// One table, three jobs:
//   1. detection  - recognise an installed game from its exe stem, its install folder name
//                   or its launcher display name, even when no launcher manifest exists;
//   2. naming     - give a detected game its real title instead of an exe basename;
//   3. genre      - give every known title a family (fps, mmo, racing...) so the Game
//                   Library can suggest the right tweak set for it.
//
// Header-only and constexpr: no build-file change, no allocation, no I/O. `lookup()` is a
// linear scan over a few hundred short strings - microseconds, called once per candidate.
#pragma once
#include "common.h"
#include <cwctype>

namespace ok::gamedb {

struct Entry {
    const wchar_t* exe;    // primary executable stem (lowercase, no extension)
    const wchar_t* name;   // display name
    const char*    family; // fps|br|moba|mmo|coop|rpg|openworld|racing|fighting|sports|
                           // horror|sandbox|party|strategy|sim|survival|roguelike
    const wchar_t* alias;  // '|'-separated alternative exe stems / folder names ("" if none)
};

// ~300 titles: the ones people actually run on Windows. Families drive the suggested pack.
inline const Entry kGames[] = {
    // ---------------------------------------------------------------- competitive FPS
    { L"cs2",                     L"Counter-Strike 2",                  "fps",      L"csgo|cstrike" },
    { L"csgo",                    L"Counter-Strike: Global Offensive",  "fps",      L"" },
    { L"valorant-win64-shipping", L"Valorant",                          "fps",      L"valorant|riotvalorant" },
    { L"r5apex",                  L"Apex Legends",                      "fps",      L"apex|r5apex_dx12" },
    { L"overwatch",               L"Overwatch 2",                       "fps",      L"" },
    { L"cod",                     L"Call of Duty: HQ",                  "fps",      L"codhq|callofduty" },
    { L"modernwarfare",           L"CoD: Modern Warfare (2019)",        "fps",      L"mw2019" },
    { L"bf2042",                  L"Battlefield 2042",                  "fps",      L"" },
    { L"bfv",                     L"Battlefield V",                     "fps",      L"" },
    { L"bf1",                     L"Battlefield 1",                     "fps",      L"" },
    { L"bf4",                     L"Battlefield 4",                     "fps",      L"" },
    { L"rainbowsix",              L"Rainbow Six Siege",                 "fps",      L"r6|r6siege|rainbowsix_dx11" },
    { L"thefinals",               L"The Finals",                        "fps",      L"discovery" },
    { L"huntgame",                L"Hunt: Showdown",                    "fps",      L"hunt" },
    { L"escapefromtarkov",        L"Escape from Tarkov",                "fps",      L"eft" },
    { L"darktide",                L"Warhammer 40K: Darktide",           "fps",      L"" },
    { L"spacemarine2",            L"Warhammer 40K: Space Marine 2",     "fps",      L"" },
    { L"squadgame",               L"Squad",                             "fps",      L"squad|squadlauncher" },
    { L"hll",                     L"Hell Let Loose",                    "fps",      L"hll-win64-shipping" },
    { L"readyornot",              L"Ready or Not",                      "fps",      L"" },
    { L"insurgencyclient",        L"Insurgency: Sandstorm",             "fps",      L"sandstorm|insurgency" },
    { L"fsd",                     L"Deep Rock Galactic",                "fps",      L"deeprockgalactic|deeprock" },
    { L"deltaforceclient",        L"Delta Force",                       "fps",      L"deltaforce" },
    { L"grayzonewarfare",         L"Gray Zone Warfare",                 "fps",      L"grayzone" },
    { L"stalker2",                L"S.T.A.L.K.E.R. 2",                  "fps",      L"stalker2-win64-shipping" },
    { L"planetside2",             L"PlanetSide 2",                      "fps",      L"" },
    { L"tf2",                     L"Team Fortress 2",                   "fps",      L"team fortress 2" },
    { L"l4d2",                    L"Left 4 Dead 2",                     "fps",      L"left4dead2" },
    { L"doometernal",             L"DOOM Eternal",                      "fps",      L"doom" },
    { L"farcry6",                 L"Far Cry 6",                         "fps",      L"" },
    { L"farcry5",                 L"Far Cry 5",                         "fps",      L"" },
    { L"farcry4",                 L"Far Cry 4",                         "fps",      L"" },
    { L"farcry3",                 L"Far Cry 3",                         "fps",      L"" },
    { L"metroexodus",             L"Metro Exodus",                      "fps",      L"" },
    { L"borderlands3",            L"Borderlands 3",                     "fps",      L"" },
    { L"borderlands2",            L"Borderlands 2",                     "fps",      L"" },
    { L"payday3",                 L"Payday 3",                          "fps",      L"" },
    { L"payday2",                 L"Payday 2",                          "fps",      L"" },
    { L"portal2",                 L"Portal 2",                          "fps",      L"" },
    { L"hl2",                     L"Half-Life 2",                       "fps",      L"" },
    { L"splitgate2",              L"Splitgate 2",                       "fps",      L"" },
    { L"marvelrivals",            L"Marvel Rivals",                     "fps",      L"marvelrivals-win64-shipping" },
    { L"paladins",                L"Paladins",                          "fps",      L"paladins-win64-shipping" },

    // ---------------------------------------------------------------- battle royale
    { L"fortniteclient-win64-shipping", L"Fortnite",                    "br",       L"fortnite|fortnitelauncher" },
    { L"tslgame",                 L"PUBG: Battlegrounds",                "br",       L"pubg" },
    { L"narakabladepoint",        L"Naraka: Bladepoint",                "br",       L"naraka" },
    { L"warzone",                 L"CoD: Warzone",                      "br",       L"" },
    { L"thecycle",                L"The Cycle: Frontier",               "br",       L"" },
    { L"superpeople",             L"Super People",                      "br",       L"" },
    { L"vampirebattleroyale",     L"Vampire: Bloodhunt",                "br",       L"bloodhunt" },

    // ---------------------------------------------------------------- MOBA
    { L"dota2",                   L"Dota 2",                            "moba",     L"" },
    { L"league of legends",       L"League of Legends",                 "moba",     L"leagueclient|league of legends" },
    { L"smite",                   L"SMITE",                             "moba",     L"smitegame" },
    { L"project8",                L"Deadlock",                          "moba",     L"deadlock" },
    { L"heroesofthestorm",        L"Heroes of the Storm",               "moba",     L"hots" },
    { L"predecessor",             L"Predecessor",                       "moba",     L"" },

    // ---------------------------------------------------------------- MMO / live service
    { L"wow",                     L"World of Warcraft",                 "mmo",      L"wowclassic|wowt" },
    { L"ffxiv_dx11",              L"Final Fantasy XIV",                 "mmo",      L"ffxiv|ffxivboot" },
    { L"newworld",                L"New World",                         "mmo",      L"newworldlauncher" },
    { L"lostark",                 L"Lost Ark",                          "mmo",      L"" },
    { L"blackdesert64",           L"Black Desert Online",               "mmo",      L"blackdesert" },
    { L"destiny2",                L"Destiny 2",                         "mmo",      L"" },
    { L"warframex64",             L"Warframe",                          "mmo",      L"warframe" },
    { L"pathofexile",             L"Path of Exile",                     "mmo",      L"pathofexile_x64" },
    { L"pathofexile2",            L"Path of Exile 2",                   "mmo",      L"pathofexile_x64steam" },
    { L"gw2-64",                  L"Guild Wars 2",                      "mmo",      L"gw2" },
    { L"eso64",                   L"The Elder Scrolls Online",          "mmo",      L"eso" },
    { L"swtor",                   L"Star Wars: The Old Republic",       "mmo",      L"" },
    { L"throne",                  L"Throne and Liberty",                "mmo",      L"tl" },
    { L"genshinimpact",           L"Genshin Impact",                    "mmo",      L"genshin" },
    { L"starrail",                L"Honkai: Star Rail",                 "mmo",      L"" },
    { L"zenlesszonezero",         L"Zenless Zone Zero",                 "mmo",      L"zzz" },
    { L"wutheringwaves",          L"Wuthering Waves",                   "mmo",      L"" },
    { L"toweroffantasy",          L"Tower of Fantasy",                  "mmo",      L"" },
    { L"thedivision2",            L"The Division 2",                    "mmo",      L"division2" },
    { L"thefirstdescendant",      L"The First Descendant",              "mmo",      L"firstdescendant" },
    { L"oncehuman",               L"Once Human",                        "mmo",      L"" },
    { L"duneawakening",           L"Dune: Awakening",                   "mmo",      L"" },
    { L"rs2client",               L"RuneScape",                         "mmo",      L"runescape" },
    { L"osclient",                L"Old School RuneScape",              "mmo",      L"runescape" },
    { L"exefile",                 L"EVE Online",                        "mmo",      L"eveonline" },
    { L"albion-online",           L"Albion Online",                     "mmo",      L"albion" },
    { L"bladeandsoul",            L"Blade & Soul",                      "mmo",      L"" },
    { L"archeage",                L"ArcheAge",                          "mmo",      L"" },
    { L"lastepoch",               L"Last Epoch",                        "mmo",      L"" },
    { L"torchlight3",             L"Torchlight III",                    "mmo",      L"" },
    { L"grimdawn",                L"Grim Dawn",                         "mmo",      L"" },
    { L"diablo iv",               L"Diablo IV",                         "mmo",      L"diablo4" },
    { L"diablo iii",              L"Diablo III",                        "mmo",      L"diablo3" },
    { L"diablo ii resurrected",   L"Diablo II: Resurrected",            "mmo",      L"diablo2r" },

    // ---------------------------------------------------------------- co-op / PvE
    { L"helldivers2",             L"Helldivers 2",                      "coop",     L"" },
    { L"valheim",                 L"Valheim",                           "coop",     L"" },
    { L"vrising",                 L"V Rising",                          "coop",     L"vrisingserver" },
    { L"remnant2",                L"Remnant II",                        "coop",     L"" },
    { L"monsterhunterwilds",      L"Monster Hunter Wilds",              "coop",     L"mhwilds" },
    { L"monsterhunterworld",      L"Monster Hunter: World",             "coop",     L"mhw" },
    { L"monsterhunterrise",       L"Monster Hunter Rise",               "coop",     L"mhrise" },
    { L"sotgame",                 L"Sea of Thieves",                    "coop",     L"sot|seaofthieves" },
    { L"enshrouded",              L"Enshrouded",                        "coop",     L"" },
    { L"sotf",                    L"Sons of the Forest",                 "coop",     L"sonsoftheforest" },
    { L"theforest",               L"The Forest",                        "coop",     L"" },
    { L"raft",                    L"Raft",                              "coop",     L"" },
    { L"icarus",                  L"ICARUS",                            "coop",     L"" },
    { L"grounded",                L"Grounded",                          "coop",     L"" },
    { L"palworld-win64-shipping", L"Palworld",                          "coop",     L"palworld" },
    { L"factorygame-win64-shipping", L"Satisfactory",                   "coop",     L"satisfactory" },
    { L"factorio",                L"Factorio",                          "coop",     L"" },
    { L"dst",                     L"Don't Starve Together",             "coop",     L"" },
    { L"terraria",                L"Terraria",                          "coop",     L"" },
    { L"stardew valley",          L"Stardew Valley",                    "coop",     L"stardewvalley" },
    { L"repo",                    L"R.E.P.O.",                          "coop",     L"" },
    { L"contentwarning",          L"Content Warning",                   "coop",     L"" },
    { L"peak",                    L"PEAK",                              "coop",     L"" },
    { L"lethal",                  L"Lethal Company",                    "coop",     L"" },
    { L"astroneer",               L"Astroneer",                         "coop",     L"" },
    { L"spaceengineers",          L"Space Engineers",                   "coop",     L"" },
    { L"scrapmechanic",           L"Scrap Mechanic",                    "coop",     L"" },
    { L"thedivision",             L"The Division",                      "coop",     L"" },
    { L"destiny",                 L"Destiny",                           "coop",     L"" },
    { L"ragnarok",                L"Ragnarok Online",                   "coop",     L"" },
    { L"muck",                    L"Muck",                              "coop",     L"" },
    { L"dungeondefenders2",       L"Dungeon Defenders II",              "coop",     L"" },
    { L"warhammer vermintide 2",  L"Warhammer: Vermintide 2",           "coop",     L"vermintide2" },
    { L"gtfo",                    L"GTFO",                              "coop",     L"" },
    { L"back4blood",              L"Back 4 Blood",                      "coop",     L"" },

    // ---------------------------------------------------------------- RPG / action
    { L"cyberpunk2077",           L"Cyberpunk 2077",                    "rpg",      L"" },
    { L"witcher3",                L"The Witcher 3",                     "rpg",      L"" },
    { L"eldenring",               L"Elden Ring",                        "rpg",      L"" },
    { L"sekiro",                  L"Sekiro: Shadows Die Twice",         "rpg",      L"" },
    { L"darksoulsiii",            L"Dark Souls III",                    "rpg",      L"ds3" },
    { L"darksoulsii",             L"Dark Souls II",                     "rpg",      L"ds2" },
    { L"darksoulsremastered",     L"Dark Souls: Remastered",            "rpg",      L"dsr" },
    { L"bg3",                     L"Baldur's Gate 3",                   "rpg",      L"bg3_dx11" },
    { L"starfield",               L"Starfield",                         "rpg",      L"" },
    { L"skyrimse",                L"Skyrim Special Edition",            "rpg",      L"tesv|skyrim" },
    { L"fallout4",                L"Fallout 4",                         "rpg",      L"" },
    { L"fallout76",               L"Fallout 76",                        "rpg",      L"" },
    { L"kingdomcome2",            L"Kingdom Come: Deliverance II",      "rpg",      L"kcd2" },
    { L"kingdomcome",             L"Kingdom Come: Deliverance",         "rpg",      L"kcd" },
    { L"hogwartslegacy",          L"Hogwarts Legacy",                   "rpg",      L"hogwarts" },
    { L"ghostoftsushima",         L"Ghost of Tsushima",                 "rpg",      L"ghostts" },
    { L"tlou-i",                  L"The Last of Us Part I",             "rpg",      L"tlou" },
    { L"tlou-ii",                 L"The Last of Us Part II Remastered", "rpg",      L"" },
    { L"spiderman",               L"Spider-Man Remastered",             "rpg",      L"spidermanr" },
    { L"spiderman2",              L"Marvel's Spider-Man 2",             "rpg",      L"" },
    { L"horizonfw",               L"Horizon Forbidden West",            "rpg",      L"" },
    { L"horizenzd",               L"Horizon Zero Dawn",                 "rpg",      L"" },
    { L"gow",                     L"God of War",                        "rpg",      L"" },
    { L"gowragnarok",             L"God of War Ragnarök",               "rpg",      L"" },
    { L"avowed",                  L"Avowed",                            "rpg",      L"" },
    { L"thegreatcircle",          L"Indiana Jones & the Great Circle",  "rpg",      L"indianajones" },
    { L"b1-win64-shipping",       L"Black Myth: Wukong",                "rpg",      L"wukong" },
    { L"liesofp",                 L"Lies of P",                         "rpg",      L"" },
    { L"armoredcore6",            L"Armored Core VI",                   "rpg",      L"" },
    { L"dragonsdogma2",           L"Dragon's Dogma 2",                  "rpg",      L"" },
    { L"outerworlds",             L"The Outer Worlds",                  "rpg",      L"" },
    { L"nierautomata",            L"NieR:Automata",                     "rpg",      L"nier" },
    { L"p5r",                     L"Persona 5 Royal",                   "rpg",      L"persona5" },
    { L"p3r",                     L"Persona 3 Reload",                  "rpg",      L"" },
    { L"ff7remake",               L"FINAL FANTASY VII REMAKE",          "rpg",      L"" },
    { L"ff16",                    L"FINAL FANTASY XVI",                 "rpg",      L"" },
    { L"dragonage the veilguard", L"Dragon Age: The Veilguard",         "rpg",      L"dragonage4" },
    { L"mass effect legendary edition", L"Mass Effect Legendary Edition", "rpg",    L"" },
    { L"greedfall",               L"GreedFall",                         "rpg",      L"" },
    { L"vampyr",                  L"Vampyr",                            "rpg",      L"" },
    { L"gothic1",                 L"Gothic",                            "rpg",      L"" },
    { L"divinity original sin 2", L"Divinity: Original Sin 2",          "rpg",      L"eocapp" },
    { L"pillarsofeternity2",      L"Pillars of Eternity II",            "rpg",      L"" },

    // ---------------------------------------------------------------- open world / sandbox
    { L"gtav",                    L"Grand Theft Auto V",                "openworld", L"gta5" },
    { L"rdr2",                    L"Red Dead Redemption 2",             "openworld", L"" },
    { L"gtaiv",                   L"Grand Theft Auto IV",               "openworld", L"" },
    { L"minecraft.windows",       L"Minecraft (Bedrock)",               "sandbox",  L"minecraft" },
    { L"minecraft",               L"Minecraft (Java)",                  "sandbox",  L"javaw" },
    { L"rustclient",              L"Rust",                              "sandbox",  L"rust" },
    { L"dayz",                    L"DayZ",                              "sandbox",  L"dayz_x64" },
    { L"arma3_x64",               L"Arma 3",                            "sandbox",  L"arma3" },
    { L"armareforger",            L"Arma Reforger",                     "sandbox",  L"" },
    { L"arkascended",             L"ARK: Survival Ascended",            "sandbox",  L"arkasa|arkascended-win64-shipping" },
    { L"shootergame",             L"ARK: Survival Evolved",             "sandbox",  L"arksurvival" },
    { L"nomanssky",               L"No Man's Sky",                      "sandbox",  L"" },
    { L"7daystodie",              L"7 Days to Die",                     "survival", L"" },
    { L"conansandbox",            L"Conan Exiles",                      "survival", L"" },
    { L"projectzomboid",          L"Project Zomboid",                   "survival", L"" },
    { L"greenhell",               L"Green Hell",                        "survival", L"" },
    { L"subnautica",              L"Subnautica",                        "survival", L"" },
    { L"subnauticabetweenus",     L"Subnautica: Below Zero",            "survival", L"subnauticabelowzero" },
    { L"robloxplayerbeta",        L"Roblox",                            "sandbox",  L"roblox" },
    { L"robloxcrashhandler",      L"Roblox Studio",                     "sandbox",  L"robloxstudio" },
    { L"teardown",                L"Teardown",                          "sandbox",  L"" },
    { L"brickrigs",               L"Brick Rigs",                        "sandbox",  L"" },
    { L"beamng.drive",            L"BeamNG.drive",                      "sandbox",  L"beamng" },
    { L"cities2",                 L"Cities: Skylines II",               "sim",      L"cities skylines ii" },
    { L"citiesskylines",          L"Cities: Skylines",                  "sim",      L"" },
    { L"anno1800",                L"Anno 1800",                         "strategy", L"" },
    { L"tropico6",                L"Tropico 6",                         "strategy", L"" },
    { L"survivingmars",           L"Surviving Mars",                    "strategy", L"" },
    { L"oxygennotincluded",       L"Oxygen Not Included",               "strategy", L"" },
    { L"rimworldwin64",           L"RimWorld",                          "strategy", L"rimworld" },
    { L"northgard",               L"Northgard",                         "strategy", L"" },
    { L"theyarebillions",         L"They Are Billions",                 "strategy", L"" },

    // ---------------------------------------------------------------- racing / sim
    { L"forzahorizon5",           L"Forza Horizon 5",                   "racing",   L"forza5" },
    { L"forzahorizon4",           L"Forza Horizon 4",                   "racing",   L"forza4" },
    { L"forzamotorsport",         L"Forza Motorsport",                  "racing",   L"forzams" },
    { L"eurotrucks2",             L"Euro Truck Simulator 2",            "racing",   L"ets2" },
    { L"amtrucks",                L"American Truck Simulator",          "racing",   L"ats" },
    { L"acs",                     L"Assetto Corsa Competizione",        "racing",   L"acc" },
    { L"assettocorsa",            L"Assetto Corsa",                     "racing",   L"" },
    { L"iracingsim64dx11",        L"iRacing",                           "racing",   L"iracing" },
    { L"dirtrally2",              L"DiRT Rally 2.0",                    "racing",   L"" },
    { L"f1_24",                   L"F1 24",                             "racing",   L"" },
    { L"f1_23",                   L"F1 23",                             "racing",   L"" },
    { L"rfactor2",                L"rFactor 2",                         "racing",   L"" },
    { L"automobilista2",          L"Automobilista 2",                   "racing",   L"" },
    { L"nfsunbound",              L"Need for Speed Unbound",            "racing",   L"" },
    { L"needforspeedheat",        L"Need for Speed Heat",               "racing",   L"" },
    { L"trackmania",              L"Trackmania",                        "racing",   L"" },
    { L"thecrew2",                L"The Crew 2",                        "racing",   L"" },
    { L"snowrunner",              L"SnowRunner",                        "racing",   L"" },
    { L"aeroflyfs4",              L"Aerofly FS 4",                      "sim",      L"" },
    { L"flightsimulator",         L"Microsoft Flight Simulator",        "sim",      L"mfs2020" },
    { L"dcs",                     L"DCS World",                         "sim",      L"" },
    { L"x-plane",                 L"X-Plane 12",                        "sim",      L"xplane12" },
    { L"elitedangerous64",        L"Elite Dangerous",                   "sim",      L"elitedangerous" },
    { L"starcitizen",             L"Star Citizen",                      "sim",      L"" },
    { L"farmingsimulator2025",    L"Farming Simulator 25",              "sim",      L"" },
    { L"farmingsimulator2022",    L"Farming Simulator 22",              "sim",      L"" },
    { L"ksp_x64",                 L"Kerbal Space Program",              "sim",      L"kerbal" },
    { L"planetcoaster2",          L"Planet Coaster 2",                  "sim",      L"" },
    { L"houseflipper2",           L"House Flipper 2",                   "sim",      L"" },
    { L"twopointmuseum",          L"Two Point Museum",                  "sim",      L"" },

    // ---------------------------------------------------------------- fighting
    { L"tekken8",                 L"Tekken 8",                          "fighting", L"polished" },
    { L"tekken7",                 L"Tekken 7",                          "fighting", L"" },
    { L"streetfighter6",          L"Street Fighter 6",                  "fighting", L"sf6" },
    { L"mk1",                     L"Mortal Kombat 1",                   "fighting", L"" },
    { L"mk11",                    L"Mortal Kombat 11",                  "fighting", L"" },
    { L"guiltygearstrive",        L"Guilty Gear -Strive-",              "fighting", L"ggst" },
    { L"dbfz",                    L"Dragon Ball FighterZ",              "fighting", L"dragonballfighterz" },
    { L"gbvsr",                   L"Granblue Fantasy Versus: Rising",   "fighting", L"granblue" },
    { L"brawlhalla",              L"Brawlhalla",                        "fighting", L"" },
    { L"soulcaliburvi",           L"Soulcalibur VI",                    "fighting", L"sc6" },
    { L"thekingoffightersxv",     L"The King of Fighters XV",           "fighting", L"kof15" },
    { L"multiversus",             L"MultiVersus",                       "fighting", L"" },
    { L"skullgirls",              L"Skullgirls 2nd Encore",             "fighting", L"" },

    // ---------------------------------------------------------------- sports / party
    { L"fc25",                    L"EA Sports FC 25",                   "sports",   L"eafc25" },
    { L"fc24",                    L"EA Sports FC 24",                   "sports",   L"eafc24" },
    { L"fifa23",                  L"FIFA 23",                           "sports",   L"" },
    { L"nba2k25",                 L"NBA 2K25",                          "sports",   L"" },
    { L"nba2k24",                 L"NBA 2K24",                          "sports",   L"" },
    { L"madden25",                L"Madden NFL 25",                     "sports",   L"" },
    { L"rocketleague",            L"Rocket League",                     "sports",   L"" },
    { L"wt",                      L"War Thunder",                       "sports",   L"aces" },
    { L"worldoftanks",            L"World of Tanks",                    "sports",   L"wot" },
    { L"worldofwarships",         L"World of Warships",                 "sports",   L"" },
    { L"fallguys_client",         L"Fall Guys",                         "party",    L"fallguys" },
    { L"among us",                L"Among Us",                          "party",    L"amongus" },
    { L"ittakestwo",              L"It Takes Two",                      "party",    L"" },
    { L"awayout",                 L"A Way Out",                         "party",    L"" },
    { L"overcooked2",             L"Overcooked! 2",                     "party",    L"" },
    { L"humanfallflat",           L"Human Fall Flat",                   "party",    L"" },
    { L"gangbeasts",              L"Gang Beasts",                       "party",    L"" },
    { L"golfwithyourfriends",     L"Golf With Your Friends",            "party",    L"" },
    { L"pummelparty",             L"Pummel Party",                      "party",    L"" },
    { L"jackboxparty",            L"The Jackbox Party Pack",            "party",    L"" },
    { L"stumbleguys",             L"Stumble Guys",                      "party",    L"" },

    // ---------------------------------------------------------------- horror
    { L"deadbydaylight-win64-shipping", L"Dead by Daylight",            "horror",   L"dbd|deadbyd" },
    { L"phasmophobia",            L"Phasmophobia",                      "horror",   L"phasmo" },
    { L"re4",                     L"Resident Evil 4",                   "horror",   L"" },
    { L"re2",                     L"Resident Evil 2",                   "horror",   L"" },
    { L"re3",                     L"Resident Evil 3",                   "horror",   L"" },
    { L"re8",                     L"Resident Evil Village",             "horror",   L"revillage" },
    { L"re7",                     L"Resident Evil 7",                   "horror",   L"" },
    { L"outlast2",                L"Outlast 2",                         "horror",   L"" },
    { L"alienisolation",          L"Alien: Isolation",                  "horror",   L"ai" },
    { L"deadspace",               L"Dead Space",                        "horror",   L"" },
    { L"visage",                  L"Visage",                            "horror",   L"" },
    { L"devour",                  L"DEVOUR",                            "horror",   L"" },
    { L"demonologist",            L"Demonologist",                      "horror",   L"" },
    { L"fnaf",                    L"Five Nights at Freddy's",           "horror",   L"fivenightsatfreddys" },
    { L"soma",                    L"SOMA",                              "horror",   L"" },
    { L"amnesiathedarkdescent",   L"Amnesia: The Dark Descent",         "horror",   L"" },
    { L"theevilwithin2",          L"The Evil Within 2",                 "horror",   L"" },

    // ---------------------------------------------------------------- strategy
    { L"civilizationvi",          L"Civilization VI",                   "strategy", L"civ6" },
    { L"civilizationvii",         L"Civilization VII",                  "strategy", L"civ7" },
    { L"aoe2de",                  L"Age of Empires II: DE",             "strategy", L"aoe2" },
    { L"aoe4",                    L"Age of Empires IV",                 "strategy", L"aoe4_s" },
    { L"warhammer3",              L"Total War: WARHAMMER III",          "strategy", L"" },
    { L"threekingdoms",           L"Total War: THREE KINGDOMS",         "strategy", L"three_kingdoms" },
    { L"rome2",                   L"Total War: ROME II",                "strategy", L"" },
    { L"stellaris",               L"Stellaris",                         "strategy", L"" },
    { L"hoi4",                    L"Hearts of Iron IV",                 "strategy", L"" },
    { L"eu4",                     L"Europa Universalis IV",             "strategy", L"" },
    { L"ck3",                     L"Crusader Kings III",                "strategy", L"" },
    { L"xcom2",                   L"XCOM 2",                            "strategy", L"" },
    { L"frostpunk2",              L"Frostpunk 2",                       "strategy", L"" },
    { L"frostpunk",               L"Frostpunk",                         "strategy", L"" },
    { L"companyofheroes3",        L"Company of Heroes 3",               "strategy", L"" },

    // ---------------------------------------------------------------- roguelike / arcade
    { L"hades",                   L"Hades",                             "roguelike", L"hades2" },
    { L"hadesii",                 L"Hades II",                          "roguelike", L"" },
    { L"deadcells",               L"Dead Cells",                        "roguelike", L"" },
    { L"risk of rain 2",          L"Risk of Rain 2",                    "roguelike", L"riskofrain2" },
    { L"vampiresurvivors",        L"Vampire Survivors",                 "roguelike", L"" },
    { L"balatro",                 L"Balatro",                           "roguelike", L"" },
    { L"slaythespire",            L"Slay the Spire",                    "roguelike", L"" },
    { L"enterthegungeon",         L"Enter the Gungeon",                 "roguelike", L"" },
    { L"isaac-ng",                L"The Binding of Isaac: Rebirth",     "roguelike", L"bindingofisaac" },
    { L"gunfire reborn",          L"Gunfire Reborn",                    "roguelike", L"" },
    { L"backpackhero",            L"Backpack Hero",                     "roguelike", L"" },
    { L"brotato",                 L"Brotato",                           "roguelike", L"" },
};

inline constexpr size_t kCount = sizeof(kGames) / sizeof(kGames[0]);

// lowercase alphanumeric only: "VALORANT-Win64-Shipping.exe" -> "valorantwin64shipping"
inline wstring normalize(const wstring& in) {
    wstring out;
    out.reserve(in.size());
    for (wchar_t c : in) {
        wchar_t l = (wchar_t)::towlower(c);
        if ((l >= L'a' && l <= L'z') || (l >= L'0' && l <= L'9')) out += l;
    }
    return out;
}

// Split a '|'-separated alias field.
inline vector<wstring> aliases(const Entry& e) {
    vector<wstring> out;
    wstring a = e.alias ? e.alias : L"";
    size_t start = 0;
    while (start <= a.size()) {
        size_t p = a.find(L'|', start);
        wstring tok = a.substr(start, p == wstring::npos ? wstring::npos : p - start);
        if (!tok.empty()) out.push_back(tok);
        if (p == wstring::npos) break;
        start = p + 1;
    }
    return out;
}

// Exact match only: the exe stem, the display name or one of the aliases.
// Used for arbitrary folder names found by the drive survey, where a loose match
// would turn "Steam" or "Games" into a wrong game.
inline const Entry* lookupStrict(const wstring& raw) {
    wstring hay = normalize(raw);
    if (hay.size() < 3) return nullptr;
    for (const auto& e : kGames) {
        if (hay == normalize(e.exe) || hay == normalize(e.name)) return &e;
        for (const auto& a : aliases(e)) if (hay == normalize(a)) return &e;
    }
    return nullptr;
}

// Match an exe stem, an install folder name or a launcher display name against the table.
// Exact hits win; otherwise the longest substring hit is used, which is what catches
// "VALORANT-Win64-Shipping" or "Cyberpunk 2077 v2.21". Both sides must be at least 6
// characters so a common short word inside a longer alias cannot drag in a wrong title.
inline const Entry* lookup(const wstring& raw) {
    wstring hay = normalize(raw);
    if (hay.size() < 3) return nullptr;
    const Entry* best = nullptr;
    size_t bestLen = 0;
    for (const auto& e : kGames) {
        wstring nx = normalize(e.exe), nn = normalize(e.name);
        if (hay == nx || hay == nn) return &e;
        for (const auto& a : aliases(e)) {
            wstring na = normalize(a);
            if (na.empty()) continue;
            if (hay == na) return &e;
            if (na.size() >= 6 && hay.size() >= 6 &&
                (hay.find(na) != wstring::npos || na.find(hay) != wstring::npos)) {
                if (na.size() > bestLen) { best = &e; bestLen = na.size(); }
            }
        }
        for (const wstring& cand : { nx, nn }) {
            if (cand.size() >= 6 && hay.size() >= 6 &&
                (hay.find(cand) != wstring::npos || cand.find(hay) != wstring::npos)) {
                if (cand.size() > bestLen) { best = &e; bestLen = cand.size(); }
            }
        }
    }
    return best;
}

inline const Entry* lookup(const string& raw) { return lookup(widen(raw)); }

} // namespace ok::gamedb
