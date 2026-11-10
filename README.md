# GamePad-Wächter (GamePad Guard)

Ein Aroma-Plugin für die Wii U mit zwei Spielmodi:

| Modus | Was passiert |
|---|---|
| **Automatisch** (Standard) | Ist kein GamePad verbunden, werden Spiele aus der Sperrliste nicht gestartet. Mit GamePad läuft alles normal. |
| **Ohne GamePad** | Spiele aus der Sperrliste werden immer blockiert. |
| **Mit GamePad** | Nichts wird blockiert, die Wii U verhält sich ganz normal. |

Startest du im GamePad-losen Modus ein gesperrtes Spiel (z. B. *Wii Sports Club*), kehrt die Wii U sofort
zum Wii-U-Menü zurück und zeigt die Meldung:

> "Wii Sports Club" benoetigt das Wii U GamePad. Bitte verbinde dein GamePad, um fortzufahren.

## Modus wechseln

* **Schnellwechsel:** `L + R + MINUS` 1,5 Sekunden gedrückt halten (GamePad oder Pro Controller).
  Reihenfolge: Automatisch → Ohne GamePad → Mit GamePad → Automatisch.
* **Plugin-Menü von Aroma:** `L + Steuerkreuz unten + SELECT` → *GamePad-Waechter*.
* Nach dem Einschalten zeigt das Plugin im Wii-U-Menü kurz an, welcher Modus aktiv ist
  (lässt sich im Plugin-Menü abschalten).

## Sperrliste

Die Liste liegt auf der SD-Karte unter `SD:/wiiu/gamepad-guard/blocklist.txt` und wird beim ersten Start
automatisch angelegt. Pro Zeile ein Eintrag:

```
# Name (oder Teil davon), Groß-/Kleinschreibung egal
Wii Sports Club
Nintendo Land

# oder eine Title-ID (16 Hex-Zeichen)
00050000-10101E00
```

Namen werden mit den englischen, deutschen, französischen, italienischen, spanischen, niederländischen und
portugiesischen Titeln des Spiels verglichen – ein Eintrag funktioniert also unabhängig von der Region.
Änderungen gelten beim nächsten Spielstart, ohne Neustart.

Die mitgelieferte Startliste ist **nicht vollständig und nicht für jedes Spiel geprüft** – bitte an die
eigene Sammlung anpassen.

## Installation

1. [Aroma](https://aroma.foryour.cafe/) muss auf der Wii U installiert sein.
2. Unter *Actions* → letzter erfolgreicher Lauf → Artefakt **GamePadGuard-SD** herunterladen.
3. Inhalt auf die SD-Karte kopieren:
   * `wiiu/environments/aroma/plugins/GamePadGuard.wps`
   * `wiiu/gamepad-guard/blocklist.txt`
4. Wii U neu starten.

## Selbst bauen

```sh
docker build . -t gamepad-guard-builder
docker run --rm -v ${PWD}:/project gamepad-guard-builder make
```

Ergebnis: `GamePadGuard.wps`.

## Grenzen

* Das Wii-U-Menü selbst wird nicht verändert – gesperrte Spiele werden dort **nicht ausgegraut**. Die Sperre
  greift beim Start des Spiels.
* Das Spiel beginnt kurz zu laden, bevor es zum Menü zurückgeht.
* Ob ein Spiel das GamePad wirklich braucht, lässt sich nicht automatisch erkennen – dafür ist die Sperrliste da.
* Eine eigene Auswahl *vor* dem Wii-U-Menü (direkt beim Einschalten) bräuchte ein eigenes Aroma-Modul statt
  eines Plugins. Bis dahin übernehmen der Modus *Automatisch* und der Hinweis nach dem Einschalten diese Aufgabe.
* Meldungen verwenden aktuell Umschreibungen statt Umlauten (ae/oe/ue), um Darstellungsprobleme zu vermeiden.
