# Mécanique — pièces imprimées 3D et modèles CAO

Toutes les pièces mécaniques spécifiques du projet VESPA mk.2 ont été **conçues puis
imprimées en 3D**. Ce dossier est leur source de vérité.

```
hardware/mechanical/
├── cad/        ← modèles CAO au format STEP  ◀── LA RÉFÉRENCE
├── print/      ← maillages prêts à imprimer (3MF / STL)
└── motor/      ← caractérisation du moteur GM5208-12
```

---

## Pourquoi le STEP est la référence, et pas le 3MF

Un fichier **3MF** ou **STL** ne contient qu'un **maillage** : une soupe de triangles. On
peut l'imprimer, on ne peut pas le **modifier**. Le congé est figé, la cote est perdue,
l'intention de conception a disparu.

Un fichier **STEP** (ISO 10303, AP214/AP242) contient la **géométrie exacte** — surfaces,
arêtes, topologie — et s'ouvre dans **n'importe quel logiciel de CAO** (SolidWorks, Fusion,
FreeCAD, Onshape, CATIA). C'est le format d'échange neutre de l'industrie.

**Un repreneur qui n'a que le 3MF ne peut pas faire évoluer la pièce. Avec le STEP, il le
peut.** C'est toute la différence entre un projet transmissible et un projet mort.

> Les fichiers natifs (`.SLDPRT`, `.f3d`, `.FCStd`…) sont les bienvenus **en plus** du STEP,
> jamais **à la place** : ils enferment la géométrie dans un logiciel, souvent dans une
> version donnée de ce logiciel.

---

## Convention de nommage

```
VESPA_<Sous-ensemble>_<Pièce>_<Révision>.step
```

| Champ | Règle | Exemple |
|---|---|---|
| `Sous-ensemble` | Où la pièce se monte | `Turret`, `Camera`, `Optics`, `Killbox`, `Frame` |
| `Pièce` | Ce qu'elle est | `MotorMount`, `Carrier`, `LensHolder` |
| `Révision` | Lettre.chiffre, croissante | `A.1`, `A.2`, `B.1` |

**Exemple :** `VESPA_Camera_Carrier_B_A.1.step`

Une pièce et son maillage portent le **même nom**, seule l'extension change — c'est ce qui
permet de retrouver le STEP d'un 3MF sans se poser de question.

---

## Comment ajouter vos pièces

1. **Exporter en STEP** depuis votre logiciel de CAO (AP214 ou AP242, unités en **mm**).
2. Déposer le `.step` dans **`cad/`**.
3. Déposer le maillage réellement imprimé (`.3mf` / `.stl`) dans **`print/`**, sous le même nom.
4. **Ajouter une ligne au tableau ci-dessous.** C'est ce tableau qui rend le dossier lisible
   — un dossier de STEP sans index est un tas de fichiers.

```bash
git add hardware/mechanical/
git commit -m "Ajout des modèles CAO : <pièces>"
```

> Les formats binaires (`.step`, `.3mf`, `.stl`) sont déclarés dans
> [`.gitattributes`](../.gitattributes) : Git ne tentera pas de les traiter comme du texte
> ni de convertir leurs fins de ligne.
>
> **Au-delà de ~50 Mo par fichier**, passer à [Git LFS](https://git-lfs.com/)
> (`git lfs track "*.step"`) plutôt que de gonfler l'historique.

---

## Index des pièces

<!-- Une ligne par pièce. Compléter au fur et à mesure des imports. -->

| Pièce | Sous-ensemble | Rév. | STEP | Maillage | Rôle |
|---|---|---|---|---|---|
| `ADEL_SK_Carrier_B` | Caméra | A.1 | ⬜ *à importer* | [`print/ADEL_SK_Carrier_B_A.1.3mf`](print/ADEL_SK_Carrier_B_A.1.3mf) | Support de carte porteuse |

**Légende :** ✅ présent · ⬜ à importer

> ⚠️ **Le maillage seul ne suffit pas.** Tant que la colonne *STEP* d'une pièce est à ⬜,
> cette pièce **ne peut pas être modifiée** par un repreneur — seulement réimprimée à
> l'identique.

---

## Paramètres d'impression

<!-- À compléter : matériau, hauteur de couche, remplissage, supports. -->

| Pièce | Matériau | Couche | Remplissage | Supports |
|---|---|---|---|---|
| *(à documenter)* | | | | |

> Les pièces qui portent les moteurs et l'optique subissent des efforts et de la chaleur.
> Le matériau et le taux de remplissage ne sont pas neutres : les consigner évite au
> repreneur de réimprimer trois fois avant de retrouver une pièce rigide.

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
