# Annexe — Sécurité laser

> **À lire intégralement avant toute manipulation de l'effecteur.**
> Ce document n'est pas une formalité administrative.

---

## 1. Ce qu'il faut savoir en dix secondes

| | |
|---|---|
| **Source** | Diode laser **bleue, 5,5 W** (nomenclature) — dimensionnement théorique conduit à 5 W / 460 nm |
| **Classe** | **4** (IEC 60825-1) — la classe la plus dangereuse |
| **Irradiance visée** | ≈ **2 200 W/cm²** sur un spot de **0,53 mm** |
| **État** | **La diode n'a jamais été mise sous tension.** |

> ### 🔴 La diode n'a jamais été alimentée, et ce n'était pas un oubli
>
> Le *watchdog* matériel qui conditionne l'autorisation de tir **n'a jamais été
> implémenté**. Mettre la diode sous tension sans lui aurait été une faute, pas une prise
> de risque calculée : rien n'aurait garanti l'extinction du faisceau si un nœud s'était
> figé.
>
> **Ne pas alimenter cette diode tant que le watchdog n'est pas opérationnel et testé.**

---

## 2. Pourquoi la Classe 4 change tout

Un laser de Classe 4 n'est pas « un laser puissant ». C'est une catégorie où **les
protections habituelles cessent de s'appliquer** :

- **La réflexion diffuse suffit à blesser.** Il n'est pas nécessaire de regarder le
  faisceau : la lumière renvoyée par un mur mat, une vis, une table peut causer une lésion
  rétinienne irréversible. Le réflexe palpébral (0,25 s) est **trop lent**.
- **Brûlure cutanée instantanée** à l'impact.
- **Risque d'incendie** sur toute matière sèche.
- **Il n'existe aucune distance sûre en intérieur.**

À 460 nm (bleu profond), le rayonnement est **très efficacement absorbé** — c'est
précisément ce qui est recherché pour la neutralisation (couplage optimal avec la cuticule),
et c'est ce qui le rend dangereux pour la rétine, qui est transparente à cette longueur
d'onde jusqu'au fond de l'œil.

---

## 3. Le résultat le plus important de l'étude optique

Le mécanisme de **collimation dynamique** a été conçu pour dégrader volontairement le
faisceau hors du point d'impact. Le tracé de densité de puissance
(`simulation/optique/Laser_Optics_v1.m`) donne **deux résultats opposés** :

| | Portée du danger |
|---|---|
| 🟢 Seuil d'**embrasement** de matières sèches | **≈ 50 mm** de part et d'autre de la cible |
| 🔴 Seuil de danger **oculaire** | **≈ 200 mètres** |

**La collimation dynamique réduit le risque d'incendie. Elle ne réduit PAS le risque
oculaire.**

C'est contre-intuitif et c'est le piège : on pourrait croire, en voyant le faisceau
« défocalisé », que le dispositif est intrinsèquement sûr hors de la killbox. **Il ne l'est
pas.** Un faisceau défocalisé de 5,5 W reste très au-dessus du seuil de lésion rétinienne
sur des distances considérables.

---

## 4. Protections obligatoires

- **Lunettes de protection.** Le matériel acheté est un modèle **OD7+ CE, 190–550 nm**
  (nomenclature). Elles doivent être **portées par toute personne présente**, pas seulement
  par l'opérateur — la réflexion diffuse ne distingue pas les rôles.
- **Interlock matériel.** Coupure de l'**alimentation générale** (pas une inhibition
  logicielle du tir). Le watchdog est prévu sur un cœur CPU dédié du nœud de tir, au plus
  près de l'organe dangereux.
- **Arrêt d'urgence** accessible, latché, à réarmement manuel.
- **Confinement.** Aucun essai en espace ouvert ou traversable. Fond absorbant (pas de
  surface métallique, pas de verre) dans l'axe du tir.
- **Détection de renversement** et **détection de présence humaine** : identifiées comme
  nécessaires dans le rapport de soutenance, **jamais implémentées**.

---

## 5. Chaîne de sécurité — état réel

