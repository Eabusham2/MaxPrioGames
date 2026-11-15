# Game catalog — November 2026

Reviewed **15 November 2026**. The compiled catalog contains **110 games and 10 game-service families**.

## Selection

The baseline is [Valve's live Most Played chart](https://store.steampowered.com/charts/mostplayed), which ranks Steam apps by concurrent players. Each of its 100 entries was checked against its linked Steam store page. The captured app IDs and chart positions are in [the source snapshot](../catalog/steam-chart-2026-11-15.json).

Four chart entries are non-game utilities: Wallpaper Engine, Crosshair X, OBS Studio, and Soundpad. They are excluded from automatic prioritization. The remaining 96 games are supplemented by 14 major PC games or editions distributed outside Steam. Those additions are coverage choices; they have no invented rank or player-count estimate. There is no single comparable public top-100 ranking across all PC launchers.

## Detection

Steam entries use their actual app ID and installed manifest location, so a title can be supported without guessing its executable filename. Known executable aliases provide additional coverage for other stores. Generic filenames such as javaw.exe, Agent.exe, Discovery.exe, and ShooterGame.exe require a game-specific path hint. They are never global wildcards. Windows paths are matched without regard to case.

An empty alias cell means detection uses the Steam installation. It does not imply that an unverified executable name was fabricated. Alias compatibility can change with game updates; these are detection rules, not a claim that every game was installed and play-tested on CI. Add a missing game from its open window or browse to its EXE.

Steam and publisher links below establish the game's identity. The catalog's executable mappings are maintained implementation data, not a claim that each linked product page documents its filenames. Helpful publisher filename references include [Fortnite](https://www.epicgames.com/help/c-34254770/c-33726977/a10628872?lang=en-US), [League/Riot](https://support-leagueoflegends.riotgames.com/hc/en-us/articles/43435443937299), [World of Warcraft](https://us.forums.blizzard.com/en/wow/t/wow-exe-missing/1698586/6), and [BattlEye](https://www.battleye.com/support/faq/).

## Games

| Game | Steam app ID | Known EXE aliases |
| --- | --- | --- |
| [Counter-Strike 2](https://store.steampowered.com/app/730/) | 730 | `cs2.exe` |
| [Dota 2](https://store.steampowered.com/app/570/) | 570 | `dota2.exe` |
| [ROB RUN](https://store.steampowered.com/app/4262460/) | 4262460 | Installed Steam game |
| [Deadlock](https://store.steampowered.com/app/1422450/) | 1422450 | `deadlock.exe` |
| [Bongo Cat](https://store.steampowered.com/app/3419430/) | 3419430 | Installed Steam game |
| [PUBG: BATTLEGROUNDS](https://store.steampowered.com/app/578080/) | 578080 | `TslGame.exe` |
| [Call of Duty®](https://store.steampowered.com/app/1938090/) | 1938090 | `cod.exe` |
| [AION 2](https://store.steampowered.com/app/3393110/) | 3393110 | Installed Steam game |
| [WARDOGS](https://store.steampowered.com/app/1867240/) | 1867240 | Installed Steam game |
| [Path of Exile](https://store.steampowered.com/app/238960/) | 238960 | `PathOfExile.exe`, `PathOfExile_x64.exe`, `PathOfExileSteam.exe`, `PathOfExile_x64Steam.exe` |
| [Cyberpunk 2077](https://store.steampowered.com/app/1091500/) | 1091500 | `Cyberpunk2077.exe` |
| [Rust](https://store.steampowered.com/app/252490/) | 252490 | `RustClient.exe` |
| [Team Fortress 2](https://store.steampowered.com/app/440/) | 440 | `tf_win64.exe` |
| [Slay the Spire 2](https://store.steampowered.com/app/2868840/) | 2868840 | Installed Steam game |
| [Tom Clancy's Rainbow Six Siege](https://store.steampowered.com/app/359550/) | 359550 | `RainbowSix.exe`, `RainbowSix_Vulkan.exe`, `RainbowSix_DX11.exe` |
| [Euro Truck Simulator 2](https://store.steampowered.com/app/227300/) | 227300 | `eurotrucks2.exe` |
| [Football Manager 27](https://store.steampowered.com/app/4530370/) | 4530370 | Installed Steam game |
| [Apex Legends™](https://store.steampowered.com/app/1172470/) | 1172470 | `r5apex.exe`, `r5apex_dx12.exe` |
| [Valheim](https://store.steampowered.com/app/892970/) | 892970 | `valheim.exe` |
| [Marvel Rivals](https://store.steampowered.com/app/2767030/) | 2767030 | `MarvelRivals-Win64-Shipping.exe` |
| [War Thunder](https://store.steampowered.com/app/236390/) | 236390 | `aces.exe` |
| [Enshrouded](https://store.steampowered.com/app/1203620/) | 1203620 | `enshrouded.exe` |
| [ARC Raiders](https://store.steampowered.com/app/1808500/) | 1808500 | Installed Steam game |
| [Geometry Dash](https://store.steampowered.com/app/322170/) | 322170 | `GeometryDash.exe` |
| [Warframe](https://store.steampowered.com/app/230410/) | 230410 | `Warframe.x64.exe`, `Warframe.exe` |
| [Phantom Blade Zero](https://store.steampowered.com/app/4115450/) | 4115450 | Installed Steam game |
| [Grand Theft Auto V Enhanced](https://store.steampowered.com/app/3240220/) | 3240220 | `GTA5_Enhanced.exe` |
| [Grand Theft Auto V Legacy](https://store.steampowered.com/app/271590/) | 271590 | `GTA5.exe` |
| [Dead by Daylight](https://store.steampowered.com/app/381210/) | 381210 | `DeadByDaylight-Win64-Shipping.exe` |
| [Baldur's Gate 3](https://store.steampowered.com/app/1086940/) | 1086940 | `bg3.exe`, `bg3_dx11.exe` |
| [Project Zomboid](https://store.steampowered.com/app/108600/) | 108600 | `ProjectZomboid64.exe`, `ProjectZomboid32.exe` |
| [Overwatch®](https://store.steampowered.com/app/2357570/) | 2357570 | `Overwatch.exe` |
| [DayZ](https://store.steampowered.com/app/221100/) | 221100 | `DayZ_x64.exe` |
| [Stardew Valley](https://store.steampowered.com/app/413150/) | 413150 | `Stardew Valley.exe` |
| [Farming Simulator 25](https://store.steampowered.com/app/2300320/) | 2300320 | `FarmingSimulator2025Game.exe` |
| [TBH: Task Bar Hero](https://store.steampowered.com/app/3678970/) | 3678970 | Installed Steam game |
| [EA SPORTS FC™ 27](https://store.steampowered.com/app/4080220/) | 4080220 | Installed Steam game |
| [VRChat](https://store.steampowered.com/app/438100/) | 438100 | `VRChat.exe` |
| [The Elder Scrolls V: Skyrim Special Edition](https://store.steampowered.com/app/489830/) | 489830 | `SkyrimSE.exe` |
| [Probably Stolen - Cyberpunk Shopkeeper Sim](https://store.steampowered.com/app/4348910/) | 4348910 | Installed Steam game |
| [R.E.P.O.](https://store.steampowered.com/app/3241660/) | 3241660 | `REPO.exe` |
| [Hearts of Iron IV](https://store.steampowered.com/app/394360/) | 394360 | `hoi4.exe` |
| [Battlefield™ 6](https://store.steampowered.com/app/2807960/) | 2807960 | Installed Steam game |
| [Moon Sweepers](https://store.steampowered.com/app/5335510/) | 5335510 | Installed Steam game |
| [The Witcher 3: Wild Hunt — Remastered](https://store.steampowered.com/app/292030/) | 292030 | `witcher3.exe` |
| [The Binding of Isaac: Rebirth](https://store.steampowered.com/app/250900/) | 250900 | `isaac-ng.exe` |
| [Delta Force](https://store.steampowered.com/app/2507950/) | 2507950 | Installed Steam game |
| [Total War: WARHAMMER III](https://store.steampowered.com/app/1142710/) | 1142710 | `Warhammer3.exe` |
| [BeamNG.drive](https://store.steampowered.com/app/284160/) | 284160 | `BeamNG.drive.x64.exe` |
| [The Sims™ 4](https://store.steampowered.com/app/1222670/) | 1222670 | `TS4_x64.exe` |
| [Garry's Mod](https://store.steampowered.com/app/4000/) | 4000 | `gmod.exe` |
| [Red Dead Redemption 2](https://store.steampowered.com/app/1174180/) | 1174180 | `RDR2.exe` |
| [Aniimo](https://store.steampowered.com/app/4126040/) | 4126040 | Installed Steam game |
| [HELLDIVERS™ 2](https://store.steampowered.com/app/553850/) | 553850 | `helldivers2.exe` |
| [RimWorld](https://store.steampowered.com/app/294100/) | 294100 | `RimWorldWin64.exe` |
| [Desktop Wallet Heroes](https://store.steampowered.com/app/5052450/) | 5052450 | Installed Steam game |
| [Sid Meier’s Civilization® VI](https://store.steampowered.com/app/289070/) | 289070 | `CivilizationVI.exe`, `CivilizationVI_DX12.exe` |
| [Crimson Desert Enhanced](https://store.steampowered.com/app/3321460/) | 3321460 | Installed Steam game |
| [Rocket League®](https://store.steampowered.com/app/252950/) | 252950 | `RocketLeague.exe` |
| [Arena Breakout: Infinite](https://store.steampowered.com/app/2073620/) | 2073620 | Installed Steam game |
| [Palworld](https://store.steampowered.com/app/1623730/) | 1623730 | `Palworld-Win64-Shipping.exe` |
| [Left 4 Dead 2](https://store.steampowered.com/app/550/) | 550 | `left4dead2.exe` |
| [Bodycam](https://store.steampowered.com/app/2406770/) | 2406770 | Installed Steam game |
| [ARK: Survival Ascended](https://store.steampowered.com/app/2399830/) | 2399830 | `ArkAscended.exe` |
| [Terraria](https://store.steampowered.com/app/105600/) | 105600 | `Terraria.exe` |
| [Satisfactory](https://store.steampowered.com/app/526870/) | 526870 | `FactoryGame-Win64-Shipping.exe` |
| [Age of Empires II: Definitive Edition](https://store.steampowered.com/app/813780/) | 813780 | `AoE2DE_s.exe` |
| [7 Days to Die](https://store.steampowered.com/app/251570/) | 251570 | `7DaysToDie.exe` |
| [Mount & Blade II: Bannerlord](https://store.steampowered.com/app/261550/) | 261550 | `Bannerlord.exe` |
| [ELDEN RING](https://store.steampowered.com/app/1245620/) | 1245620 | `eldenring.exe` |
| [EA SPORTS FC™ 26](https://store.steampowered.com/app/3405690/) | 3405690 | Installed Steam game |
| [Limbus Company](https://store.steampowered.com/app/1973530/) | 1973530 | `LimbusCompany.exe` |
| [Destiny 2](https://store.steampowered.com/app/1085660/) | 1085660 | `destiny2.exe` |
| [Factorio](https://store.steampowered.com/app/427520/) | 427520 | `factorio.exe` |
| [DPS: Desktop Pension Squad](https://store.steampowered.com/app/5135440/) | 5135440 | Installed Steam game |
| [FINAL FANTASY XIV Online](https://store.steampowered.com/app/39210/) | 39210 | `ffxiv_dx11.exe`, `ffxiv.exe` |
| [MapleStory](https://store.steampowered.com/app/216150/) | 216150 | `MapleStory.exe` |
| [THE FINALS](https://store.steampowered.com/app/2073850/) | 2073850 | `Discovery.exe` |
| [The Isle](https://store.steampowered.com/app/376210/) | 376210 | `TheIsleClient-Win64-Shipping.exe` |
| [eFootball™](https://store.steampowered.com/app/1665460/) | 1665460 | `eFootball.exe` |
| [Crusader Kings III](https://store.steampowered.com/app/1158310/) | 1158310 | `ck3.exe` |
| [PEAK](https://store.steampowered.com/app/3527290/) | 3527290 | `PEAK.exe` |
| [Deskrawl: Idle ARPG](https://store.steampowered.com/app/4623570/) | 4623570 | Installed Steam game |
| [Once Human](https://store.steampowered.com/app/2139460/) | 2139460 | `OnceHuman.exe` |
| [Unturned](https://store.steampowered.com/app/304930/) | 304930 | `Unturned.exe` |
| [Sid Meier's Civilization® V](https://store.steampowered.com/app/8930/) | 8930 | `CivilizationV.exe`, `CivilizationV_DX11.exe` |
| [Fallout 4](https://store.steampowered.com/app/377160/) | 377160 | `Fallout4.exe` |
| [Escape from Tarkov](https://store.steampowered.com/app/3932890/) | 3932890 | `EscapeFromTarkov.exe` |
| [Hunt: Showdown 1896](https://store.steampowered.com/app/594650/) | 594650 | `HuntGame.exe` |
| [Path of Exile 2](https://store.steampowered.com/app/2694490/) | 2694490 | `PathOfExileSteam2.exe`, `PathOfExile2.exe` |
| [Cities: Skylines II](https://store.steampowered.com/app/949230/) | 949230 | `Cities2.exe` |
| [Don't Starve Together](https://store.steampowered.com/app/322330/) | 322330 | `dontstarve_steam.exe`, `dontstarve_steam_x64.exe` |
| [Black Desert](https://store.steampowered.com/app/582660/) | 582660 | `BlackDesert64.exe` |
| [tModLoader](https://store.steampowered.com/app/1281930/) | 1281930 | Installed Steam game |
| [Kingdom Come: Deliverance II](https://store.steampowered.com/app/1771300/) | 1771300 | `KingdomCome.exe` |
| [ARK: Survival Evolved](https://store.steampowered.com/app/346110/) | 346110 | `ShooterGame.exe` |
| [Fortnite](https://www.fortnite.com/) | Additional PC game | `FortniteClient-Win64-Shipping.exe`, `FortniteClient-Win64-Shipping_BE.exe`, `FortniteClient-Win64-Shipping_EAC.exe` |
| [VALORANT](https://playvalorant.com/) | Additional PC game | `VALORANT-Win64-Shipping.exe` |
| [League of Legends](https://www.leagueoflegends.com/) | Additional PC game | `League of Legends.exe` |
| [Minecraft: Java Edition](https://www.minecraft.net/en-us/store/minecraft-java-bedrock-edition-pc) | Additional PC game | `javaw.exe`, `java.exe` |
| [Minecraft: Bedrock Edition](https://www.minecraft.net/en-us/store/minecraft-java-bedrock-edition-pc) | Additional PC game | `Minecraft.Windows.exe` |
| [Roblox](https://www.roblox.com/download) | Additional PC game | `RobloxPlayerBeta.exe` |
| [World of Warcraft](https://worldofwarcraft.blizzard.com/) | Additional PC game | `Wow.exe`, `WowClassic.exe`, `WowClassicT.exe` |
| [Diablo IV](https://diablo4.blizzard.com/) | Additional PC game | `Diablo IV.exe` |
| [Hearthstone](https://hearthstone.blizzard.com/) | Additional PC game | `Hearthstone.exe` |
| [StarCraft II](https://starcraft2.blizzard.com/) | Additional PC game | `SC2_x64.exe`, `SC2.exe` |
| [Genshin Impact](https://genshin.hoyoverse.com/) | Additional PC game | `GenshinImpact.exe`, `YuanShen.exe` |
| [Honkai: Star Rail](https://hsr.hoyoverse.com/) | Additional PC game | `StarRail.exe` |
| [Zenless Zone Zero](https://zenless.hoyoverse.com/) | Additional PC game | `ZenlessZoneZero.exe` |
| [Wuthering Waves](https://wutheringwaves.kurogames.com/) | Additional PC game | `Wuthering Waves.exe`, `Client-Win64-Shipping.exe` |

## Game services

These are specific user-mode service processes, not kernel drivers. Access denied or an unsupported priority/affinity change is silently skipped. No service is stopped, patched, injected into, or reconfigured. Shared Windows hosts such as svchost.exe are excluded. The service checkbox can turn this entire category off.

| Service family | Exact EXE names | Required path hint(s) |
| --- | --- | --- |
| [Easy Anti-Cheat](https://www.easy.ac/support/articles) | `EasyAntiCheat.exe`, `EasyAntiCheat_EOS.exe` | `easyanticheat` |
| [BattlEye](https://www.battleye.com/support/faq/) | `BEService.exe`, `BEService_x64.exe` | `battleye` |
| [Riot Vanguard](https://support-leagueoflegends.riotgames.com/hc/en-us/articles/43435443937299) | `vgc.exe`, `vgm.exe` | `riot vanguard` |
| [Riot Client Services](https://support-leagueoflegends.riotgames.com/hc/en-us/articles/43435443937299) | `RiotClientServices.exe` | `riot client` |
| [EA Javelin anticheat](https://help.ea.com/en/articles/platforms/pc-ea-anticheat/) | `EAAntiCheat.GameService.exe` | `ea\ac`, `eaanticheat` |
| [EA Background Service](https://help.ea.com/en/ea-app/) | `EABackgroundService.exe` | `electronic arts\ea desktop` |
| [Steam Client Service](https://help.steampowered.com/) | `steamservice.exe` | `steam` |
| [Epic Online Services](https://dev.epicgames.com/docs/epic-online-services) | `EpicOnlineServices.exe` | `epic online services`, `epiconlineservices` |
| [Battle.net Agent](https://us.forums.blizzard.com/en/blizzard/t/game-crashes-exceptioninternalerror/59378) | `Agent.exe` | `battle.net\agent` |
| [Microsoft Gaming Services](https://support.xbox.com/help/games-apps/troubleshooting/gaming-services-repair-tool) | `GamingServices.exe`, `GamingServicesNet.exe` | `microsoft.gamingservices_` |

## Maintaining the catalog

Edit `catalog/games.json`, then run `python scripts/Generate-Catalog.py`. Commit the generated `src/catalog.cpp` and this inventory together. CI checks that the generated content is current. Add only verified game identities, avoid generic bare filenames, and preserve app-ID and path exclusions for non-game software.
