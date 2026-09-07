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

Getestet werden PPSSPP, Menüs, Training, Rennen, Lenkung, Reset/Pause, Musik und Ergebnisse. Keine Messung auf physischer PSP-Hardware. Neue Spielernamen werden über die native PSP-Bildschirmtastatur eingegeben (maximal 24 Zeichen). Die Taste zum Öffnen muss vor der Übergabe losgelassen sein, damit sie nicht zusätzlich ein Zeichen eingibt. Die Schriftanbindung deckt den Latin-1-Zeichensatz ab. Screenshots werden über PPSSPP aufgenommen.

Die alte Tux-Racer-0.61/Dreamcast-basierte Versuchsversion bleibt lokal als Vergleich erhalten, gehört aber weder zum Quellpaket noch zum Build dieses Repositories.

## Native Spielstände

`psp/savedata.cpp` verwendet `sceUtilitySavedata*` für automatische und interaktive Speicherung. Ein nativer Save (`ETRX00001PROFILE/PROFILE.DAT`) enthält Spieler, freigeschaltete Cups, Highscores und Einstellungen. Die PSP-Utility erzeugt Metadaten und übernimmt die native Dateiverarbeitung; das Spielpaket liefert keine persönlichen Spielstände mit.

Der innere Container hat einen 28-Byte-Header mit Formatversion, drei Längen und CRC32 über Header und Nutzdaten. Grenze: 256 KiB. Ungültige Daten werden vor dem Ersetzen lokaler Dateien abgewiesen. Lokale Dateien werden vollständig temporär geschrieben und mit Rücksicherung ersetzt; das native Save bleibt die maßgebliche Kopie. Ein fehlgeschlagenes Laden deaktiviert automatisches Überschreiben. Fehler beim Schreiben oder Schließen der Arbeitsdateien verhindern eine neue native Speicherung.

Der Heap lässt initial 4 MiB frei für PSP-Systemutilities über `PSP_HEAP_THRESHOLD_SIZE_KB(4096)`. Im gepinnten SDK bedeutet eine beliebige negative `PSP_HEAP_SIZE_KB` lediglich maximalen Heap; ohne explizite Schwelle bleiben standardmäßig nur 512 KiB übrig. Siehe die [gepinnten SDK-Quellen](https://github.com/pspdev/pspsdk/blob/09f02b88b9f30055bea916c5ddbdbddcdef9e31f/src/libcglue/glue.c#L696). Laden stellt auch den aktiven Spieler korrekt wieder her; wiederholtes Laden ersetzt Highscores, statt sie erneut anzuhängen. Charaktervorschauen werden nach dem Laden wieder aufgebaut. Die Controller-Navigation berücksichtigt die aktuell fokussierte Eingabe statt alle Eingaben eines Menüs gleichzeitig.

Normale Rennen schreiben keine periodischen Timing-Protokolle mehr auf den Memory Stick. Nur der explizite Benchmark-Modus erfasst Präsentationsintervalle, Arbeit vor dem Swap und Heap-/freien User-Speicher (alle 60 Frames). Speicherwerte sind Stichproben, keine nachgewiesenen absoluten Spitzen. In PPSSPP gemessene Arbeitszeiten sind kein Zyklusmodell der echten PSP-CPU/GPU.

Skybox-PNGs werden bereits beim Staging auf die vom Renderer ohnehin verwendeten maximal 256 × 256 Pixel reduziert. Dadurch entfallen temporäre volle 512 × 512-Dekodier- und RGBA-Kopien beim Streckenladen, was den kleineren PSP-1000-Heap mit echter Systemreserve entlastet.

Die Upstream-Umgebungen enthalten nur drei Skybox-Seiten. Für die vollständige PSP-Skybox ergänzt das Staging obere/untere Flächen aus den Randzeilen und eine gespiegelte Rückseite aus der vorhandenen Frontgrafik. Dadurch werden beim Umdrehen keine fehlenden/uninitialisierten Texturen mehr gebunden; die Rückseite ist eine wiederholte Kulisse, kein neues Panorama.

## Hardware-Startfehler / Musikdateien

Nach dem Hardwarebericht zu 6.60 ME-1.3 wird Musik erst beim tatsächlichen Abspielen mit SDL_mixer geöffnet. Die Liste der zehn Titel hält nur Dateinamen; vor einem Titelwechsel wird der bisherige Stream geschlossen. Inaktive Titel können den aktiven Stream nicht stoppen. Fehler und Freigabe werden im Hosttest mit einem Backend geprüft, das maximal einen Stream zulässt. Ein Dateihandle-Limit ist eine plausible Ursache des gemeldeten Ressourcenfehlers, aber noch nicht durch einen erneuten Hardwarelauf bestätigt. Ressourcen-Öffnungs-/Lesefehler protokollieren jetzt Pfad und errno; der Start protokolliert Firmware, Arbeitsverzeichnis und argv[0].

Zusätzliche Startkorrekturen: Fehlende Musikreferenzen werden sicher als null behandelt. Übersetzungen werden nur beim Eintritt in den Startbildschirm geladen; Fehler der Objektliste werden angezeigt. Mehrzeiliger Text berücksichtigt Zeilenumbrüche beim Zeichnen, bei den Abmessungen und bei der Cursorposition. Der ferne Nebelabstand wird vor dem nahen gesetzt, damit PSPGL beim ersten Aufbau keinen Nullabstand berechnet. Leere oder ungültige Benchmark-Dateien sperren normale Spielstände nicht mehr. Der reproduzierte Dateilimit-Test und die unabhängige Claude-Prüfung der öffentlichen Quellen stehen in der [Startvalidierung](startup-validation.md).

Die Schriftanbindung erhält die von SDL_ttf gelieferten Glyphenmetriken (`minx`, `ascent - maxy`). Die Glyphen-Bitmaps enthalten nur das jeweilige Zeichen, keine vollständige Textzeile; die bisherigen Null-Offsets versetzten insbesondere Satzzeichen und Unterlängen. Ein gepackter 512×512-Atlas ersetzt die starren, teils abschneidenden 32×32-Zellen. Transparente weiße Zwischenräume verhindern das Einblenden benachbarter Zeichen und dunkle Filterränder. Atlasgröße und 16-Bit-Texturformat bleiben gleich.
