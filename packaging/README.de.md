# Legacy Randomizer {VERSION}

Eine Random Map Challenge (RMC) für alte TrackMania-Spiele, direkt im Spiel. Du drückst Start, die Mod lädt eine zufällige Strecke von TM Exchange, du versuchst die Ziel-Medaille zu holen, dann kommt die nächste Strecke. Es ist ein Plugin für den Horizon Host.

## Unterstützte Spiele

- TrackMania Nations ESWC, inoffizielle 1.80-Patch-Exe
- TrackMania Sunrise, Win10-Repack. Sunrise: Ziel-Erkennung gibt es noch nicht. Strecken musst du von Hand überspringen.

Nur Solo. Online läuft nichts.

## Installieren

Das Zip "with Horizon Host" enthält den Host und das Plugin:

1. In den Spielordner entpacken (neben TmNationsESWC.exe bzw. TmSunrise.exe).
2. Horizon-Install.cmd doppelklicken. Eine vorhandene dinput8.dll einer anderen Mod wird zu dinput8.chain.dll und läuft weiter.
3. Spiel starten und Einfg (Insert) drücken.

Wenn du den Horizon Host schon hast, nimm stattdessen das "plugin"-Zip: in den Spielordner entpacken, die Dateien landen in horizon\plugins\legacy-randomizer\.

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

Den Ordner horizon\plugins\legacy-randomizer löschen. Den ganzen Host entfernst du mit Horizon-Uninstall.cmd, danach den Ordner horizon löschen.

## Bitte schonend mit TMX umgehen

Die Mod fragt die TMX-API immer nur nach einer Strecke auf einmal. Bitte keine Dauerschleifen mit vielen Runs starten und die Mod nicht zum Massen-Download von Strecken benutzen.

## Dank

- Strecken und Screenshots: TM Exchange (tm-exchange.com)
- Die RMC-Idee: Flink
- Regeln nach dem Vorbild von BigBang1112s RandomizerTMF (kein Code daraus übernommen)
- Dear ImGui von Omar Cornut (MIT), siehe THIRD-PARTY.txt
- Legacy Randomizer steht unter der MIT-Lizenz, siehe LICENSE.txt
