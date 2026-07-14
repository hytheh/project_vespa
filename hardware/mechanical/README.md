# Mécanique — CAO et pièces imprimées 3D

Géométrie du prototype VESPA mk.2 : assemblages STEP, maillages imprimés, projets
PrusaSlicer qui les ont produits.

```
hardware/mechanical/
├── cad/            assemblages STEP (AP242, mm)
├── print/          maillages des pièces imprimées (STL / 3MF)
│   └── plates/     projets PrusaSlicer des plateaux lancés
└── motor/          caractérisation du moteur GM5208-12
```

---

## Fichiers éditables

La CAO est faite sous **3DEXPERIENCE R2026x**, sur la **plateforme privée de l'école**.
Chez Dassault Systèmes les objets natifs (`3D Shape`, `Physical Product`) sont des
enregistrements de base de données, pas des fichiers : ils ne sortent pas de la plateforme.

**Les modèles paramétriques — arbre de construction, esquisses, contraintes — sont sur le
3DEXPERIENCE de l'école, et nulle part ailleurs.** La colonne *Item 3DX* des tableaux
ci-dessous donne, pour chaque pièce, le nom sous lequel elle y est enregistrée : c'est la
clé de recherche dans la base.

Ce qui sort de la plateforme, et qui est ici, est l'export **STEP** (ISO 10303 AP242) :
géométrie exacte — surfaces, arêtes, topologie — lisible par SolidWorks, Fusion, FreeCAD,
Onshape, CATIA. Il porte la géométrie, pas l'historique de conception. Les maillages
(`.stl`, `.3mf`) ne portent ni l'un ni l'autre.

---

## Convention de nommage

```
VESPA_<Sous-ensemble>_<Pièce>_<Révision>.<ext>
```

| Champ | Règle | Exemple |
|---|---|---|
| `Sous-ensemble` | Où la pièce se monte | `Turret`, `Frame`, `Link`, `Base`, `Camera`, `Optics` |
| `Pièce` | Ce qu'elle est | `MotorMount`, `FrontShield`, `LinkC` |
| `Révision` | Lettre.chiffre, croissante | `A.1`, `A.2`, `A.3` |

Les assemblages prennent `ASY` comme sous-ensemble ; le suffixe `_WIP` marque un assemblage
inachevé. Les plateaux d'impression sont préfixés `PLATE_`.

Les noms d'origine (préfixe `ADEL_`, espaces, parenthèses) ont été normalisés. Le nom
d'origine est conservé dans la colonne *Item 3DX* : c'est la clé de recherche dans la base
de l'école.

---

## Assemblages — `cad/`

| Fichier | Item 3DX | Rév. | Contenu | État |
|---|---|---|---|---|
| [`VESPA_ASY_FrameBase_A.2.step`](cad/VESPA_ASY_FrameBase_A.2.step) | `ADEL_VESPA_Frame_Base_ASY` | A.2 | **Le démonstrateur présenté** : tourelle 2 axes, 2× GM5208-12, module laser non câblé, 6× roulement 604ZZ. Les caméras n'y sont pas modélisées | Assemblé, monté |
| [`VESPA_ASY_Turret_A.2_WIP.step`](cad/VESPA_ASY_Turret_A.2_WIP.step) | `ADEL_VESPA_ASY_Turret` | A.2 | **Produit complet** : Jetson Orin Nano, 2× OV9281, laser, et le **tube optique** — lentille Ø63 f100 sur bague, portée par l'actionneur linéaire 35 mm : c'est le mécanisme de collimation dynamique | Inachevé, jamais assemblé |
| [`VESPA_ASY_Base_A.1_WIP.step`](cad/VESPA_ASY_Base_A.1_WIP.step) | `ADEL_VESPA_ASY_Base_mk2` | A.1 | Embase du produit complet : moyeu + jambe | Inachevé, jamais imprimé |
| [`VESPA_Bench_BoardPlate_A.1.step`](cad/VESPA_Bench_BoardPlate_A.1.step) | `ADEL_VESPA_Board_Bench` | A.1 | Platine de banc, 160 × 246 × 52 mm, portant les cartes électroniques. Matériel d'établi, hors machine | — |

