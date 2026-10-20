# Pfeiltasten und Messwert-Cursor

In beiden Fenstern mit dem Mausrad hineinzoomen, ins Diagramm klicken und
mit Pfeil links/rechts den sichtbaren Frequenzbereich verschieben. Ein Schritt
entspricht 10 Prozent der sichtbaren Breite. Die Empfangsbandgrenzen werden
nicht ueberschritten. Doppelklick zeigt wieder das volle Band.

Im Linienfenster rastet das Fadenkreuz am naechsten dargestellten Messpunkt
ein (Bildschirmabstand). Die Anzeige nennt Frequenz, Pegel und Live/Peak Hold.
Live ist der geglaettete Anzeigewert. Bei ausgeschaltetem Peak Hold werden
nur Live-Punkte ausgewaehlt. Mausbewegungen aktualisieren ein kleines Overlay,
ohne einen QCustomPlot-Replot anzufordern.

Im Wasserfall wird der gespeicherte FFT-Pegel der Zelle unter der Maus gelesen,
nicht aus der Farbe zurueckgerechnet. Die Frequenz gehoert zum FFT-Bin.
Die Historie laeuft weiter; der Cursor haelt keine Messung an. Noch unbefuellte
Zeilen zeigen "keine Messung". Alle Pegel sind relative dB, keine kalibrierten dBm.

## Automatischer Test

Vom Repository-Hauptverzeichnis:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g tests/cursorreadout_test.cpp -o /tmp/cursorreadout_test
ASAN_OPTIONS=detect_leaks=0 /tmp/cursorreadout_test
```

MSVC Developer Command Prompt:

```bat
cl /std:c++17 /EHsc /W4 tests\cursorreadout_test.cpp /Fe:cursorreadout_test.exe
cursorreadout_test.exe
```

Geprueft: Verschiebung und Bandgrenzen, unveraenderte Zoom-Breite,
Live-/Peak-Auswahl, Wasserfall-Zuordnung nach 950 Ringzeilen, leere Zeilen,
gezoomte Frequenzzuordnung und Randzellen.

## Noch lokal in Visual Studio pruefen

Projekt neu erstellen und starten. In beiden Fenstern Zoom, Fokus durch Klick,
Pfeiltasten an beiden Bandgrenzen und Doppelklick testen. Im Linienfenster
Live/Peak Hold mit der Maus auswaehlen und Peak Hold ausschalten/zuruecksetzen.
Im Wasserfall auch nach mehr als 300 Zeilen Messwerte pruefen. Fenster skalieren
und kontrollieren, dass das Fadenkreuz korrekt folgt und keine Spuren hinterlaesst.
Der vollstaendige Windows-/Qt-/IPP-Build wurde hier nicht ausgefuehrt.
