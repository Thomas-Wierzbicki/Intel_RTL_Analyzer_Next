# Intel RTL Analyzer Next

Weiterentwicklung von Thomas-Wierzbicki/Intel_RTL_analyzer,
Branch feature/mouse-wheel-zoom, Ausgangscommit
`b655ccb91fc57e75395fec702495819852549487`.
Die Quelldateien und vorhandenen Bibliotheken wurden kopiert; das Ursprungsrepository wird nicht veraendert.
Die historischen Ordner V1 bis V4_O(1) bleiben als unveraenderte Referenzen erhalten.
Aktive Anwendung: **Intel_RTL_analyzer/**.

## Neue Funktionen

- Alle vollstaendigen I/Q-Bloecke werden weiterhin verarbeitet.
- Wasserfall: Maximum pro Frequenzbin ueber alle FFTs seit dem letzten Anzeigeabruf;
  kurze Signale verschwinden dadurch nicht allein wegen des 45-ms-Anzeigeintervalls.
- Maximumsbildung je sichtbarer Wasserfallspalte erhaelt schmale Peaks auch beim Verkleinern.
  Im Zoom stehen die gespeicherten Originalbins zur Verfuegung.
- Der Cursor zeigt den staerksten tatsaechlichen Bin der dargestellten Wasserfallspalte.
- Gemeinsame Pause: Spektrum und Wasserfall frieren ein; Erfassung und Peak Hold laufen weiter.
  Zoom, Cursor und Pegelskala bleiben bedienbar. Beim Fortsetzen werden die waehrend der Pause
  angesammelten Wasserfallmaxima verworfen. Die Zeitachse zeigt die Luecke anhand der Zeitstempel.
- Mausrad zoomt an der Mausposition, Doppelklick zeigt den gesamten erfassten Bereich,
  Links/Rechts verschiebt um 10 Prozent der sichtbaren Breite.
- Mit **Zoom koppeln** steuern beide Ansichten denselben Ausschnitt. Ohne Kopplung navigieren
  sie unabhaengig. Beim erneuten Koppeln wird der zuletzt gemeinsam verwendete Ausschnitt uebernommen.
- **Cursor: Automatisch / Live / Peak Hold** bestimmt die Messkurve.
- **Marker A setzen / Marker B setzen** auswaehlen, danach einen Messpunkt anklicken.
  Marker speichern Frequenz und Pegel zum Klickzeitpunkt; B-A zeigt kHz und dB.
  Sie sind Momentaufnahmen, keine mitlaufenden Peak-Tracker.
- Untergrenze, Dynamikbereich und manueller Anzeigeoffset gelten fuer beide Ansichten.
  Historische Wasserfallwerte werden beim Aendern der Skala neu eingefaerbt.
- Empfangsstatus zeigt Simulator, fehlenden Stick, Lesefehler und USB-Pufferueberlaeufe.
- Asynchrones RTL-SDR-Lesen erlaubt gezieltes Abbrechen. Das Geraet wird erst nach dem Ende
  aller USB-Callbacks geschlossen. Ein defekter Treiber kann weiterhin Hardwaretests erfordern.

## Messwerte richtig verstehen

Die FFT verwendet ein Hann-Fenster und korrigiert dessen kohaerenten Gewinn.
Ein komplexer, binzentrierter Ton mit Betrag 1 entspricht 0 dBFS.
Die Anzeige ist **Tonpegel pro FFT-Bin**, keine Leistungsdichte in dBFS/Hz und keine dBm-Messung.
Der Binabstand betraegt bei 2,4 MS/s und 4096 Punkten 585,9375 Hz.
Die tatsaechliche Trennschaerfe wird zusaetzlich vom Hann-Fenster bestimmt.
Zoom fuegt keine Frequenzaufloesung hinzu.

Ein manueller Offset dient einer bekannten Referenzkorrektur und wird als
"dBFS + Offset" gekennzeichnet. Bei aktivem Tuner-AGC ist eine feste Umrechnung
in absolute Eingangsleistung nicht verlaesslich.

Wasserfallzeiten sind PC-Zeitstempel der FFT-Veroeffentlichung, keine
hardwaregenauen Empfangszeitstempel. Mehrere FFTs eines USB-Blocks koennen im selben
Anzeigeintervall liegen. Jede Zeile ist dessen Maximum, kein Mittelwert.
Die Altersachse bezieht sich auf die neueste gespeicherte Zeile und bleibt bei Pause stehen.
Aenderungen der PC-Uhr beeinflussen diese Zeitangaben.

## Visual Studio: empfohlener CMake-Weg

Dieser Weg benoetigt **keine QtMsBuild-Dateien**.

1. Neues Repository klonen: https://github.com/Thomas-Wierzbicki/Intel_RTL_Analyzer_Next.git
2. Visual Studio mit C++-Desktopentwicklung und CMake-Unterstuetzung verwenden.
3. Qt 6 fuer MSVC x64 sowie Intel IPP installieren.
4. `CMakeUserPresets.example.json` nach `CMakeUserPresets.json` kopieren.
5. Darin **CMAKE_PREFIX_PATH** auf den vorhandenen Qt-Ordner setzen
   (dieser enthaelt `bin`, `include` und `lib`).
6. **IPP_ROOT** auf den Intel-Ordner setzen, der `include/ipp/ipps.h` oder
   `include/ipps.h` sowie `lib/ipps.lib` enthaelt.
   Beispiel fuer einen passenden vereinheitlichten Installationsordner:
   `C:/Program Files (x86)/Intel/oneAPI/2026.1`.
7. In Visual Studio **Datei > Oeffnen > Ordner** und den Repository-Hauptordner auswaehlen.
8. Konfiguration **windows-release-local** auswaehlen, CMake-Konfiguration abwarten.
9. Ziel **Intel_RTL_Analyzer_Next.exe** erstellen und starten.

Qt- und RTL-SDR-Laufzeitdateien werden nach dem Build kopiert, sofern windeployqt
in der Qt-Installation gefunden wird. Dynamisch gebundene IPP-DLLs muessen im PATH
oder neben der EXE liegen; alternativ statische IPP-Bibliotheken verwenden.
Die bereits mitgelieferten RTL-SDR-Binaerdateien wurden aus dem Ausgangsrepository uebernommen.

Kommandozeile in einer x64-Developer-Konsole:
```
cmake --preset windows-release-local
cmake --build --preset windows-release-local
ctest --preset windows-release-local
```

Alternativ bleibt `Intel_RTL_analyzer.slnx` verfuegbar.
Dieser klassische Projektweg benoetigt passende Qt VS Tools, eine registrierte
Qt-Installation und `IppRoot` bzw. die Umgebungsvariable `IPPROOT`.
Debug und Release verwenden dieselben Includes, Bibliotheken und C++17-Einstellungen.

## Tests und Grenzen der Pruefung

Kernlogik ohne Qt, Stick oder IPP:
```
cmake -S . -B build-tests -DBUILD_ANALYZER=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
```

GitHub Actions fuehrt die C++-Regressionstests auf Windows und Linux aus.
Ein zusaetzlicher Qt-Compile-Check uebersetzt die GUI einschliesslich moc/uic.
Dabei wird ausschliesslich fuer dieses Objektziel ein als solcher gekennzeichneter,
deklarationsbasierter IPP-Testheader verwendet. Er ersetzt keinen echten IPP-Linktest.
Der produktive Anwendungstarget verwendet immer die echte IPP-Installation.
Der Actions-Status ist massgeblich; ein angelegter Workflow allein bedeutet keinen bestandenen Test.

Lokal zu pruefen:
- Echter Windows-Release-Build mit Qt und IPP, Start mit und ohne Stick.
- Kurze HF-Aussendung zwischen zwei Anzeigeaktualisierungen.
- Schmaler Peak bei voller Breite und starkem Zoom; Cursorfrequenz kontrollieren.
- Pause/Fortsetzen, Marker A/B und gekoppeltes Verschieben in beiden Fenstern.
- Stick waehrend des Betriebs abziehen, Simulator umschalten, Programm schliessen.
- Pegel mit bekanntem Referenzsignal vergleichen; Offset und AGC beachten.
