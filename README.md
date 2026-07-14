# Projet V.E.S.P.A. — prototype mk.2

**V**ision-**E**nabled **S**entry for the **P**rotection of **A**piaries

Tourelle optronique autonome de suivi prédictif par stéréo-vision, destinée à détecter,
suivre et neutraliser le frelon asiatique (*Vespa velutina nigrithorax*) en vol aux abords
d'une ruche, à l'aide d'un laser de puissance à collimation dynamique.

Projet de fin d'études — **ENSIL-ENSCI, Limoges** — filière Mécatronique (MIX 5), promotion 2026.
Antonin Delmas, Clément Desmedt, Clément Gilson — encadrement : Michel Ousset, Thierry Cortier.

> ### ⚠️ Statut : prototype semi-fonctionnel — projet clôturé, ouvert à reprise
>
> Ce dépôt est **archivé en l'état** à la fin du projet de fin d'études. Il est structuré
> et documenté pour être **repris par une équipe suivante**. Il n'est pas un produit fini :
> voir [État d'avancement](#état-davancement) et surtout
> **[docs/Difficultes_techniques.pdf](docs/Difficultes_techniques.pdf)**, qui recense les
> verrous levés et ceux qui restent ouverts.
>
> **Le laser de puissance (diode bleue 5,5 W — Classe 4) n'a jamais été mis sous tension.**
> C'était une décision de sécurité délibérée : le *watchdog* matériel qui conditionne
> l'autorisation de tir n'a jamais été implémenté. Ne pas alimenter la diode avant
> d'avoir lu [docs/annexes/securite_laser.md](docs/annexes/securite_laser.md).

---

## Documents

| Document | Contenu |
|---|---|
| **[Rapport technique](docs/Rapport_technique.pdf)** | Tout ce qu'il faut pour **reproduire** le système : matériel, câblage, système d'exploitation, firmware, compilation, calibration, mise en route. |
| **[Difficultés techniques](docs/Difficultes_techniques.pdf)** | Les **verrous non triviaux** rencontrés : ceux qui ont été levés (et comment), ceux qui ont été reportés (et pourquoi). À lire avant toute reprise. |
| **[Bibliographie](docs/Bibliographie.pdf)** | Sources du projet (biologie, vision, commande, optique, sécurité laser, matériel), chacune annotée de ce qu'elle a déterminé. Source BibTeX : [`docs/src/vespa.bib`](docs/src/vespa.bib). |
| [Rapport de PFE (soutenance)](docs/Rapport_PFE_2026_VESPA.pdf) | Le rapport rendu à la soutenance (mars 2026). Document historique, conservé tel quel. |

Annexes techniques (formats légers, tenus à jour avec le code) :
[protocole CAN](docs/annexes/protocole_can.md) ·
[câblage](docs/annexes/cablage.md) ·
[nomenclature / BOM](docs/annexes/bom.md) ·
[configuration Jetson](docs/annexes/jetson_setup.md) ·
[sécurité laser](docs/annexes/securite_laser.md) ·
[dette technique](docs/annexes/dette_technique.md)

---

## Architecture

Trois nœuds sur un bus **CAN 2.0B à 500 kbit/s**, plus un système auxiliaire de supervision.

```
                    ┌──────────────────────────────┐
                    │  VISION NODE                 │
                    │  Jetson Orin Nano Super 8 Go │
                    │  2x OV9281 global shutter    │
                    │  Stéréo 60 Hz + Kalman       │──┐
                    └──────────────────────────────┘  │
                                                      │
    ┌─────────────────────────────────┐               │   BUS CAN
    │  MOTION NODE                    │               │   500 kbit/s
    │  NUCLEO-G431RB                  │───────────────┤   11 bits
    │  2x X-NUCLEO-IHM16M1 (STSPIN830)│               │
    │  2x GM5208-12 + AS5048A · FOC   │               │
    └─────────────────────────────────┘               │
                                                      │
    ┌─────────────────────────────────┐               │
    │  COORDINATION NODE   ⚠ ABSENT   │───────────────┘
    │  ESP32 · collimation + tir      │
    │  + Watchdog (jamais implémenté) │
    └─────────────────────────────────┘
```

| Nœud | Cible | Rôle | État |
|---|---|---|---|
| **[vision_node](software/vision_node/)** | Jetson Orin Nano | Acquisition stéréo synchronisée matériellement (60 Hz), détection, triangulation, prédiction de Kalman, émission de la consigne | ✅ Fonctionnel (cible ArUco) |
| **[motion_node](software/motion_node/)** | STM32G431RB | Cinématique inverse, asservissement FOC en boucle fermée des deux axes pan/tilt | ✅ Fonctionnel |
| **[coordination_node](software/coordination_node/)** | ESP32 | Collimation dynamique, séquence de tir, **watchdog** et autorisation de tir | ❌ **Non implémenté** |
| **[common](software/common/)** | tous | Protocole CAN partagé (source unique de vérité) | ✅ Fonctionnel |

---

## État d'avancement

**Ce qui fonctionne, validé sur matériel réel :**

- **Vision.** Paire stéréo OV9281 déclenchée par le **PWM matériel du Tegra** (60 Hz), avec une
  dérive inter-caméras mesurée à **0,000 ms**. Chaîne zéro-copie V4L2 (mmap), suivi de cible,
  triangulation stéréo éparse, filtre de Kalman prédictif, émission des trames CAN.
- **Mouvement.** Asservissement FOC en boucle fermée des deux moteurs *pancake* GM5208-12
  sur encodeurs absolus AS5048A 14 bits. Butées logicielles, filtre de trajectoire,
  arrêt d'urgence matériel latché. Précision théorique 0,022°.
- **Protocole CAN.** Sérialisation/désérialisation partagée, développée sur bus virtuel
  `vcan0` puis **portée sur le bus physique sans changer une ligne**. Le Jetson a reçu
  **111 909 trames du STM32, sans une seule erreur** (`ip -s link show can0`, 21 mars 2026).

**Ce qui ne fonctionne pas, et pourquoi :**

| Verrou | Nature | Détail |
|---|---|---|
| 🟠 **Bus CAN — persistance** | À remettre en service | Le bus **a fonctionné** sur cuivre. Puis une mise à jour a **effacé `can0-pinmux.dtbo` de `/boot`** sans toucher à la ligne `OVERLAYS` qui le référence : le *bootloader* ne trouve rien, **n'émet aucune erreur**, et démarre sans l'*overlay*. `can0` existe toujours, mais ses broches sont `(MUX UNCLAIMED)` et le nœud émet dans le vide. **C'est une remise en état, pas une recherche** — le binaire est dans [`jetson/dts/`](jetson/dts/). Reste aussi à reconfirmer le sens émission après réparation des transceivers. |
| 🔴 **Reconnaissance biologique** | Reporté | Le modèle **VespAI** est entraîné en RGB ; les capteurs sont monochromes. Le portage en Y8 (1 canal) produit un modèle rapide (10,3 ms, 100 fps) mais **inexploitable** : faux positifs massifs, incapacité à distinguer *V. velutina* de *V. crabro*. Remplacé par un **marqueur ArUco** servant de cible de substitution. |
| 🔴 **Tir laser** | Reporté (sécurité) | Aucun tir n'a été effectué. Le *watchdog* qui conditionne l'autorisation de tir n'existe pas. |
| 🔴 **Nœud de coordination** | Non commencé | Collimation dynamique, séquence de tir, watchdog : spécifiés, jamais codés. |

Le détail complet — symptômes, causes racines, correctifs — est dans
**[Difficultés techniques](docs/Difficultes_techniques.pdf)**.

---

## Reprendre le projet : par où commencer

1. Lire **[Difficultés techniques](docs/Difficultes_techniques.pdf)** en entier. Beaucoup de
   pièges de ce système sont invisibles dans le code (transceivers CAN contrefaits, broche
   `PA2` qui coupe la liaison série, impédance interne cachée de 4 kΩ sur le GPIO du Jetson…).
2. Reproduire l'environnement avec le **[Rapport technique](docs/Rapport_technique.pdf)**.
3. Attaquer les chantiers dans cet ordre — c'est l'ordre des dépendances :

   | # | Chantier | Pourquoi d'abord |
   |---|---|---|
   | 1 | **Remettre le CAN physique en service** | Réinstaller `can0-pinmux.dtbo` dans `/boot`, remonter les transceivers avec le correctif 5 V. Le bus a déjà fonctionné : c'est du remontage, pas du débogage. |
   | 2 | **Implémenter le watchdog** (ESP32) | Condition *sine qua non* de toute mise sous tension du laser Classe 4. |
   | 3 | **Constituer un jeu de données monochrome** et réentraîner un détecteur | Rend au système sa fonction biologique ; remplace le marqueur ArUco. |
   | 4 | **Collimation dynamique + essais optiques** | Loi de commande établie, mécanisme spécifié, jamais assemblé ni étalonné. |
   | 5 | **Gestion multi-cibles** | Un seul filtre de Kalman aujourd'hui ; il en faut un par cible + association de données. |

---

## Structure du dépôt

```
project_vespa/
├── docs/                    Rapports (PDF + sources LaTeX), annexes, bibliographie, références
│   ├── src/                 Sources LaTeX + vespa.bib + figures
│   ├── annexes/             Câblage, protocole CAN, BOM, setup Jetson, sécurité, dette technique
│   └── references/          Articles, datasheets, études préparatoires, rapport mk.1 (PDF)
├── hardware/
│   └── mechanical/          Pièces imprimées 3D : CAO (STEP), maillages, moteur — voir son README
├── jetson/                  Configuration système du Jetson (overlays, services CAN, extlinux)
├── simulation/              Scripts MATLAB de dimensionnement (vision + optique laser)
├── software/
│   ├── common/              Protocole CAN partagé (C, portable STM32/ESP32/Linux)
│   ├── vision_node/         Jetson — C++17 / OpenCV / V4L2 / SocketCAN (CMake)
│   ├── motion_node/         STM32G431RB — SimpleFOC / FDCAN (PlatformIO)
│   ├── coordination_node/   ESP32 — non implémenté (spécification uniquement)
│   ├── tools/               Outils de maintenance : calibration stéréo, exposition, viewport
│   └── legacy_mk1_bench/    Banc de caractérisation mk.1 (Arduino Uno) — annexes du rapport PFE
└── tools/                   Scripts utilitaires du dépôt
```

---

## Démarrage rapide

Prérequis : Jetson Orin Nano sous **JetPack 6.2.1**, avec le noyau Arducam et les *overlays*
installés (voir [configuration Jetson](docs/annexes/jetson_setup.md) — cette étape n'est
**pas** optionnelle, les caméras ne fonctionnent pas sans).

```bash
# --- Nœud de vision (Jetson) ---
sudo apt install libopencv-dev libi2c-dev libzmq3-dev cppzmq-dev v4l-utils can-utils
cd software/vision_node && mkdir -p build && cd build
cmake .. && make -j6
sudo ./vision_node                 # accès requis à /sys/class/pwm et /dev/video*

# --- Nœud de mouvement (STM32, depuis le PC de développement) ---
cd software/motion_node
pio run -t upload                  # sonde ST-LINK
pio device monitor -b 115200

# --- Outils de maintenance (Jetson, headless : interface web) ---
cd software/tools/calib_stereo && ./run_calibration.sh      # http://<jetson>:5000
```

> Le nœud de vision et les outils Python se disputent le même matériel. Un verrou
> d'exclusion mutuelle (`/tmp/vespa_hardware.lock`) empêche les deux de toucher le bus I²C
> en même temps : **lancer les deux à la fois échouera volontairement**, ce n'est pas un bug.

---

## Avertissement sécurité

L'effecteur prévu est une diode laser **bleue de 5,5 W — Classe 4 (IEC 60825-1)**. À la densité
de puissance visée (≈ 2 200 W/cm² sur un spot de 0,53 mm), il n'existe **aucune distance sûre
en intérieur** : lésions oculaires irréversibles y compris sur réflexion diffuse, brûlures
cutanées, risque d'incendie. Lunettes **OD7+ (190–550 nm)** et interlock matériel obligatoires.

⚠️ Le mécanisme de collimation dynamique réduit le risque d'**incendie** (seuil d'embrasement
franchi sur ≈ 50 mm autour de la cible), mais **pas** le risque **oculaire**, qui persiste sur
près de **200 m**. Ne pas se croire protégé par la défocalisation.

Le dispositif de tir de ce dépôt est **incomplet et non validé**. Voir
[docs/annexes/securite_laser.md](docs/annexes/securite_laser.md) avant toute manipulation.

---

## Note sur l'usage de l'IA

Ce projet a été développé avec une assistance importante de modèles de langage (rédaction,
débogage, état de l'art), comme documenté dans le préambule du rapport de PFE. Les archives
de conversations ayant mené à la résolution des verrous clés ont été conservées et ont servi
à rédiger le document [Difficultés techniques](docs/Difficultes_techniques.pdf).
Plusieurs verrous majeurs — transceivers CAN contrefaits, impédance cachée du GPIO,
profondeur de champ létale réelle — ont été identifiés **contre** l'avis de l'IA, par la mesure.
