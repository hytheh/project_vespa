# Reprendre / contribuer au projet V.E.S.P.A.

Ce dépôt est **archivé en l'état** à la fin d'un projet de fin d'études (ENSIL-ENSCI,
promotion 2026) et **ouvert à reprise** par une équipe suivante.

## Avant de commencer

1. Lire **[docs/Difficultes_techniques.pdf](docs/Difficultes_techniques.pdf)** en entier :
   beaucoup de pièges sont invisibles dans le code.
2. Reproduire l'environnement avec le **[Rapport technique](docs/Rapport_technique.pdf)**.
3. Suivre l'ordre des chantiers de reprise donné dans le
   [README](README.md#reprendre-le-projet--par-où-commencer).
4. Consulter la **[dette technique](docs/annexes/dette_technique.md)** : elle liste les
   limites connues (dont la triangulation du `vision_node` et le *watchdog* absent).

## Sécurité — non négociable

Le laser de puissance est de **Classe 4** et n'a jamais été alimenté. Ne pas mettre la diode
sous tension avant d'avoir lu **[docs/annexes/securite_laser.md](docs/annexes/securite_laser.md)**
et implémenté le *watchdog* matériel d'autorisation de tir.

## Style et flux de travail

- Une branche par chantier, une *pull request* par changement cohérent.
- Garder le **protocole CAN** (`software/common/`) comme source unique de vérité : toute
  évolution de trame se fait là, puis se propage aux nœuds.
- Conserver la cohérence entre le code et les annexes Markdown (elles sont tenues à jour
  avec le code).

## Contact

Ouvrir une *issue* sur le dépôt, ou contacter les auteurs via l'ENSIL-ENSCI.
