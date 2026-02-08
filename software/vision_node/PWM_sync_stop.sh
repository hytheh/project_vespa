#!/bin/bash

# Configuration for Jetson Orin Nano Pin 32
PWM_CHIP="pwmchip3"
PWM_NUM="0"
PWM_DIR="/sys/class/pwm/${PWM_CHIP}"
PWM_PATH="${PWM_DIR}/pwm${PWM_NUM}"

echo "Stopping PWM on ${PWM_CHIP} Channel ${PWM_NUM}..."

if [ -d "$PWM_PATH" ]; then
    # 1. Disable the output
    echo 0 > "${PWM_PATH}/enable"
    echo "PWM disabled."

    # 2. Unexport the channel (Clean up resources)
    echo $PWM_NUM > "${PWM_DIR}/unexport"
    echo "Channel unexported."
else
    echo "PWM channel was not active."
fi