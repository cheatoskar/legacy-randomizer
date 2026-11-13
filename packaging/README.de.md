# Legacy Randomizer {VERSION}

Eine Random Map Challenge (RMC) für alte TrackMania-Spiele, direkt im Spiel. Du drückst Start, die Mod lädt eine zufällige Strecke von TM Exchange, du versuchst die Ziel-Medaille zu holen, dann kommt die nächste Strecke. Es ist ein Plugin für den Horizon Client.

## Unterstützte Spiele

- TrackMania Nations ESWC, inoffizielle 1.80-Patch-Exe
- TrackMania Sunrise, Win10-Repack (Ziel-Erkennung ab Horizon Client 0.3.1)
- TrackMania Original, Exe des Online Patch v2: neu und weniger getestet. Erst das Original-Zip des Horizon Client installieren, dann das Zip "plugin".

Nur Solo. Online läuft nichts.

## Installieren

Das Zip "with Horizon Client" enthält den Horizon Client und das Plugin:

1. In den Spielordner entpacken (neben TmNationsESWC.exe bzw. TmSunrise.exe).
2. Horizon-Install.cmd doppelklicken. Eine vorhandene dinput8.dll einer anderen Mod wird zu dinput8.chain.dll und läuft weiter.
3. Spiel starten und Einfg (Insert) drücken.

Wenn du den Horizon Client schon hast, nimm stattdessen das "plugin"-Zip: in den Spielordner entpacken, die Dateien landen in horizon\plugins\legacy-randomizer\.

## Tasten

- Einfg (Insert): Menü öffnen (Run starten, Filter, Regeln, HUD-Layout, Verlauf)
- Strg+F9: Strecke überspringen
- Strg+F10: Gold-Skip (nur mit Ziel Author)
- Strg+F11: HUD ein-/ausblenden

## Modi und Regeln

- Challenge (RMC): so viele Ziel-Medaillen wie möglich, bevor die Zeit abläuft.
- Survival (RMS): Medaillen geben Zeit, Skips kosten Zeit, bei 0 ist der Run vorbei.
- Free play: ohne Zeitlimit.
- Ziel: Author, Gold, Silber, Bronze oder Finished. Eine bessere Medaille zählt auch.
- Filter: Umgebung, Schwierigkeit, Länge, Stimmung, Routen, Name, Autor, Upload-Datum und mehr.
- Strecken aus früheren Runs kommen nicht nochmal (abschaltbar).

## Entfernen

Den Ordner horizon\plugins\legacy-randomizer löschen. Den ganzen Horizon Client entfernst du mit Horizon-Uninstall.cmd, danach den Ordner horizon löschen.

## Bitte schonend mit TMX umgehen

Die Mod fragt die TMX-API immer nur nach einer Strecke auf einmal. Bitte keine Dauerschleifen mit vielen Runs starten und die Mod nicht zum Massen-Download von Strecken benutzen.

## Danksagung

- Flink: die Idee der Random Map Challenge
- BigBang1112: RandomizerTMF (https://github.com/BigBang1112/randomizer-tmf) ist das Vorbild für Regeln und Filter; kein Code daraus übernommen
- TM Exchange (tm-exchange.com): die Strecken, Screenshots und die API
- Dear ImGui von Omar Cornut (MIT), siehe THIRD-PARTY.txt
- Legacy Randomizer steht unter der MIT-Lizenz, siehe LICENSE.txt