`VESPA_ASY_Turret_A.2_WIP.step` pèse **83 Mo** : l'essentiel est le modèle fournisseur du
kit de développement Jetson Orin Nano publié par NVIDIA, modélisé jusqu'aux résistances
0402. La géométrie propre à VESPA y est marginale en volume.

`cad/` ne contient **que des assemblages** : il n'existe pas d'export STEP pièce à pièce.
Une pièce seule s'obtient en l'isolant dans l'assemblage, ou depuis 3DEXPERIENCE.

---

## Index des pièces imprimées

Quantités telles que définies dans l'assemblage.

### Châssis 2 axes — `VESPA_ASY_FrameBase_A.2`

| Pièce | Qté | Maillage | Plateau | Item 3DX |
|---|---:|---|---|---|
| `Frame_BaseA` | 1 | [`A.2`](print/VESPA_Frame_BaseA_A.2.stl) | — | `ADEL_VESPA_Frame_BaseA` |
| `Frame_BaseB` | 1 | [`A.2`](print/VESPA_Frame_BaseB_A.2.stl) | — | `ADEL_VESPA_Frame_BaseB` |
| `Frame_JointB2` | 1 | [`A.1`](print/VESPA_Frame_JointB2_A.1.stl) | [`Structural_x4`](print/plates/PLATE_Structural_x4.3mf) | `ADEL_VESPA_Frame_JointB2` |
| `Frame_LinkB1` | 1 | [`A.1`](print/VESPA_Frame_LinkB1_A.1.stl) | — | `ADEL_VESPA_Frame_LinkB1` |
| `Frame_LinkC` | 1 | [`A.3`](print/VESPA_Frame_LinkC_A.3.stl) | — | `ADEL_VESPA_Frame_LinkC` |
| `Frame_LinkA` | 1 | ⬜ absent | — | `ADEL_VESPA_Frame_LinkA` |
| `Link_BracketLeft` | 1 | [`A.1`](print/VESPA_Link_BracketLeft_A.1.stl) | [`Structural_x4`](print/plates/PLATE_Structural_x4.3mf) | `ADEL_VESPA_Link_BracketLeft` |
| `Link_BracketRight` | 1 | [`A.1`](print/VESPA_Link_BracketRight_A.1.stl) | — | `ADEL_VESPA_Link_BracketRight` |
| `Turret_MotorPlate` | 1 | [`A.1`](print/VESPA_Turret_MotorPlate_A.1.stl) | [`Structural_x4`](print/plates/PLATE_Structural_x4.3mf) | `ADEL_VESPA_Turret_MotorPlate` |
| `Turret_BearingPlate` | 1 | [`A.1`](print/VESPA_Turret_BearingPlate_A.1.stl) | — | `ADEL_VESPA_Turret_BearingPlate` |
| `Turret_ShieldLite` | 1 | [`A.1`](print/VESPA_Turret_ShieldLite_A.1.stl) | [`Structural_x4`](print/plates/PLATE_Structural_x4.3mf) | `ADEL_VESPA_Turret_ShieldLite` |

### Tourelle complète — `VESPA_ASY_Turret_A.2_WIP`

