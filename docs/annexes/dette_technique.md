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
`software/vision_node/CMakeLists.txt` imposait `find_package(CUDA REQUIRED)`,
`find_package(VPI 3.0 REQUIRED)` et un test sur `i2c/smbus.h` — vestiges de la voie TensorRT
abandonnée. Le chemin de code actuel (ArUco + OpenCV + V4L2) n'en utilise aucun : il fallait
donc installer CUDA, VPI et libi2c pour que CMake aboutisse, **sans bénéfice**.

**Correctif** : les trois exigences sont retirées ; elles restent **en commentaire** en tête du
`CMakeLists.txt`, à réactiver telles quelles si une reprise réintroduit l'inférence sur GPU.