| Élément | Rôle | État |
|---|---|---|
| Arrêt d'urgence `motion_node` (`PC13`) | Coupe les étages de puissance moteurs, latché | ✅ Implémenté |
| *Heartbeats* CAN (`HB_*`) | Signalent l'état de chaque nœud | ✅ Implémenté (protocole) |
| **Watchdog** (nœud de coordination) | **Écoute les heartbeats, coupe l'alimentation** | ❌ **Absent** |
| **Interlock laser** | **Autorise/interdit le tir** | ❌ **Absent** |
| Détection de renversement | Mise en sécurité si le châssis bascule | ❌ Absent |
| Détection de présence humaine | Inhibition du tir | ❌ Absent |

**Quatre des six maillons de la chaîne de sécurité n'existent pas.** L'arrêt d'urgence
existant ne protège que la mécanique — il ne coupe pas le laser, puisque le laser n'est pas
câblé.

---

## 6. Conditions minimales avant un premier tir

Aucun tir ne doit avoir lieu avant que **toutes** les conditions suivantes soient réunies :

1. **Watchdog implémenté et testé** — y compris ses cas de défaut (nœud muet, nœud en
   `FAULT`, câble CAN débranché en cours de tir).
2. **Interlock matériel** coupant l'alimentation générale, vérifié par mesure.
3. **Discrimination d'espèce rétablie.** Le système ne sait actuellement **pas** distinguer
   *V. velutina* (invasive) de *V. crabro* (native, protégée) : il suit un marqueur ArUco.
   **Tirer sans discrimination serait à la fois inutile et écologiquement contraire à
   l'objectif du projet.**
4. **Environnement confiné**, lunettes OD7+ pour tous, absence de tiers.
5. Essais **statiques** d'abord (cible fixe, faible puissance si la diode le permet), avant
   tout essai sur cible mobile.

---

## 7. Références

- **IEC 60825-1:2014** — *Safety of laser products, Part 1: Equipment classification and
  requirements*. Norme de classification. → `docs/Bibliographie.pdf`
- Rapport de soutenance, §10 « Dispositif de tir laser » — dimensionnement thermophysique
  et calcul d'irradiance.
- `docs/Difficultes_techniques.pdf`, § « La profondeur de champ létale » — analyse du
  résultat des 200 m.

### Origine des valeurs biologiques du dimensionnement

Ajouté en juillet 2026 : le dimensionnement s'appuyait sur des valeurs biologiques dont
**aucune source n'était citée**. Elles le sont désormais dans `docs/src/vespa.bib` :

| Valeur | Statut | Source |
|---|---|---|
| **Longueur d'onde bleue** (≈ 460 nm), « couplage cuticule » | ✅ **Fondée** — le visible exige bien moins d'exposition que l'IR (LD90 1,6 J/cm² à 445 nm contre 12,9 J/cm² à 1064 nm) | Keller *et al.* 2020, *Sci. Rep.* **10**:14795 |
| **Durée d'exposition** 20 ms | ✅ **Corroborée** — ≈ 25 ms identifié comme durée idéale | Keller *et al.* 2020 |
| **Limite thermique létale** ≈ 45 °C | ✅ **Fondée**, mesurée sur *V. velutina nigrithorax* | Ruiz-Cristi *et al.* 2020, *PLoS ONE* **15**(10):e0239742 |
| **Fluence létale 50 J/cm²** | ❌ **NON SOURCÉE** — hypothèse, pas une mesure | — |

> ### ⚠️ Les 50 J/cm² sont une hypothèse, et elle est orientée du mauvais côté
>
> **Aucune mesure de létalité laser n'existe pour un frelon.** La seule LD90 publiée dans
> le bleu vaut **1,6 J/cm²** — sur un moustique, deux ordres de grandeur plus léger. La
> valeur retenue par le projet est **~30× plus élevée**, par extrapolation jamais écrite.
>
> Ce n'est pas un détail bibliographique : **c'est ce chiffre qui commande la puissance**,
> donc l'irradiance de 2 200–2 500 W/cm², donc la **Classe 4** et les **200 m** de danger
> oculaire. Surestimer la fluence létale est prudent pour l'efficacité et **dangereux pour
> les personnes**. Si la valeur réelle est plus basse, le dispositif est plus puissant —
> donc plus dangereux — qu'il n'a besoin de l'être.
>
> **À refaire avant tout tir :** dose létale calculée à partir du seuil thermique (45 °C,
> Ruiz-Cristi 2020) et de la masse/surface du thorax (Sipos 2025), par la méthode de
> Keller *et al.* 2016 (`doi:10.1038/srep20936`).
