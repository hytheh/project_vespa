# Annexe — Configuration du Jetson Orin Nano

> Cette étape **n'est pas optionnelle** : sans le noyau Arducam et les *overlays*, les
> caméras ne fonctionnent pas du tout.

---

## ⚠️ La règle numéro un

> ### Ne jamais exécuter `jetson-io.py`
>
> L'outil NVIDIA `sudo /opt/nvidia/jetson-io/jetson-io.py` régénère **globalement** les
> *overlays* d'arbre de périphériques. Il **écrase** la configuration existante au lieu de
> la compléter.
>
> Le lancer et sauvegarder **détruira la synchronisation stéréo** (l'overlay PWM), et
> possiblement la configuration caméra. C'est la raison pour laquelle toute la configuration
> matérielle de ce projet est portée par des *overlays* explicites et versionnés.

---

## 1. Système de base

1. Flasher **JetPack 6.2.1** (L4T r36.4) sur le Jetson Orin Nano Super 8 Go, via NVIDIA SDK
   Manager. La configuration d'origine a la racine sur **NVMe**.
2. Installer le **noyau et les pilotes Arducam** pour l'OV9281 (procédure du constructeur).
   Le noyau dédié est installé dans `/boot/arducam/`.

## 2. Overlays d'arbre de périphériques

Trois *overlays* sont chargés au démarrage. Les trois binaires sont dans
[`jetson/dts/`](../../jetson/dts/), avec les sources `.dts` des deux *overlays* propres au
projet.

| Overlay | Rôle | État |
|---|---|---|
| `tegra234-p3767-camera-p3768-arducam-dual.dtbo` | Double caméra OV9281 (un bus I²C par port CSI) | ✅ Fonctionne |
| `jetson-io-hdr40-user-custom.dtbo` | **PWM de synchronisation** — broches 32 et 33 | ✅ Fonctionne — **ne pas modifier** |
| `can0-pinmux.dtbo` | Routage du CAN0 (« CAN0 Hardware Override V5 »), broches 29/31 | ✅ Fonctionne |

### Contenu réel de `jetson-io-hdr40-user-custom.dtbo`

Source :
[`jetson/dts/jetson-io-hdr40-user-custom.dts`](../../jetson/dts/jetson-io-hdr40-user-custom.dts),
décompilée du binaire par `dtc -I dtb -O dts` (Jetson-IO ne produit pas de `.dts`).
L'*overlay* ne configure **que** deux broches :

```
hdr40-pin32  ->  soc_gpio19_pg6
hdr40-pin33  ->  soc_gpio21_ph0
```

**Il ne contient aucune configuration CAN.** Ce point est important : une hypothèse
répandue en fin de projet voulait que ce fichier contienne une mauvaise affectation CAN.
C'est faux. Ne pas perdre de temps à le « réparer » — et surtout ne pas le modifier, car
il porte le PWM de synchronisation.

## 3. `extlinux.conf`

Fichier de référence : [`jetson/extlinux.conf.reference`](../../jetson/extlinux.conf.reference).
L'entrée active (`DEFAULT JetsonIO`) :

```
LABEL JetsonIO
    MENU LABEL Custom Header Config: <CSI Camera ARDUCAM Dual>
    LINUX /boot/arducam/Image
    FDT   /boot/arducam/dts/dtb/tegra234-p3768-0000+p3767-0005-nv-super.dtb
    INITRD /boot/initrd
    APPEND ${cbootargs} root=PARTUUID=<...> rw rootwait rootfstype=ext4 ...
    OVERLAYS /boot/arducam/dts/tegra234-p3767-camera-p3768-arducam-dual.dtbo,/boot/jetson-io-hdr40-user-custom.dtbo,/boot/can0-pinmux.dtbo
```

> Adapter le `PARTUUID` à votre disque.

## 4. Vérifications

```bash
# Caméras : deux nœuds doivent apparaître
ls /dev/video*                    # -> /dev/video0  /dev/video1
v4l2-ctl --list-devices

# PWM de synchronisation
ls /sys/class/pwm/pwmchip3/

# CAN physique : overlay présent et broches routées
find /boot -name "*can*.dtbo"
#   -> /boot/can0-pinmux.dtbo ; sinon recopier depuis jetson/dts/
sudo bash -c "cat /sys/kernel/debug/pinctrl/*/pinmux-pins | grep -i paa"
#   -> pin 0 (CAN0_DOUT_PAA0): c310000.mttcan
```

## 5. Synchronisation stéréo (PWM)

Les deux caméras sont forcées en **mode esclave** (`trigger_mode=1`) et reçoivent le même
front de déclenchement, généré par le **PWM matériel du Tegra** — indépendant de la charge
CPU. C'est ce qui garantit une simultanéité *par construction*.

```bash
# 60 Hz, rapport cyclique 10 %
echo 0        > /sys/class/pwm/pwmchip3/export
echo 16666666 > /sys/class/pwm/pwmchip3/pwm0/period      # ns  -> 60 Hz
echo 1666666  > /sys/class/pwm/pwmchip3/pwm0/duty_cycle  # ns  -> 10 %
echo 1        > /sys/class/pwm/pwmchip3/pwm0/enable
```

C'est exactement ce que fait `HardwareSyncController::armTrigger()`. Dérive inter-caméras
mesurée : **0,000 ms**.

## 6. Bus CAN

### Virtuel (`vcan0`) — pour développer sans matériel

Le module est chargé au démarrage par [`jetson/systemd/vcan.conf`](../../jetson/systemd/vcan.conf) :

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
candump vcan0
```

`vcan0` est indiscernable d'un bus réel du point de vue de SocketCAN : le code applicatif est
identique.

### Physique (`can0`) — a fonctionné, à remettre en service

Le service [`jetson/systemd/can0-setup.service`](../../jetson/systemd/can0-setup.service)
est prêt (500 kbit/s, `restart-ms 500`).

**Le bus a réellement fonctionné** : le 21 mars 2026, le Jetson a reçu **111 909 trames du
STM32, sans une seule erreur** (`RX: 772684 bytes, 111909 packets, 0 errors`). Le *pinmux*,
le câblage, la terminaison et le *bit timing* étaient donc corrects.

**Câblage** — ⚠️ dépend de l'authenticité du transceiver (voir [`bom.md`](bom.md)) :

| WCMCU-230 | En-tête 40 broches | Remarque |
|---|---|---|
| Alim. | broche 1 (3,3 V) **ou** broche 2 (5 V) | **5 V si le module est un clone** |
| `GND` | broche 6 | |
| `CTX` | broche **31** (CAN0_TX) | direct — le 3,3 V du Jetson suffit |
| `CRX` | broche **29** (CAN0_RX) | **pont diviseur 1 kΩ / 2 kΩ si clone 5 V** |

**Ce qui reste à faire.** Remonter les transceivers selon le câblage ci-dessus, puis
valider l'émission Jetson → STM32 (seule la réception l'a été).

> ⚠️ **Une mise à jour de JetPack peut supprimer le `.dtbo` sans toucher à la ligne
> `OVERLAYS` qui le référence.** Le *bootloader* n'émet alors **aucune erreur** : `can0`
> existe toujours, mais ses broches ne sont plus routées et le bus est muet. **C'est ainsi
> que le bus a été perdu.** Revérifier après chaque mise à jour.

Analyse complète et solution de repli (contrôleur CAN externe sur SPI) dans
**`docs/Difficultes_techniques.pdf`**.
