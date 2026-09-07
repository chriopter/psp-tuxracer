# Herkunft und Lizenzen

Dieser inoffizielle PSP-Port basiert auf **Extreme Tux Racer 0.8.4**, heruntergeladen aus dem offiziellen [SourceForge-Release](https://sourceforge.net/projects/extremetuxracer/files/releases/0.8.4/). Die PC-Spielquellen und Spieldaten liegen in `ports/extremetuxracer/`. Die PSP-Plattformschicht wurde für diesen Port geschrieben. Dreamcast-Code und Jim/Tcl gehören nicht zu diesem Build.

Die ursprünglichen Inhalte sind fremde Open-Source-Werke, nicht unser Eigentum. Ihre Lizenzen erlauben die hier vorgenommenen Änderungen und die Weitergabe unter den jeweiligen Bedingungen.

- Spiel und Spieldaten: **GPL-2.0-or-later**. Copyrightvermerke, `AUTHORS`, `COPYING`, Musikautoren und Streckenautoren bleiben enthalten.
- Quadtree: gesonderte erlaubende Lizenz von Ulrich Thatcher, unverändert im Quelltext enthalten.
- `install-sh`: X-Consortium/Expat-Lizenz, im Skript enthalten.
- AppStream-Metadaten: CC0, wie in der beigefügten Lizenzaufstellung ausgewiesen.
- Neue PSP-Dateien: GPL-2.0-or-later.

Die [Debian-Lizenzaufstellung](upstream-copyright.txt) stammt aus `extremetuxracer_0.8.4-2.debian.tar.xz`. Alle **466 Spieldateien** des offiziellen Releases wurden per SHA-256 mit dem Debian-Originalarchiv verglichen: **keine Abweichung**. Build-Makefiles waren von diesem Datenvergleich ausgenommen. Archivquellen, Prüfsummen und Prüfumfang stehen in [upstream.json](upstream.json).

Die PSP-Ausgabe verkleinert Höhen-/Terrainkarten und konvertiert Musik nach PCM/WAV. Die unveränderten Quelldaten bleiben im Repository; die Konvertierung ist in `tools/stage-extremetuxracer.py` nachvollziehbar. Screenshots zeigen das daraus gebaute Spiel.

Der Git-Verlauf enthält Quellen und Dokumentation. CI-Artefakte und Releases enthalten zusätzlich die gebaute Anwendung und ein zugehöriges Quellpaket. PSPDEV, PSPGL, SDL 1.2, SDL_image, SDL_ttf, SDL_mixer, FreeType und die Codec-Bibliotheken werden aus dem angegebenen SDK-Container eingebunden und behalten ihre eigenen Lizenzen. Die CI liefert die Lizenztexte und passenden Quellen der eingebundenen Bibliotheken gemeinsam mit dem Spielpaket aus. Paketrezepte, PSP-Patches und SDK-Laufzeitrevisionen sind enthalten und gegen die SDK-Buildmetadaten geprüft. Siehe [Binary-Verteilung](binary-distribution.md) und [festgeschriebene Abhängigkeiten](dependencies-lock.json).

Tux Racer / Extreme Tux Racer und die ursprünglichen Autoren werden zur Kennzeichnung der Herkunft genannt. Dieses Projekt ist kein offizielles Release des Extreme Tux Racer Teams.
