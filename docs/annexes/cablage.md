# Annexe — Câblage du `motion_node`

Matériel : 1× **NUCLEO-G431RB**, 2× **X-NUCLEO-IHM16M1** (STSPIN830), 2× encodeurs
**AS5048A**, 1× transceiver CAN **WCMCU-230**.
Les deux shields sont montés **côte à côte** (et non empilés), cavaliers configurés pour
la FOC à **3 shunts**.

> **Source unique de vérité : [`software/motion_node/include/pinout.h`](../../software/motion_node/src/../include/pinout.h).**
> Le firmware ne lit que ce fichier. Le tableau ci-dessous en est le reflet.

---

## ⚠️ Deux pièges à connaître avant de câbler

Ces deux points ne sont pas des erreurs de conception : ce sont des contraintes de la carte
Nucleo qui ont coûté du temps et qui **invalident le manifeste de câblage d'origine**.

### 1. Le potentiomètre PAN n'est **pas** sur `PA2` — il est sur `PB14`

`PA2` est le brochage par défaut du shield IHM16M1 pour l'entrée « SPEED »
(potentiomètre). Mais sur la NUCLEO-G431RB, **`PA2` est câblée sur `USART2_TX`, la liaison
série virtuelle du ST-LINK** (le port COM que l'on ouvre pour lire la console).

Y brancher le potentiomètre revient à court-circuiter la sortie série : la console devient
muette ou crache des caractères parasites, sans qu'aucun symptôme ne pointe vers le
potentiomètre. Le potentiomètre PAN a donc été **déplacé sur `PB14`**.

> Le manifeste historique [`cablage_motion_node.txt`](cablage_motion_node.txt) (ex-
> `STM32_WiringManifest.txt`, conservé pour mémoire) indique encore `PA2`.
> **Ne pas le suivre sur ce point.**

### 2. `PA4` ne fait pas de retour de courant — il protège le SPI

`PA4` est à la fois la broche `SENSEW` du moteur 1 (retour de courant phase W) *et* la
broche **NSS matérielle du SPI1**. Le bus SPI1 porte les deux encodeurs AS5048A, dont
dépend tout l'asservissement. Un NSS matériel laissé flottant peut faire basculer le
périphérique SPI en mode esclave et tuer la lecture des encodeurs.

`PA4` est donc **maintenue haute et réservée au SPI**. Conséquence assumée : le retour de
courant de la phase W du moteur 1 est sacrifié. Le FOC fonctionne (SimpleFOC reconstruit
la 3ᵉ phase : `iW = -iU - iV`), mais **la mesure de courant n'est pas activée dans le
firmware actuel** — l'asservissement est en position/vitesse sur encodeur seul.

---

## 1. Alimentation et masses

Le 24 V moteur est **isolé** de la logique 3,3 V. Les masses sont communes.

| Signal | NUCLEO-G431RB | Shield 1 (M1) | Shield 2 (M2) |
|---|---|---|---|
| 24 V puissance | — | CN1 pin 2 (VIN) | CN1 pin 2 (VIN) |
| Masse puissance | — | CN1 pin 1 (GND) | CN1 pin 1 (GND) |
| Logique 3,3 V | 3V3 | CN7 pin 12 | CN7 pin 12 |
| Masse logique | GND | CN7 pin 20 | CN7 pin 20 |

## 2. Moteur 1 — PAN (TIM1 / ADC1, brochage par défaut du shield)

| Signal | MCU | Shield | Morpho |
|---|---|---|---|
| Commande INU | `PA8` | INU | CN10-23 |
| Commande INV | `PA9` | INV | CN10-21 |
| Commande INW | `PA10` | INW | CN10-33 |
| EN / FAULT | `PA11` | EN\FAULT | CN10-14 |
| Courant U | `PA1` | SENSEU | CN7-30 |
| Courant V | `PB1` | SENSEV | CN10-24 |
| Courant W | ~~`PA4`~~ | SENSEW | *(réservé SPI1-NSS — voir piège n°2)* |
| Tension bus | `PA0` | VBUS | CN7-28 |
| **Potentiomètre** | **`PB14`** | SPEED | *(déplacé — voir piège n°1)* |

## 3. Moteur 2 — TILT (TIM8 / ADC2, routage MCU personnalisé)

Le second shield ne peut pas réutiliser le brochage par défaut : il est remappé sur
un second timer avancé (TIM8) et un second ADC.

| Signal | MCU | Shield | Morpho |
|---|---|---|---|
| Commande INU | `PC6` | INU | CN10-23 |
| Commande INV | `PC7` | INV | CN10-21 |
| Commande INW | `PC8` | INW | CN10-33 |
| EN / FAULT | `PB12` | EN\FAULT | CN10-14 |
| Courant U | `PC0` | SENSEU | CN7-30 |
| Courant V | `PC1` | SENSEV | CN10-24 |
| Courant W | `PC2` | SENSEW | CN7-34 |
| Tension bus | `PC3` | VBUS | CN7-28 |
| Potentiomètre | `PB0` | SPEED | CN7-35 |

## 4. Encodeurs AS5048A — SPI1 (bus partagé, 2 chip-selects)

Bus SPI commun aux deux encodeurs ; seule la ligne CS les distingue.

| Signal | MCU | AS5048A (M1) | AS5048A (M2) | Fil |
|---|---|---|---|---|
| Masse | CN7-20 | GND | GND | blanc |
| 3,3 V | CN7-16 | VCC | VCC | rouge |
| MISO | `PA6` | MISO | MISO | vert |
| MOSI | `PA7` | MOSI | MOSI | jaune |
| SCK | `PA5` | SCK | SCK | bleu |
| CS pan | `PB6` | CS | — | noir |
| CS tilt | `PB7` | — | CS | noir |
| *(NSS matériel)* | `PA4` | *maintenu haut* | | |

## 5. Transceiver CAN WCMCU-230 — FDCAN1

| Signal | MCU | WCMCU-230 |
|---|---|---|
| 3,3 V | CN7-16 | 3V3 |
| Masse | CN7-20 | GND |
| CAN TX | `PB9` (CN10-5) | CTX / TX |
| CAN RX | `PB8` (CN10-3) | CRX / RX |
| CAN High | — | CANH → bus |
| CAN Low | — | CANL → bus |

> ⚠️ **Attention au piège du constructeur `MotionCan`.** Sa signature déclare des valeurs
> par défaut `MotionCan(rxPin = PA11, txPin = PA12)` — qui sont **fausses** pour ce
> câblage (`PA11` est le EN/FAULT du moteur 1 !). Le firmware passe toujours les broches
> explicitement : `MotionCan canBus(CAN_RX, CAN_TX);` → `PB8`/`PB9`.
> **Ne jamais instancier `MotionCan` sans arguments.**

## 6. Sécurité

| Fonction | Broche |
|---|---|
| Arrêt d'urgence matériel (bouton bleu) | `PC13` |

L'arrêt d'urgence est **latché** : l'ISR coupe les deux étages de puissance
(`EN/FAULT` → bas), diffuse un *heartbeat* `FAULT` (`0x02`) sur le CAN, puis entre dans une
boucle infinie. **Seul un reset matériel (bouton noir) permet de repartir.** C'est
volontaire : sur un système portant un laser de Classe 4, aucun redémarrage automatique
n'est acceptable.