| Pièce | Qté | Maillage | Plateau | Item 3DX |
|---|---:|---|---|---|
| `Turret_RailType1` | 2 | [`A.2`](print/VESPA_Turret_RailType1_A.2.3mf) | [`RailType1_x2`](print/plates/PLATE_RailType1_x2.3mf) | `ADEL_VESPA_Turret_Rail_Type1` |
| `Turret_RailType2` | 2 | [`A.2`](print/VESPA_Turret_RailType2_A.2.3mf) | [`RailType2_x2`](print/plates/PLATE_RailType2_x2.3mf) | `ADEL_VESPA_Turret_RailType2` |
| `Turret_XFrame1Top` | 2 | [`A.1`](print/VESPA_Turret_XFrame1Top_A.1.3mf) | — | `ADEL_VESPA_Turret_XFrame1Top` |
| `Turret_FrontShield` | 1 | [`A.1`](print/VESPA_Turret_FrontShield_A.1.3mf) | [`flat`](print/plates/PLATE_FrontShield_flat.3mf) · [`tilted`](print/plates/PLATE_FrontShield_tilted.3mf) | `ADEL_VESPA_Turret_FrontShield` |
| `Turret_MotorMount` | 1 | [`A.1`](print/VESPA_Turret_MotorMount_A.1.3mf) | [`BearingMount_MotorMount`](print/plates/PLATE_BearingMount_MotorMount.3mf) | `ADEL_VESPA_Turret_MotorMount` |
| `Turret_BearingMount` | 1 | ⬜ absent — géométrie dans le plateau | [`BearingMount_MotorMount`](print/plates/PLATE_BearingMount_MotorMount.3mf) | `ADEL_VESPA_Turret_BearingMount` |
| `Turret_LABracket` | 1 | [`A.1`](print/VESPA_Turret_LABracket_A.1.3mf) | — | `ADEL_VESPA_Turret_LABracket` |
| `Turret_LaserMountFront` | 1 | [`A.2`](print/VESPA_Turret_LaserMountFront_A.2.3mf) | — | `ADEL_VESPA_Turret_LaserMountFront` |
| `Turret_CamBracket` | 2 | ⬜ absent | — | `ADEL_VESPA_Turret_CamBracket` |
| `Turret_JetsonHolder_L` | 1 | ⬜ absent | — | `ADEL_VESPA_Turret_JetsonHolder_L` |
| `Turret_JetsonHolder_R` | 1 | ⬜ absent | — | `ADEL_VESPA_Turret_JetsonHolder_R` |
| `LensRing` | 1 | ⬜ absent | — | `ADEL_VESPA_LensRing` |
| `Linear35_CarriageBody` | 1 | ⬜ absent | — | `ADEL_VESPA_Linear35_CarriageBody` |

### Embase — `VESPA_ASY_Base_A.1_WIP`

| Pièce | Qté | Maillage | Plateau | Item 3DX |
|---|---:|---|---|---|
| `Base_Hub` | 1 | [`A.1`](print/VESPA_Base_Hub_A.1.stl) | — | `ADEL_VESPA_Base_Hub` |
| `Base_Leg` | 1 | ⬜ absent | — | `ADEL_VESPA_Base_Leg` |

### Hors assemblage

| Pièce | Maillage | Item 3DX | Statut |
|---|---|---|---|
| `Optics_LensHolderSpacer` | [`A.1`](print/VESPA_Optics_LensHolderSpacer_A.1.3mf) | `ADEL_VESPA_Temporary_LensHolderSpacer` | Cale provisoire de porte-lentille. Pièce d'appoint, n'apparaît dans aucun assemblage |
| `Camera_CarrierB` | [`A.1`](print/VESPA_Camera_CarrierB_A.1.3mf) | `ADEL_SK_Carrier_B` | Support de carte porteuse, 53 × 56 × 4,6 mm. Absent des assemblages A.2 |

**Légende :** ⬜ maillage absent du dépôt. La pièce existe dans le STEP ; son export
d'impression n'a pas été retrouvé.

---

## Paramètres d'impression

Imprimante **Prusa CORE One**, buse **0,4 mm haut débit**, **PLA**, couche **0,2 mm**,
plateau **60 °C**.

Les six fichiers de [`print/plates/`](print/plates/) sont des projets PrusaSlicer complets :
le `.3mf` contient l'orientation de pose, les supports et le profil utilisés.

