# `coordination_node` — NON IMPLÉMENTÉ

> **Ce nœud n'existe pas.** Ce dossier ne contient aucun code : uniquement sa spécification.
> Il est conservé dans l'arborescence parce que le protocole CAN réserve ses identifiants et
> que le reste du système est écrit en supposant son existence future.

**Cible matérielle prévue :** ESP32 **bi-cœur** (ESP32 classique ou ESP32-S3).

> ⚠️ L'ESP32-**C3** SuperMini listé dans la nomenclature d'origine est **mono-cœur**
> (RISC-V) : il ne peut pas fournir le « cœur CPU dédié » exigé par le watchdog ci-dessous.
> Deux options pour une reprise : (a) viser un ESP32/ESP32-S3 **bi-cœur** pour réserver un
> cœur au superviseur ; ou (b) sur mono-cœur, réaliser l'isolation par une **tâche FreeRTOS
> de priorité maximale épinglée** + le **watchdog matériel (TWDT)** de la puce. L'option (a)
> est recommandée pour un organe qui conditionne un laser de Classe 4.

---

## Pourquoi c'est le chantier n°2 de toute reprise

Ce nœud porte le **watchdog**. Le watchdog est la **condition *sine qua non*** de toute mise
sous tension du laser de Classe 4. Tant qu'il n'existe pas, **le laser ne doit pas être
alimenté** — et il ne l'a jamais été.

Voir [`docs/annexes/securite_laser.md`](../../docs/annexes/securite_laser.md).

---

## Les trois fonctions à implémenter

### 1. Watchdog (priorité absolue)

Superviseur centralisé, **sur un cœur CPU dédié** (voir la note matérielle ci-dessus), au
plus près de l'organe dangereux.

- Écoute les *heartbeats* périodiques de tous les nœuds : `HB_VISION` (`0x100`),
  `HB_MOTION` (`0x110`), `HB_COORD` (`0x120`).
- Chaque *heartbeat* porte un `uint8_t system_state` : `0x00` OK, `0x01` WARNING,
  `0x02` FAULT (énumération normative dans `can_protocol.h`).
- **Timeout par nœud (à figer) :** valeur proposée **200 ms** de silence glissant. Le
  firmware actuel émet les *heartbeats* à **1 Hz** (`motion_node`) ; pour un interlock à
  200 ms, il faut **relever la cadence** sur le lien critique (p. ex. ≥ 10 Hz), ce qui est
  un préalable d'implémentation.
- **Déclenche la mise en sécurité si** : un nœud signale `FAULT`, **ou** un nœud cesse
  d'émettre au-delà du *timeout* ci-dessus.
- **Signal d'interlock — ligne physique, pas une trame CAN.** Le superviseur pilote une
  sortie GPIO dédiée `INTERLOCK_EN`, **fail-safe** (niveau **bas = tir interdit** ; l'organe
  de tir n'est armé que sur niveau haut maintenu). Elle est **indépendante du bus** pour ne
  pas dépendre de la panne qu'elle doit traiter (cf. l'avertissement ci-dessous).
- La mise en sécurité est **matérielle** : coupure de l'**alimentation générale** et passage
  de `INTERLOCK_EN` au niveau bas. Pendant le temps de décharge des condensateurs, l'ordre
  de tir est en outre inhibé au niveau logiciel.

> ⚠️ Un superviseur qui dépendrait du bus pour couper le courant serait vulnérable à la
> panne même qu'il doit traiter. La coupure doit être physique et par défaut fermée
> (*fail-safe*).

### 2. Séquence de tir

- Consomme `FIRE_CMD` (`0x010`) émis par le `vision_node` — identifiant le plus faible du
  protocole, donc **priorité absolue** à l'arbitrage CAN.
- N'autorise le tir que si **toutes** les conditions sont réunies : cible verrouillée,
  watchdog nominal, interlock fermé, collimateur en position.

### 3. Collimation dynamique

- Pilote la lentille mobile **L2** (actionneur linéaire + TMC2209).
- Publie sa position absolue sur `POS_COORD` (`0x040`, `int32_t position_mm`).
- La **loi de commande est déjà établie** (relations de conjugaison de Descartes, approximée
  par un polynôme à 1e-4 mm près) — voir le rapport de soutenance §10.2 et
  `simulation/optique/Laser_Optics_v1.m`.

---

## Trames CAN réservées

| ID | Trame | Sens | Charge utile |
|---|---|---|---|
| `0x010` | `FIRE_CMD` | ← vision | `uint8_t fire_order` (`0x01` = tir) |
| `0x040` | `POS_COORD` | → vision | `int32_t position_mm` (collimateur) |
| `0x120` | `HB_COORD` | → (watchdog) | `uint8_t system_state` |
| `0x210` | `DBG_COORD` | → broadcast | Débogage |

Définition normative : [`software/common/can_protocol.h`](../common/can_protocol.h).
Le protocole est **partagé** — l'implémentation de ce nœud doit lier `software/common/`,
et surtout **ne pas redéfinir les structures**.

---

## Point de départ

Le code CAN est portable : `software/common/can_utils.c` compile sur STM32, ESP32 et Linux.
Un ESP32 dispose d'un contrôleur CAN natif (TWAI), compatible avec le bus 500 kbit/s du
projet, moyennant un transceiver.

⚠️ **Attention au transceiver.** Les modules WCMCU-230 approvisionnés pour le projet sont
des **contrefaçons 5 V** vendues pour des SN65HVD230 3,3 V (boîtiers poncés et re-marqués).
Ils fonctionnent, mais **alimentés en 5 V**, avec un pont diviseur 1 kΩ / 2 kΩ sur la sortie
`CRX`. Vérifier ce que vous avez en main avant de câbler — voir
[`docs/annexes/bom.md`](../../docs/annexes/bom.md).

Le plus simple pour démarrer reste de développer sur `vcan0`, comme l'ont fait les autres
nœuds : l'interface est indiscernable d'un bus réel côté SocketCAN, et le passage au matériel
n'a demandé aucune modification du code. Voir
[`docs/Difficultes_techniques.pdf`](../../docs/Difficultes_techniques.pdf).
