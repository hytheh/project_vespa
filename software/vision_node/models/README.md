# `models/` — moteur d'inférence de la voie abandonnée

## `vespai.engine` (16 Mo)

Moteur **TensorRT sérialisé** issu du portage du modèle **VespAI**
([O'Shea-Wheller et al., 2024](../../../docs/references/articles/OSheaWheller2024_VespAI_CommsBiology.pdf)),
construit sur le Jetson Orin Nano du projet.

> **Ce fichier n'est utilisé par aucun code du dépôt.** Ni le `CMakeLists.txt`, ni
> `vision_node`, ni aucun outil ne le chargent. Il est conservé comme **trace de la voie
> d'inférence explorée puis abandonnée**, pas comme un composant du système livré.

### Pourquoi il n'est pas utilisé

VespAI est entraîné en **RGB** ; les capteurs OV9281 du projet sont **monochromes**. Le
portage en Y8 (1 canal) produit un modèle **rapide** — 10,3 ms par inférence, soit 100 fps,
donc parfaitement compatible avec la cadence de 60 Hz — mais **inexploitable en pratique** :
faux positifs massifs, et incapacité à distinguer *Vespa velutina* de *Vespa crabro*, ce qui
est précisément la discrimination que le système doit faire.

La détection biologique a donc été remplacée par un **marqueur ArUco** servant de cible de
substitution. Voir le README racine et `docs/Difficultes_techniques.pdf`.

### Pourquoi il n'est pas réutilisable tel quel

Un moteur TensorRT sérialisé **n'est pas portable** : il est lié à la version de TensorRT et
à l'architecture GPU qui l'ont produit. Il ne se rechargera pas sur une autre machine, ni
même sur ce Jetson après une mise à jour de JetPack.

**Le modèle source (ONNX / poids) n'est pas au dépôt.** Ce binaire ne permet donc **pas** de
repartir de l'existant.

### Ce qu'il faut faire à la reprise

Ne pas essayer de réutiliser ce fichier. Le chantier est de **constituer un jeu de données
monochrome** et de **réentraîner** un détecteur dessus (chantier n°3 du README racine) — ce
qui rend de toute façon ce moteur caduc.