| Plateau | Pièces | Profil | Remplissage | Périm. | Buse | Supports |
|---|---|---|---|---:|---:|---|
| [`PLATE_Structural_x4`](print/plates/PLATE_Structural_x4.3mf) | `Turret_MotorPlate`, `Turret_ShieldLite`, `Link_BracketLeft`, `Frame_JointB2` | `0.20mm STRUCTURAL` — **Prusament PLA** | gyroïde 25 % | 2 | 230 °C | peints |
| [`PLATE_RailType1_x2`](print/plates/PLATE_RailType1_x2.3mf) | 2× `Turret_RailType1` | `0.20mm SPEED` | gyroïde 25 % | 3 | 225 °C | peints |
| [`PLATE_RailType2_x2`](print/plates/PLATE_RailType2_x2.3mf) | 2× `Turret_RailType2` | `0.20mm SPEED` | gyroïde 25 % | 3 | 225 °C | peints |
| [`PLATE_FrontShield_flat`](print/plates/PLATE_FrontShield_flat.3mf) | `Turret_FrontShield`, pose à plat | `0.20mm SPEED` | gyroïde 25 % | 3 | 225 °C | peints |
| [`PLATE_FrontShield_tilted`](print/plates/PLATE_FrontShield_tilted.3mf) | `Turret_FrontShield`, pose inclinée | `0.20mm SPEED` | gyroïde 25 % | 3 | 225 °C | automatiques |
| [`PLATE_BearingMount_MotorMount`](print/plates/PLATE_BearingMount_MotorMount.3mf) | `Turret_BearingMount`, `Turret_MotorMount` | `0.20mm SPEED` | grille 15 % | 2 | 225 °C | automatiques |

Les deux plateaux `FrontShield` sont deux orientations de pose de la même pièce A.1.

`STRUCTURAL` est le seul profil en Prusament PLA à 230 °C, avec 4 couches pleines en fond ;
il porte les quatre pièces qui reprennent les efforts des moteurs. Les cinq autres plateaux
sont en `SPEED`. *Supports peints* = générés uniquement sur les zones marquées
(`support_material_auto = 0`).

---

## Composants du commerce présents dans la CAO

Modélisés pour l'encombrement et les fixations, **non fabriqués**. Références et prix dans
la [nomenclature](../../docs/annexes/bom.md).

| Composant | Item 3DX | Où |
|---|---|---|
| Moteur *pancake* GM5208-12 | `ADEL_VESPA_5208BLDC_ASY` | FrameBase (×2), Turret (×1) |
| Module laser 5,5 W — corps, carte, lentille, capot acrylique, ventilateur 40×15 | `ADEL_VESPA_ASY_Laser` | FrameBase, Turret |
| Jetson Orin Nano — kit de développement | `ADEL_NvidiaJetsonOrinNano` | Turret |
| Caméra OV9281 UC788B | `ADEL_VESPA_OV9281_CameraModuleUC788B` | Turret (×2) |
| Actionneur linéaire 35 mm — rails Ø3, douilles, moteur pas-à-pas 20 mm | `ADEL_VESPA_ASY_Linear35Actuator` | Turret |
| Lentille asphérique K9 Ø63 f100 | `ADEL_VESPA_Lens63d100f` | Turret |
| Roulements 604ZZ (×6) et 6810 (×1) | `ADEL_VESPA_604ZZ_Bearing`, `ADEL_VESPA_bearing_6810O` | FrameBase, Turret |
| Goupilles 4×20, vis CHc M4×40 | `ADEL_VESPA_HRDW_Pin4x20`, `ADEL_VESPA_Laser_~Vis_Chc_M4x40` | FrameBase |

---

## Moteur

[`motor/GM5208-12.json`](motor/GM5208-12.json) — caractérisation du moteur *pancake*
GM5208-12 au format ST Motor Profiler, exploitable directement par le
*ST Motor Control Workbench*.

| Paramètre | Valeur |
|---|---|
| Type | PMSM *pancake* (gimbal), 14 aimants |
| Paires de pôles | **7** |
| Résistance statorique `Rs` | 7,48 Ω |
| Inductance `Ls` | 4,49 mH |
| Tension nominale | 23,6 V |
| Courant nominal | 1,5 A |
| Constante de FEM | 32,96 |
| Encodeur associé | AS5048A, magnétique absolu **14 bits** (0,022°) |
