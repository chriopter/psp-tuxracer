# Binary-Pakete und zugehörige Quellen

Die CI baut jeden Push auf `main`, Pull Requests und manuell ausgelöste Läufe. Das Artefakt enthält gemeinsam:

- `extremetuxracer-psp.zip`: EBOOT.PBP, aufbereitete Spieldaten, frische Konfiguration, Installationshinweise und Bibliothekslizenzen.
- `sources.tar.gz`: vollständige Spielquellen des gebauten Commits, Bibliotheksquellen, PSP-Patches/Buildrezepte, SDK-/Laufzeitquellen und Buildnachweise.
- `SHA256SUMS`: Prüfsummen beider Pakete.

Jeder erfolgreiche Push oder manuelle Build auf `main` veröffentlicht diese Dateien gemeinsam als nächstes GitHub-Release `v0.x.0` (z. B. `v0.3.0`, danach `v0.4.0`). Titel und Tag entsprechen der Version; der Tag verweist auf den tatsächlich gebauten Commit. Es gibt ausschließlich CI-Releases; ein manuell gepushter Versions-Tag startet keinen zusätzlichen Build. Pull Requests erzeugen nur Test-Artefakte. Laufende Builds werden bei weiteren Pushes nicht abgebrochen, damit die Veröffentlichung fertig werden kann. Normale CI-Artefakte bleiben 30 Tage verfügbar; Quell- und Spielpaket haben dieselbe Aufbewahrung. Beim Weitergeben beide Pakete zusammen behalten. Die Quellbeigabe enthält auch die Lizenztexte von PSPSDK, Newlib und GCC einschließlich dessen Runtime Library Exception.

## Release-Beschreibung und Reihenfolge

Für eine beschriebene Veröffentlichung `docs/release-notes.md` im selben Commit aktualisieren: fünf kurze Highlights, anschließend `✨ New`, `🐛 Fixed`, tatsächlich geprüfte Geräte und Grenzen sowie Downloads. Die CI verwendet diese Datei nur, wenn sie seit der vorherigen Veröffentlichung geändert wurde. Hardware-Freigaben und FPS gehören nur mit entsprechenden Messergebnissen hinein. Ohne neue Beschreibung erzeugt die CI eine kompakte Übersicht der Commit-Betreffzeilen seit dem letzten veröffentlichten Release, ohne mehrzeilige Commit-Texte. Bei einem frischen oder unabhängigen Verlauf wird nur der aktuelle Commit beschrieben, nicht der ursprüngliche Import.

Ein Release bleibt bis zum vollständigen Upload aller drei Dateien ein Entwurf. Wiederholungen desselben CI-Laufs setzen denselben Entwurf fort; Versionsnummern bereits vorhandener Entwürfe bleiben reserviert. Bestätigt aufgegebene, unvollständige Entwürfe können gezielt entfernt werden, veröffentlichte Versionen und Tags werden nicht umnummeriert. Erst nach vollständigem Upload wird die neue Version veröffentlicht und als Latest markiert. Die verborgene `ci-run`-Markierung in manuell bearbeiteten Release Notes erhalten, damit ein erneuter CI-Lauf keine doppelte Veröffentlichung erzeugt.

## Herkunft der Bibliotheken

`dependencies-lock.json` legt SDK-Image, Paketversionen, Quelladressen und SHA-256-Werte fest. Die PSPBUILD-Rezepte wurden gegen `pkgbuild_sha256sum` in den `.BUILDINFO`-Dateien der tatsächlich installierten SDK-Pakete geprüft. Die Paketierung wiederholt diesen Abgleich und bricht bei fehlenden Quellen, Lizenzen oder falschen Prüfsummen ab. Es werden keine Bibliotheksrevisionen anhand des jeweils neuesten Branchstands ausgewählt.

Das Paket enthält Quellen für PSPGL, SDL und dessen verwendete Erweiterungen, Bild-/Audio-Codecs, FreeType, PSP-IR-Tastaturunterstützung sowie SDK, Newlib, pthread-embedded und GCC. Einige Quellen gehen über die tatsächlich vom Linker übernommenen Funktionen hinaus. Die Bibliotheken behalten ihre jeweiligen Lizenzen; die Anwendung wird unter GPL-2.0-or-later weitergegeben.

## Nachbauen und Bibliotheken ändern

1. `game-source.tar.gz` auspacken und dessen README folgen. `build.json` nennt den Commit und das verwendete SDK-Image.
2. `psp-packages.tar.gz` enthält die exakten PSPBUILD-Rezepte samt lokalen Patches und Ersetzung von Konfigurationsdateien. Die übrigen Archive enthalten deren Quellen, einschließlich der PSP-Anpassungen in den angegebenen Git-Revisionen. Git-Archive enthalten den vollständigen Quellbaum, keine Git-Historie.
3. Für geänderte Bibliotheken die betreffenden Quellen entpacken, die `prepare`-/`build`-/`package`-Schritte des jeweiligen PSPBUILD mit dem PSP-SDK ausführen und die resultierenden Bibliotheken im SDK ersetzen. Die Rezepte dokumentieren Ordnernamen, Flags, Abhängigkeiten und Installationspfade. Ihr normaler `psp-makepkg`-Weg lädt die im Rezept angegebenen Originalquellen; für lokale Änderungen diese Quelldefinition entsprechend anpassen.
4. Die Anwendung mit `make clean` und dem enthaltenen PSP-Makefile neu bauen. Sämtlicher Anwendungscode zum erneuten Linken ist enthalten. Die PSPSDK-/Toolchain-Archive und `sdk-build-records/build.txt` dokumentieren auch die SDK-Laufzeitrevisionen.

Lokal erstellt `python3 tools/package-extremetuxracer.py` nach einem Build dieselben Pakete. Dafür muss der Arbeitsbaum dem Commit entsprechen; persönliche Spielstände und Profile werden nicht verwendet. Downloads landen im ignorierten `.cache/dependency-sources/` und werden auch bei Wiederverwendung auf ihre Prüfsummen geprüft.

Die CI prüft den numerischen Spielkern und die Struktur der erzeugten PSP-Datei. Sie misst keine FPS und ersetzt keinen Test in PPSSPP oder auf Hardware.
