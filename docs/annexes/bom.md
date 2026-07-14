# Annexe — Nomenclature (BOM)

Nomenclature du prototype **mk.2**, **telle que réellement montée**.
Total en fin de page. Les prix de la colonne `Prix TTC` sont des **prix de ligne**
(quantité comprise), et non des prix unitaires.

---

## Calcul et électronique

| Élément | Fournisseur | Qté | Prix TTC |
|---|---|---:|---:|
| Jetson Orin Nano Super Developer Kit | RS Online | 1 | 311,18 € |
| **NUCLEO-G431RB** — *MCU du `motion_node`* | ST | 1 | — |
| **X-NUCLEO-IHM16M1** (STSPIN830) — *étages de puissance* | ST | 2 | — |
| ESP32-C3 SuperMini — *nœud de coordination, jamais implémenté* | AliExpress | 1 | 4,29 € |
| Alimentation AC-DC 24 V / 150 W | AliExpress | 1 | 5,29 € |
| Transceivers CAN WCMCU-230 (lot de 10) — ⚠️ **contrefaits, voir ci-dessous** | AliExpress | 1 | 5,19 € |
| TMC2209 (lot de 4) — *pilotage collimation* | AliExpress | 1 | 9,09 € |

> **Les deux shields IHM16M1 sont câblés côte à côte, et non empilés sur la Nucleo.**
> Ce n'est pas un détail de confort : empilés, la broche `A5` (qu'il faut tirer à 3,3 V pour
> sortir le STSPIN830 de veille) est reliée à `PC0`, qui sert de retour de courant `SENSEU`
> au moteur 2. L'empilement saturerait cette entrée et casserait la boucle de courant.

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

## Total

| | Prix TTC |
|---|---:|
| Calcul et électronique *(hors cartes ST, non chiffrées)* | 335,04 € |
| Vision | 151,88 € |
| Mécanique et actionneurs | 157,95 € |
| Optique de tir | 107,28 € |
| Sécurité | 30,29 € |
| **Sous-total — lignes chiffrées** | **782,44 €** |
| **NUCLEO-G431RB** (×1) + **X-NUCLEO-IHM16M1** (×2) | **à consigner** |
| **TOTAL** | **782,44 € + cartes ST** |

> ⚠️ **Ce total est incomplet et le restera tant que les trois cartes ST ne seront pas
> chiffrées.** Elles ont été achetées chez **Farnell France** mais leur prix n'a jamais été
> consigné, et il n'est pas reconstituable : il faut le relever sur la facture Farnell.
>
> L'ancien en-tête annonçait « Total relevé ≈ 830 € TTC ». **Ce chiffre ne se recoupe avec
> aucune lecture de la table** : la somme des lignes vaut 782,44 € (prix de ligne) ou
> 938,30 € (si on lisait la colonne comme des prix unitaires). Il a donc été retiré plutôt
> que propagé.

---

## ⚠️ Les transceivers CAN du lot sont des contrefaçons

Les modules **WCMCU-230** de ce lot sont vendus comme portant un **SN65HVD230** de Texas
Instruments (3,3 V). Ce sont des **clones 5 V** : les boîtiers ont été **poncés puis
re-marqués au laser**.

Câblés en 3,3 V comme le voudrait la théorie, ils donnent un bus **asymétrique** —
réception parfaite, émission morte — ce qui est impossible sur un bus différentiel et
constitue la signature du défaut.

| | Vrai SN65HVD230 | Le clone livré |
|---|---|---|
| Alimentation | 3,3 V | **5 V** (sur la broche sérigraphiée « 3V3 ») |
| Sortie `CRX` | 3,3 V, directe | **5 V** → **pont diviseur 1 kΩ / 2 kΩ** vers le Jetson |
| Entrée `CTX` | 3,3 V | Le 3,3 V du Jetson suffit à l'attaquer, sans adaptation |

> **Ne pas utiliser de convertisseur de niveau à MOSFET.** Ces petites cartes
> bidirectionnelles (prévues pour l'I²C) sont trop lentes à 500 kbit/s : les fronts
> s'arrondissent et le contrôleur CAN du Tegra les rejette comme du bruit. Un pont diviseur
> résistif n'a quasiment aucune capacité et garde le front franc.

**Vérifier lequel des deux vous avez avant de câbler.** Voir *Difficultés techniques*.

---

## Le laser : 5 W au calcul, 5,5 W à l'achat

| Source | Spécification |
|---|---|
| **Nomenclature** (matériel acheté) | **5,5 W**, bleu |
| **Rapport de soutenance** (dimensionnement théorique) | 5 W à 460 nm |

Les deux sont cohérentes en ordre de grandeur : le calcul d'irradiance (≈ 2 200 W/cm² sur
un spot de 0,53 mm) a été mené pour 5 W, la diode physiquement présente fait 5,5 W.
**Classe 4 dans les deux cas.** Voir [`securite_laser.md`](securite_laser.md).

---

## Caractéristiques du moteur GM5208-12

Source : [`hardware/mechanical/motor/GM5208-12.json`](../../hardware/mechanical/motor/GM5208-12.json)

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
différent, dont les limites ont motivé le mk.2 —
[rapport archivé](../references/rapports/Cosson_Grimal_2025_VESPA_mk1_Pointage.pdf) :

- Arduino Uno (ATmega328P — **pas de FPU**, d'où le *jitter* sur les rampes d'accélération)
- Moteurs pas-à-pas + shield **DRV8825** (microstepping 1/32) — **décrochage**,
  dérive cumulative, aucun retour de position
- Détection **YOLOv8** déportée sur PC, liaison Python ↔ Arduino par port série
- Diode laser rouge 650 nm, **10 mW** — sans capacité de neutralisation
- **Monoculaire** : aucune mesure de distance, donc aucune collimation possible

Le code de caractérisation de ce banc est conservé dans
[`software/legacy_mk1_bench/`](../../software/legacy_mk1_bench/) : ce sont les annexes A à D
du rapport de soutenance.
