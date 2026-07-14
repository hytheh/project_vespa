# Annexe — Protocole CAN du projet VESPA

> **Source unique de vérité : [`software/common/can_protocol.h`](../../software/common/can_protocol.h).**
> Ce document décrit le protocole ; le header le *définit*. En cas de divergence,
> le header fait foi. Toute modification du protocole doit être faite dans
> `software/common/`, jamais dupliquée dans un nœud.

## 1. Couche physique

| Paramètre | Valeur |
|---|---|
| Standard | CAN 2.0B (ISO 11898-1), trames **standard 11 bits** |
| Débit | **500 kbit/s** |
| Terminaison | 120 Ω à chaque extrémité du bus |
| Transceiver `motion_node` | WCMCU-230 — ⚠️ **contrefaçon 5 V**, voir [`bom.md`](bom.md) |
| Transceiver `vision_node` | Jetson Orin Nano — contrôleur CAN intégré (MTTCAN), broches 29/31 |

> **Le protocole a tourné sur le bus physique** : le Jetson a reçu **111 909 trames du
> STM32, sans une seule erreur** (relevé `ip -s link show can0`, 21 mars 2026). Il a d'abord
> été développé sur interface virtuelle `vcan0`, et le passage au `can0` réel n'a demandé
> **aucune modification du code applicatif**.
>
> Deux points restent à refermer côté matériel — le sens émission, et la persistance de
> l'*overlay* de *pinmux* après redémarrage. Voir *Difficultés techniques*, verrou « bus CAN
> physique », et [`jetson_setup.md`](jetson_setup.md).

## 2. Principe de conception

Deux décisions structurent le protocole :

**Priorité par l'identifiant.** L'arbitrage bit à bit du CAN est non destructif : à
collision, la trame d'identifiant le plus faible gagne le bus sans retransmission. Les
identifiants sont donc attribués par criticité décroissante — `FIRE_CMD` (`0x010`) est
la trame la plus prioritaire du système, les trames de débogage (`0x200`+) les moins
prioritaires.

**Virgule fixe, pas de flottant.** Les angles sont transmis en `int16_t` après
multiplication par `ANGLE_SCALE_FACTOR = 10000`. Cela évite toute sérialisation de
`float` (représentation non garantie entre STM32, ESP32 et aarch64) et supprime le coût
de conversion. Résolution : 10⁻⁴ rad ≈ 0,0057° — bien en deçà de la précision mécanique
(0,022°), donc non limitante.

```c
// Émission                          // Réception
int16_t tx = (int16_t)(rad * 10000.0f);   float rad = rx / 10000.0f;
```

## 3. Dictionnaire de données

| ID | Trame | Émetteur | Récepteur(s) | DLC | Charge utile |
|---|---|---|---|---|---|
| `0x010` | `FIRE_CMD` | vision | coord | 1 | `uint8_t fire_order` — `0x01` = tir, `0x00` = maintien |
| `0x020` | `TARGET_VEC` | vision | motion | 7 | `int16 pan`, `int16 tilt` (×10⁴ rad), `uint16 depth_mm`, `uint8 is_locked` |
| `0x030` | `POS_MOTION` | motion | vision | 4 | `int16 current_pan`, `int16 current_tilt` (×10⁴ rad) |
| `0x040` | `POS_COORD` | coord | vision | 4 | `int32 position_mm` — position linéaire du collimateur |
| `0x100` | `HB_VISION` | vision | coord | 1 | `uint8 system_state` |
| `0x110` | `HB_MOTION` | motion | coord | 1 | `uint8 system_state` |
| `0x120` | `HB_COORD` | coord | coord | 1 | `uint8 system_state` |
| `0x200` | `DBG_MOTION` | motion | broadcast | — | Trame de débogage |
| `0x210` | `DBG_COORD` | coord | broadcast | — | Trame de débogage |

**Codes d'état (`system_state`)** — communs à tous les *heartbeats* :

| Code | Signification |
|---|---|
| `0x00` | `OK` — nominal |
| `0x01` | `WARNING` — dégradé, opération poursuivie |
| `0x02` | `FAULT` — défaut ; le watchdog doit couper l'alimentation |

**Cadences.** `POS_MOTION` : 20 Hz (50 ms). `HB_*` : 1 Hz. `TARGET_VEC` : cadence caméra
(60 Hz nominal). Ces valeurs sont fixées dans `software/motion_node/src/main.cpp`.

## 4. Structures (extrait de `can_protocol.h`)

Toutes les structures sont `__attribute__((packed))` : **aucun bourrage**, sinon la
disposition mémoire diffère entre STM32 (Cortex-M4) et aarch64 et les trames sont
décodées de travers.

```c
typedef struct {
    int16_t  pan_angle;   // rad × 10000
    int16_t  tilt_angle;  // rad × 10000
    uint16_t depth_mm;    // mm
    uint8_t  is_locked;   // 1 = verrouillé
} CAN_PACKED TargetPayload_t;      // 7 octets
```

## 5. Implémentations

| Nœud | Fichier | Pile |
|---|---|---|
| `vision_node` | `software/vision_node/src/vision_can.cpp` | SocketCAN (`PF_CAN`/`SOCK_RAW`) |
| `motion_node` | `software/motion_node/src/motion_can.cpp` | FDCAN1 en mode CAN 2.0 classique |
| commun | `software/common/can_utils.c` | Encodage/décodage, partagé par les deux |

Le nœud de coordination (`coordination_node`, ESP32-S2) **n'est pas implémenté** : ses
trames (`FIRE_CMD` en réception, `POS_COORD`, `HB_COORD`) sont spécifiées ci-dessus mais
aucun code ne les émet ni ne les consomme.

## 6. Vérification sans matériel

Le protocole se teste intégralement sur un bus virtuel, sans aucun câblage :

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0
candump vcan0 &                    # observer
# lancer vision_node configuré sur vcan0 : les trames 020 / 010 doivent apparaître
```

`vcan0` est indiscernable d'un bus réel du point de vue de SocketCAN : c'est ainsi que la
sérialisation a été mise au point avant que le matériel soit prêt, puis reportée sur `can0`
sans changer une ligne.

## 7. Diagnostic sur bus physique

Les compteurs du noyau sont l'outil de diagnostic le plus utile du projet — c'est leur
lecture qui a révélé la contrefaçon des transceivers :

```bash
ip -details -statistics link show can0
```

| Ce que vous voyez | Ce que cela signifie |
|---|---|
| `RX` progresse, `TX` figé, état `BUS-OFF` | Vous **recevez** mais n'**émettez** pas. Bus asymétrique ⇒ **couche physique**, pas logiciel. C'était la contrefaçon. |
| `RX` et `TX` à 0, état `ERROR-ACTIVE` | Les broches ne sont probablement pas routées. Vérifier le *pinmux* (`grep -i paa`). |
| `RX errors` grimpe | Signal dégradé : fronts trop lents (convertisseur de niveau ?) ou terminaison absente. |

> **Un bus CAN différentiel ne peut pas être asymétrique.** Si un sens marche et pas
> l'autre, le défaut est électrique, entre le contrôleur et le fil.
