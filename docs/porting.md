# PSP-Anpassungen

Basis: offizielles Extreme Tux Racer 0.8.4, C++-PC-Version. Spielzustände, Physik, Streckenformat, Figuren und Ereignisse stammen aus dieser Version.

## Plattform

`ports/extremetuxracer/psp/platform.cpp` implementiert den vom Spiel benötigten Teil der SFML-Schnittstelle mit PSPGL/EGL, PSP-Controllern und SDL-Audio/Bild-/Schriftbibliotheken. Das ist keine allgemeine SFML-Portierung. Der framebuffer hat 480 × 272 Pixel; die UI nutzt einen logischen Koordinatenraum von 854 × 480.

Kontextabhängige Tastenbelegungen bleiben bis zum Loslassen an den ursprünglichen Spielzustand gebunden. Dadurch löst gehaltenes Start beim Pausieren keinen zweiten Druck zum Fortsetzen aus.

Der PSP-Takt wird auf 333/333/166 MHz gesetzt. Die Präsentation erfolgt mit VSync. PSPGL benötigt eine explizite globale Ambient-Alpha-Komponente von 1: der Bibliotheksstartwert 0 macht beleuchtete Geometrie transparent. GUI-Matrizen werden vor dem Bufferwechsel beendet; vollständige PC-Attributstapel werden nicht über einen EGL-Swap getragen.

## Darstellung und Leistung

- Single-Precision für den numerischen Spielkern, passend zur PSP-FPU.
- Terrain mit expliziten UVs und nativen PSP-Vertexlayouts.
- Zwei Dreiecksfächer statt eines Mehrfach-`GL_QUADS`-Aufrufs für gekreuzte Baumflächen.
- Wiederverwendete Kugelmeshes für Figuren, ohne GLU-Quadrics.
- Höchstens 256 Schneespray-Partikel, gemeinsam in einem Vertex-Batch.
- Texturen bis 256 × 256, Vorschauen bis 128 × 128; Glyphenatlas 512 × 512. RGB565 für opake Texturen, RGBA4444 für Transparenz. Mindestens 8 × 8 verhindert unvollständige PSP-Swizzle-Blöcke.
- Ganze Skybox, damit beim Lenken keine offenen Seiten entstehen.
- PSP-Staging begrenzt Höhen-/Terrainkarten auf höchstens etwa 16.000 Punkte und erhält dabei die Streckenabmessungen. Quelldaten bleiben unverändert.
- Musik als 22.050-Hz-Mono-PCM; SDL mischt Musik und Effekte auf Stereoausgabe.

Der Gleichungslöser prüft zusätzlich den letzten Pivot, bevor die Rückwärtssubstitution durch null teilen kann. Der numerische Test kompiliert die tatsächlichen Spielquellen und prüft ODE-Integration gegen eine analytische Lösung, singuläre Systeme, Ebenenschnitte, Transformationen und Quaternionen.

## Bisheriger Umfang

Getestet werden PPSSPP, Menüs, Training, Rennen, Lenkung, Reset/Pause, Musik und Ergebnisse. Keine Messung auf physischer PSP-Hardware. Für neue Spieler wird ein fortlaufender Name vorgeschlagen; eine PSP-Bildschirmtastatur ist noch nicht eingebunden. Die Schriftanbindung deckt den Latin-1-Zeichensatz ab. Screenshots werden über PPSSPP aufgenommen.

Die alte Tux-Racer-0.61/Dreamcast-basierte Versuchsversion bleibt lokal als Vergleich erhalten, gehört aber weder zum Quellpaket noch zum Build dieses Repositories.
