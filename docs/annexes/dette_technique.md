# Annexe — Dette technique

Liste des compromis assumés, des raccourcis pris sous contrainte de délai, et des points à
reprendre. Distincte des **verrous** (voir `docs/Difficultes_techniques.pdf`) : ici, on sait
quoi faire, il n'y a pas d'inconnue — seulement du travail.

---

## Ouvert

### `std::system()` pour piloter le PWM
`HardwareSyncController` pilote le PWM par des appels shell
(`std::system("echo ... > /sys/class/pwm/...")`) et `v4l2-ctl` par ligne de commande.

- **Coût** : surcoût système à chaque appel, échecs silencieux (le code de retour de
  `v4l2-ctl` est testé, celui des `echo` ne l'est pas), fragilité.
- **Correctif** : écritures `sysfs` directes en C++ (`std::ofstream`), et `ioctl` V4L2
  natifs plutôt que `v4l2-ctl`.
- **Fichier** : `software/vision_node/src/HardwareSyncController.cpp`

### Chemins absolus
Les `#define CMD_PWM_START` et assimilés référencent des chemins absolus. **Déplacer le
dépôt casse la compilation.**

- **Correctif** : chemins relatifs, ou fichier de configuration, ou variables CMake.

### Échelle d'exposition non homogène entre les modes
L'OV9281 n'utilise **pas le même calcul de temps-ligne** en mode maître et en mode
déclenchement externe (esclave).

- **Conséquence** : l'outil `calib_exposition` (`app_EG.py`) règle l'exposition dans un
  régime différent de celui du pipeline C++ (qui tourne en mode esclave). **Les valeurs ne
  correspondent pas 1:1** — le rendu visuel de l'outil ne prédit pas exactement le rendu en
  production.
- **Correctif** : soit forcer le mode déclenchement externe pendant la calibration, soit
  interroger le mode actif et appliquer un coefficient correctif aux curseurs.
- **Fichier** : `software/tools/calib_exposition/app_EG.py`

### Triangulation du `vision_node` — repère et distorsion à valider sur le banc
`software/vision_node/src/main.cpp` alimente le triangulateur stéréo *épars* avec les
centroïdes ArUco **déjà tournés de 90°** en repère redressé 800×1280. Deux limites subsistent
et doivent être **validées sur le vrai banc** avant de faire confiance à un angle de visée
absolu :

- **Distorsion non corrigée.** Les coefficients `dist_coeff_*` sont définis dans le repère
  brut du capteur, mais `ArUcoTracker::detect()` renvoie des points déjà tournés — les
  appliquer après rotation serait faux. Le chemin épars les ignore donc (comme le test de
  référence `test_stereo_pipeline.cpp`). Une reprise « propre » undistord **avant** la
  rotation, ou passe par `cv::stereoRectify` (voir `sparse_tracker.cpp`, désormais compilé,
  qui fait la géométrie complète K/D/R/T + `triangulatePoints`).
- **Axe de disparité vs. base physique.** Le triangulateur mesure la disparité sur l'axe
  redressé-x, alors que la calibration place la base stéréo (`T`) majoritairement sur
  l'autre axe. Le test synthétique et la calibration réelle supposent des géométries
  différentes : à **reconfirmer sur le montage** avant tout tir.
- **Corrigé au passage** : `main.cpp` configurait le triangulateur avec le point principal
  et la focale en repère **paysage** alors que les points sont en repère **redressé** ; les
  intrinsèques sont maintenant tournées comme `detect()` (focales permutées, `Cx,Cy`
  remappés).

### Suivi mono-cible
`PredictiveTurretSight` porte **un seul** filtre de Kalman. Une nuée de frelons n'est pas
gérée : ni association de données, ni priorisation, ni gestion des occlusions.

- **Correctif** : un filtre par piste + algorithme d'association (Hongrois / GNN), et une
  politique de priorisation des cibles.

### Décalages d'encodeur codés en dur
`software/motion_node/src/main.cpp` :

```cpp
motorPan.sensor_offset  = 4.7500f;
motorTilt.sensor_offset = 4.8179f;
```

Ces valeurs repoussent le passage `0/2π` hors de la plage utile. Elles sont **propres à un
montage mécanique donné** et devront être réétalonnées sur tout nouveau montage
(voir `software/motion_node/Examples/OneTimeInitialisationAngle.cpp`).

### Mesure de courant désactivée
`PA4` est réservée au NSS matériel du SPI1 (sécurité des encodeurs), au détriment du retour
de courant `SENSEW` du moteur 1. L'asservissement est donc **en position sur encodeur seul**,
sans boucle de courant.

- **Conséquence** : pas de limitation de couple par le courant, pas de détection de blocage.
- **Correctif** : arbitrage à rouvrir — déplacer le NSS, ou renoncer au SPI matériel au
  profit d'un SPI logiciel, ou remapper `SENSEW`.
- Voir `docs/annexes/cablage.md`, piège n°2.

---

## Clos

### Timing d'application des contrôles V4L2 ✅
**Symptôme** : les réglages d'exposition et de gain étaient silencieusement ignorés.
**Cause** : les registres du capteur n'écoutent pas tant que les PLL ne sont pas stabilisées.
**Correctif** : `initializeRig()` démarre les flux, **attend 500 ms**, puis applique les
réglages. Ticket clos.

### Nom du contrôle de gain ✅
Le pilote V4L2 attend strictement `analogue_gain` (orthographe britannique). Toute variante
échoue **en silence**. L'exposition fonctionnait, le gain non — d'où un diagnostic
contre-intuitif. Correctif appliqué dans le code et les profils JSON.

### Dépendances CMake exigées mais inutilisées ✅
`software/vision_node/CMakeLists.txt` imposait `find_package(CUDA REQUIRED)` et
`find_package(VPI 3.0 REQUIRED)` — vestiges de la voie TensorRT abandonnée, qu'aucune cible
n'utilise : il fallait installer CUDA et VPI pour que CMake aboutisse, **sans bénéfice**.

**Correctif** : les deux exigences sont retirées ; elles restent **en commentaire** en tête du
`CMakeLists.txt`, à réactiver telles quelles si une reprise réintroduit l'inférence sur GPU.
Le test sur `i2c/smbus.h` est **conservé** : les cibles HAL (`test_camera_hal`,
`test_hardware_sync`, `sparse_tracker`) lient `i2c`.
