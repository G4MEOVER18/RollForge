# RollForge

> Flipper Zero FAP · RollJam + RollLab + RF Jammer in einer App

![Version](https://img.shields.io/badge/version-1.0-blue)
![API](https://img.shields.io/badge/Flipper%20API-87.x-orange)
![Category](https://img.shields.io/badge/category-Sub--GHz-purple)
![License](https://img.shields.io/badge/license-GPL--3.0-red)

---

```
 ██████╗  ██████╗ ██╗     ██╗     ███████╗ ██████╗ ██████╗  ██████╗ ███████╗
 ██╔══██╗██╔═══██╗██║     ██║     ██╔════╝██╔═══██╗██╔══██╗██╔════╝ ██╔════╝
 ██████╔╝██║   ██║██║     ██║     █████╗  ██║   ██║██████╔╝██║  ███╗█████╗
 ██╔══██╗██║   ██║██║     ██║     ██╔══╝  ██║   ██║██╔══██╗██║   ██║██╔══╝
 ██║  ██║╚██████╔╝███████╗███████╗██║     ╚██████╔╝██║  ██║╚██████╔╝███████╗
 ╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚══════╝╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝
                G4MEOVER · Rolling Code Research Suite
```

---

## Übersicht

**RollForge** vereint alle Rolling-Code-Research-Werkzeuge in einer einzigen Flipper Zero App. Statt drei separate FAPs zu verwalten, steht alles in einem kompakten Menü bereit.

> **Nur für autorisierte Sicherheitstests, CTF-Wettbewerbe und Security Research an eigener Hardware.**

---

## Module

| Modul | Funktion |
|---|---|
| **RollJam** | RollJam-Angriff: Interlaced Jam + Capture Signal A → Jam + Capture Signal B → Replay A |
| **RollLab** | Research: Signal Analyzer, Replay Attack Demo, Rollback Probe, Sync-Window Probe |
| **RF Jammer** | CC1101 OOK-Interferenz auf 315 / 433.92 / 868 / 915 MHz |

---

## RollJam — Ablauf

```
Phase 1: Jam + RX (180ms/80ms interlaced)  →  Signal A captured
Phase 2: Jam + RX (180ms/80ms interlaced)  →  Signal B captured (Spare)
Phase 3: Replay Signal A                   →  Auto-öffnet das Fahrzeug
         Signal B bleibt beim Angreifer gültig
```

Der Angriff nutzt Rolling-Code-Systeme, die neu empfangene Codes gegenüber alten bevorzugen. Signal A wird durch Phase 1 "verbraucht" (Fahrzeug öffnet beim Replay), Signal B ist ein noch ungenutzter Spare-Code.

---

## RollLab — Modi

| Modus | Beschreibung |
|---|---|
| **Analyzer** | Signaleigenschaften: Edges, min/max/avg Timing, OOK-Heuristik, Preamble |
| **Replay** | Einfaches Capture → Replay (zeigt ob System gegen Replay-Angriffe schützt) |
| **Rollback** | 1 Code vorwärts, dann Replay des alten — testet Rückwärts-Akzeptanzfenster |
| **SyncWin** | 3 Codes vorwärts, dann Replay — testet Synchronisationsfenster-Größe |

---

## RF Jammer

- CC1101-basierte OOK-Interferenz (250µs Pulse)
- Frequenzen: **315 MHz**, **433.92 MHz**, **868 MHz**, **915 MHz**
- ↑/↓ während Betrieb: Frequenz wechseln ohne Neustart
- [OK] Toggle Start/Stop

---

## Hardware-Unterstützung

| CC1101 | Status |
|---|---|
| Interner CC1101 | ✅ |
| Externer CC1101 (GPIO) | ✅ bevorzugt |

---

## Installation

### Option A — SD-Karte
1. `rollforge.fap` aus [Release](https://github.com/G4MEOVER18/RollForge/releases/latest) laden
2. Auf SD-Karte: `/apps/Sub-GHz/rollforge.fap`
3. Flipper → Apps → Sub-GHz → RollForge

### Option B — Build
```bash
# Im Repo-Verzeichnis:
ufbt
# Deploy auf Flipper:
ufbt launch
```

---

## Kompatibilität

| Firmware | Status |
|---|---|
| G4MEOVER-FW v1.0.0 | ✅ |
| Momentum mntm-012 | ✅ |

---

## Verwandte Projekte

| Projekt | Beschreibung |
|---|---|
| [G4MEOVER-FW](https://github.com/G4MEOVER18/G4MEOVER-FW) | Custom Flipper Zero Firmware |
| [ProtoPirate](https://github.com/G4MEOVER18/ProtoPirate) | Car Keyfob RF Decoder/Emulator (27+ Protokolle) |
| [RollJam](https://github.com/G4MEOVER18/RollJam) | RollJam als eigenständige App |
| [RollLab](https://github.com/G4MEOVER18/RollLab) | RollLab als eigenständige App |
| [lora-ukfe](https://github.com/G4MEOVER18/lora-ukfe) | USB Army Penetrator — Remote-Control via Heltec ESP32 |

---

## Rechtlicher Hinweis

Dieses Tool ist ausschließlich für **autorisierte Sicherheitstests, CTF-Wettbewerbe und Security Research** an eigener Hardware bestimmt. Die Nutzung gegen Fahrzeuge oder Systeme ohne ausdrückliche Genehmigung des Eigentümers ist illegal.

---

## Support

[![PayPal](https://img.shields.io/badge/PayPal-Spenden-0070ba?style=flat-square&logo=paypal)](https://paypal.me/Freakbank1)
[![Bitcoin](https://img.shields.io/badge/BTC-39vZWmnUwDReQ15BwqQXzyqVQ6U8LardEf-f7931a?style=flat-square&logo=bitcoin)](bitcoin:39vZWmnUwDReQ15BwqQXzyqVQ6U8LardEf)

```
BTC: 39vZWmnUwDReQ15BwqQXzyqVQ6U8LardEf

**Kontakt:** [g4me.over.18@gmail.com](mailto:g4me.over.18@gmail.com)
```

---

*Teil der G4MEOVER Security Toolchain · [github.com/G4MEOVER18](https://github.com/G4MEOVER18)*
