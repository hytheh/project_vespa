# Annexe — Nomenclature (BOM)

Nomenclature du prototype **mk.2**, telle qu'effectivement approvisionnée.
Total ≈ **830 € TTC** (18 références).

---

## Calcul et électronique

| Élément | Fournisseur | Qté | Prix TTC |
|---|---|---:|---:|
| Jetson Orin Nano Super Developer Kit | RS Online | 1 | 311,18 € |
| B-G431B-ESC1 — *voir divergence n°1* | Mouser | 2 | 16,31 € |
| ESP32-C3 SuperMini — *nœud de coordination, non utilisé* | AliExpress | 1 | 4,29 € |
| Alimentation AC-DC 24 V / 150 W | AliExpress | 1 | 5,29 € |
| Transceivers CAN WCMCU-230 / SN65HVD230 (lot de 10) | AliExpress | 1 | 5,19 € |
| TMC2209 (lot de 4) — *pilotage collimation* | AliExpress | 1 | 9,09 € |

## Vision

| Élément | Fournisseur | Qté | Prix TTC |
|---|---|---:|---:|
| Module Arducam OV9281 1 MP monochrome *global shutter* | Arducam | 2 | 142,59 € |
| Objectif M12 5 MP 4,6 mm sans distorsion | AliExpress | 2 | 9,29 € |

## Mécanique et actionneurs

| Élément | Fournisseur | Qté | Prix TTC |
|---|---|---:|---:|
| iFlight iPower **GM5208-12** + encodeur **AS5048A** (paire) | AliExpress | 1 | 134,99 € |
| Actionneur linéaire course 35 mm | AliExpress | 1 | 20,42 € |
| Actionneur linéaire course 10 mm | AliExpress | 2 | 1,99 € |
| Capteur photoélectrique en U — *prise d'origine* | AliExpress | 1 | 2,54 € |

## Optique de tir

| Élément | Fournisseur | Qté | Prix TTC |
|---|---|---:|---:|
| **Diode laser bleue 5,5 W** — ⚠️ **Classe 4** | AliExpress | 1 | 83,69 € |
| Lentille asphérique K9 D63 F100 — *lentille L2, focale 100 mm* | AliExpress | 1 | 23,59 € |

## Sécurité — non optionnel

| Élément | Fournisseur | Qté | Prix TTC |
|---|---|---:|---:|
| **Lunettes de protection OD7+ CE 190–550 nm** | AliExpress | 1 | 30,29 € |

---

## ⚠️ Deux divergences entre la nomenclature et le matériel réellement utilisé

### 1. Cartes de commande moteur

La nomenclature liste **2× B-G431B-ESC1**. Mais le firmware livré cible en réalité une
**NUCLEO-G431RB** portant **2× X-NUCLEO-IHM16M1** (STSPIN830) :

- `software/motion_node/platformio.ini` → `board = nucleo_g431rb`
- `software/motion_node/include/pinout.h` → brochage des shields IHM16M1
- `docs/annexes/cablage.md` → câblage détaillé de cette configuration

**Suivre le code, pas la nomenclature.** La ligne B-G431B-ESC1 correspond
vraisemblablement à une intention antérieure abandonnée.

### 2. Le laser

| Source | Spécification |
|---|---|
| **Nomenclature** (matériel acheté) | **5,5 W**, bleu |
| **Rapport de soutenance** (dimensionnement théorique) | 5 W à 460 nm |

Les deux sont cohérentes en ordre de grandeur : le calcul d'irradiance (≈ 2 200 W/cm² sur
un spot de 0,53 mm) a été mené pour 5 W, la diode physiquement présente fait 5,5 W.
**Classe 4 dans les deux cas.** Voir [`securite_laser.md`](securite_laser.md).

---

## Caractéristiques du moteur GM5208-12

Source : [`hardware/mechanical/GM5208-12.json`](../../hardware/mechanical/GM5208-12.json)

| Paramètre | Valeur |
|---|---|
| Type | PMSM *pancake* (gimbal), 14 aimants |
| Paires de pôles | **7** |
| Résistance statorique `Rs` | 7,48 Ω |
| Inductance `Ls` | 4,49 mH |
| Tension nominale | 24 V |
| Courant nominal | 1,5 A |
| Vitesse max. | 504 |
| Constante de FEM | 32,96 |
| Encodeur | **AS5048A**, magnétique absolu **14 bits** |
| Résolution angulaire théorique | 360 / 2¹⁴ ≈ **0,022°** |

---

## Nomenclature du prototype mk.1 (pour mémoire)

Le premier prototype (Cosson & Grimal, 2024/2025) utilisait un matériel radicalement
différent, dont les limites ont motivé le mk.2 :

- Arduino Uno (ATmega328P — **pas de FPU**, d'où le *jitter* sur les rampes d'accélération)
- Moteurs pas-à-pas Nema 17 + drivers A4988 (microstepping 1/32) — **décrochage**,
  dérive cumulative
- Diode laser rouge 650 nm, **10 mW** — sans capacité de neutralisation
- Pas de système de vision embarqué

Le code de caractérisation de ce banc est conservé dans
[`software/legacy_mk1_bench/`](../../software/legacy_mk1_bench/) : ce sont les annexes A à D
du rapport de soutenance.
