# Legacy Randomizer – Plan

Ein Randomizer (Randomizer Anywhere / RMC) als Mod direkt im Spiel, für TrackMania Nations ESWC, Sunrise und Original. Er lädt zufällige TMX-Maps, erkennt Ziel und Medaillen, speichert Replays und zeigt eine eigene Oberfläche über dem Spiel. Er wird eine Mod für den Horizon Legacy Loader (Projekt TM Horizon). Freigabe von BigBang am 2026-11-11 („go for it“). Den Code schreiben wir neu in C++, `randomizer-tmf` dient nur als Vorbild für den Ablauf (GPLv3, daher nichts übernehmen).

Stand 2026-11-11. Status: noch nichts gebaut.

## Was schon da ist

- **eswc-autosave** (`C:\Users\osaro\Downloads\eswc-autosave`, MIT): Prüfung der Exe, Ziel-Hook, Replay speichern mit der Speicherfunktion des Spiels, WinINet mit Online-Fix, Log, Loader-Muster (4-KB-`dinput8.dll` lädt die eigentliche DLL).
- **Ghidra-Projekt für ESWC**: liegt nur im Temp-Ordner einer alten Sitzung (`%LOCALAPPDATA%\Temp\claude\…\3af10730-…\scratchpad\re-eswc`, mit `notes.txt` und Skripten). **Als Erstes nach `C:\Users\osaro\tools\tm-re\eswc\` umziehen**, sonst ist es beim nächsten Temp-Aufräumen weg. Bleibt privat, kommt in kein öffentliches Repo.
- **Ghidra** 12.1.2 unter `C:\Users\osaro\tools\`. Ohne Oberfläche per `analyzeHeadless` und eigenen Skripten, so wie bei ESWC.
- **TMX**: `nations.tm-exchange.com` hat dieselbe API wie tmnf.exchange (unsere Randomizer-Presets nutzen sie schon). Ob `original.` und `sunrise.tm-exchange.com` gleich funktionieren, wird in Schritt 0 geprüft.

## Schritte

### 0. Vorbereitung (Agent ca. 30 Min, du: Spiele installieren)
- Ghidra-Projekt ESWC umziehen (siehe oben).
- Neues privates Repo `legacy-randomizer`, Aufbau wie eswc-autosave (`build.cmd`, MSVC x86, `/W4 /WX`).
- TMX-API für Original und Sunrise prüfen: zufällige Map, Filter, Download.
- **Du:** TM Original und TM Sunrise installieren. Vorher klären, welche Version die Szene spielt (Sunrise oder Sunrise eXtreme, welcher Patch, Original oder Original Unlimiter?). Der Mod wird wie beim Autosave fest an genau eine Exe gebunden (SHA-256).

### 1. Machbarkeitsprobe ESWC: Map laden (Agent 1–2 Std, du: 1–2 Tests)
- Im Spielcode die Funktion finden, mit der das Spiel eine Map im Solo startet (Weg über „Play Track“ im Menü, also Map-Datei → Rennen).
- Testversion: F8 lädt eine fest eingestellte, schon heruntergeladene Map.
- **Abbruchkriterium:** Lässt sich keine Map sauber aus dem Menü und aus einem laufenden Rennen starten, ist der Ansatz tot, bevor viel Arbeit drinsteckt. Ausweg wäre dann, die Menüsteuerung zu simulieren (langsamer, wackliger).

### 2. ESWC spielbar (Agent 1–2 Tage, du: ca. 10 Tests)
- **Ablauf wie RMC**: Timer (z. B. 60 Min), Map von TMX holen und starten, Author-/Gold-Medaille erkennen (Zeit beim Ziel gegen die Medaillenzeiten der Map), dann nächste Map. Skip (mit RMC-Regeln: freier Skip bzw. Gold-Skip), Zähler AT/Gold.
- **Map holen**: im Hintergrund vorladen, damit zwischen zwei Maps keine Wartezeit entsteht. Filter wie bei Randomizer Anywhere (Länge, Stil, keine Unlimiter-Maps, Skip-Liste).
- **Oberfläche**: eigenes Fenster über dem Spielbild (Dear ImGui über einen Direct3D-Hook): Timer, Map-Name und Autor, Medaillen, Knöpfe für Start, Skip und Einstellungen. Taste zum Ein- und Ausblenden.
- **Replays**: Autosave-Code übernehmen. Solo-Replays sind von sich aus gültig, also direkt TMX-tauglich.
- **Ergebnis**: Am Ende eine Zusammenfassung der Runde (Maps, Medaillen, Zeiten) als Datei.

### 3. Sunrise (Agent ca. 1 Tag, du: ca. 5 Tests)
Nations baut auf der Sunrise-Engine auf. Die Funktionen (Ziel, Speichern, Map laden) sollten sich per Vergleich in Ghidra wiederfinden lassen. Oberfläche und TMX-Teil bleiben gleich, neu sind nur die Adressen pro Exe.

### 4. Original (Agent 1–2 Tage, du: ca. 5 Tests)
Ältere Engine und eventuell eine andere Grafik-Schnittstelle (DirectX 8?). Am meisten Neuarbeit, daher zuletzt.

### 5. Legacy Loader
Sobald der Loader steht: Die Randomizer-DLL wird eine normale Mod in dessen `mods\`-Ordner. Bis dahin lädt sie das Loader-Muster aus eswc-autosave. Autosave und Randomizer dürfen sich nicht doppelt ins Ziel hängen: Der Randomizer nutzt den Autosave-Code mit, oder beide teilen sich einen gemeinsamen Ziel-Hook über den Loader.

## Engpässe und Risiken

- **Testen nur bei dir**: Ich kann das Spiel nicht selbst bedienen. Jede Runde Testen kostet deine Zeit, das bremst mehr als das Bauen.
- **Map laden** ist die einzige echte Unbekannte (Schritt 1).
- **Eine Exe pro Spiel**: Andere Versionen laufen nicht, der Mod schaltet sich dann ab und schreibt das ins Log.
- **TMX**: Vor der Veröffentlichung bei TMX nachfragen, ob ein öffentlicher Mod ihre API so nutzen darf (zufällige Maps, Downloads). Bei TM Horizon gilt dieselbe Regel.
- **Nichts an der Exe ändern**, nur Hooks im Speicher. Keine Änderungen an Physik, Prüfsumme oder Validierung.
