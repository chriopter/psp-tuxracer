# Extreme Tux Racer für PSP

Inoffizieller PSP-Homebrew-Port von **Extreme Tux Racer 0.8.4**, auf Basis der offiziellen C++-PC-Quellen. Getestet in **PPSSPP 1.20.4 mit 333 MHz**, nativer PSP-Auflösung und Musik. Der frühere Tux-Racer-0.61-Versuch wurde durch diese Spielbasis ersetzt.

![Extreme Tux Racer auf PSP in PPSSPP](docs/images/extreme-tux-racer-psp.png)

## CI-Downloads

[![PSP build](https://github.com/chriopter/tuxracer-psp/actions/workflows/psp.yml/badge.svg)](https://github.com/chriopter/tuxracer-psp/actions/workflows/psp.yml)

Unter **Actions → PSP build → erfolgreicher Lauf → Artifacts** liegt das vollständige Paket: Spiel-ZIP, passende Quellen und Prüfsummen. Das ZIP enthält `PSP/GAME/ExtremeTuxRacer/` und lässt sich auf die PSP kopieren oder in PPSSPP öffnen. Tags `v*` erzeugen zusätzlich ein [Release](https://github.com/chriopter/tuxracer-psp/releases).

Die CI baut und prüft bei jedem Push auf `main`; Artefakte bleiben 30 Tage verfügbar. Quellen und Lizenzbeigaben werden gemeinsam mit der Binary hochgeladen. [Paketinhalt und Nachbau](docs/binary-distribution.md).

## Bauen und starten

Benötigt: Docker, Python 3, FFmpeg, ImageMagick und PPSSPPSDL. Das SDK-Containerimage ist per Digest fixiert.

```sh
./build-extremetuxracer.sh
python3 tools/stage-extremetuxracer.py
./start-extremetuxracer.sh
```

Optional `PPSSPP_BIN=/pfad/zu/PPSSPPSDL` setzen. Beim ersten Staging wird ein eigenes PPSSPP-Profil mit 333 MHz, 1× Auflösung und ohne Frameskip angelegt; vorhandene Einstellungen bleiben erhalten.

Mit Xvfb läuft `./start-extremetuxracer-background.sh` auf einem unsichtbaren Display, standardmäßig stumm. `TUXRACER_BACKGROUND_AUDIO=1` schaltet dabei die Audioausgabe ein.

Das spielbereite Verzeichnis liegt unter `state/extremetuxracer/config/ppsspp/PSP/GAME/ExtremeTuxRacer/`. Es enthält `EBOOT.PBP`, `data/` und `config/`. Die Laufzeitdaten und fertigen Binärdateien werden nicht ins Repository eingecheckt.

## Steuerung

| PSP | Aktion | Vorgabe in PPSSPP am PC |
|---|---|---|
| Steuerkreuz / Analogstick | Lenken, Menüs | Pfeiltasten / IJKL |
| Hoch / R | Anschieben | Hoch / W |
| Runter / L | Bremsen | Runter / Q |
| Cross | Springen / bestätigen | Leertaste |
| Circle | Zurück / Rennen beenden | Rücktaste |
| Square + Richtung | Trick | A + Richtung |
| Triangle | Zur Strecke zurücksetzen | R |
| Start | Pause / fortsetzen | Eingabetaste |

## Benchmarks

Gemessen werden die Zeitabstände zwischen präsentierten Frames **innerhalb der emulierten PSP**, einschließlich VSync. Ladezeiten und der erste Rennframe sind ausgeschlossen. Automatisch: Anschieben, jeweils 30 Frames links/rechts pro 240-Frame-Zyklus. Musik und Effekte sind aktiviert.

| Strecke / Abschnitt | Frames | Ø FPS | 95-%-Framezeit | Schlechtester Frame | Über 35 ms |
|---|---:|---:|---:|---:|---:|
| Frozen River, vollständiger Lauf | 2.844 | 59,940 | 16,684 ms | 17,626 ms | 0 |
| Davon mit Lenkung | 720 | 59,942 | 16,684 ms | 17,626 ms | 0 |
| Path of Daggers, 60-Sekunden-Test | 3.599 | 59,874 | 16,684 ms | 33,367 ms | 0 |
| Davon mit Lenkung | 900 | 59,873 | 16,684 ms | 33,367 ms | 0 |

Rohwerte und Build-Prüfsummen: [Benchmark-Daten](docs/benchmarks/). **Keine Messung auf einer physischen PSP.** Die emulierte CPU ist auf 333 MHz gestellt; daraus folgt keine garantierte Leistung auf echter Hardware. PSP-VSync liegt bei etwa 59,94 Hz.

Die [Audioaufzeichnung](docs/benchmarks/audio.json) weist ein nicht stummes Signal ohne Übersteuerung nach. Im Frozen-River-Benchmark lief Musik während aller 2.844 gemessenen Frames.

Zum Wiederholen vor dem Start in `.../ExtremeTuxRacer/config/benchmark` beispielsweise `7200 frozen_river` eintragen. Das Spiel startet den Test automatisch und schreibt `config/benchmark-result.json`. Die Datei `benchmark` anschließend entfernen, um normal zu spielen.

## Tokenaufwand der ersten Version

Die angefragte erste Version war der **erste startfähige klassische Tux-Racer-Prototyp**, vor dem Wechsel zu Extreme Tux Racer. Zeitraum: 7. September 2026, 08:04:59–08:14:02 UTC; 44 eindeutige Modellantworten im Hauptthread.

| Zählgröße | Tokens |
|---|---:|
| Eingabe gesamt, einschließlich Cache | 5.381.836 |
| Davon aus dem Cache | 5.289.472 |
| Eingabe ohne Cache | 92.364 |
| Ausgabe, einschließlich Reasoning | 14.907 |
| Eingabe + Ausgabe gesamt | **5.396.743** |

Die Eingabe zählt auch wiederholt gelesenen Gesprächskontext. Reasoning ist bereits in der Ausgabe enthalten. Das sind **keine Kostenangaben** und nicht der Aufwand des späteren Extreme-Tux-Racer-Ports. [Abgrenzung und aggregierte Messwerte](docs/token-usage.json); private Gesprächsprotokolle sind nicht enthalten.

## Quellen, Änderungen und Grenzen

Spiel und Daten stehen unter **GPL-2.0-or-later**, mit gesonderter erlaubender Quadtree-Lizenz. Die ursprünglichen Autorenvermerke bleiben erhalten. Alle 466 Spieldateien stimmen byteweise mit dem separat lizenzdokumentierten Debian-0.8.4-Quellarchiv überein. Details: [Lizenzen und Herkunft](docs/licensing.md), [Prüfsummen](docs/upstream.json), [GPL](LICENSE).

Die PSP-Ausgabe verwendet Single-Precision-Mathematik, native Vertexlayouts, gebündelte Schneepartikel, 16-Bit-Texturen, begrenzte Höhenkarten und PCM-Musik. Details und verbleibende Einschränkungen, etwa die noch fehlende Bildschirmtastatur: [Portierungsnotizen](docs/porting.md).

```sh
python3 tools/test-etr-numerics.py
```

Der Test prüft die tatsächlichen numerischen Spielquellen gegen analytische Lösungen und bekannte Geometriefälle. Das Projekt ist nicht mit dem Extreme Tux Racer Team oder Sony verbunden.
