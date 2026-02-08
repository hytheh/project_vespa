#!/bin/bash

# Configuration for Jetson Orin Nano Pin 32 (PWM7 -> pwmchip3)
PWM_CHIP="pwmchip3"
PWM_NUM="0"
PWM_DIR="/sys/class/pwm/${PWM_CHIP}"
PWM_PATH="${PWM_DIR}/pwm${PWM_NUM}"

# 80Hz Calculation
# Period = 1 sec / 80 = 0.0125 sec = 12,500,000 ns
PERIOD=12500000
# 50% Duty Cycle = 12,500,000 / 2 = 6,250,000 ns
DUTY=6250000

echo "Starting PWM on ${PWM_CHIP} Channel ${PWM_NUM}..."

# 1. Export the channel if it doesn't exist
if [ ! -d "$PWM_PATH" ]; then
    echo "Exporting channel..."
    echo $PWM_NUM > "${PWM_DIR}/export"
    sleep 0.5  # Give the system a moment to create the files
fi

# 2. Disable first to allow configuration changes
# We suppress errors here in case it's already disabled or uninitialized
echo 0 > "${PWM_PATH}/enable" 2>/dev/null

# 3. Configure Period (Must be done before Duty Cycle)
echo $PERIOD > "${PWM_PATH}/period"
echo "Period set to ${PERIOD} ns (80 Hz)"

# 4. Configure Duty Cycle
echo $DUTY > "${PWM_PATH}/duty_cycle"
echo "Duty Cycle set to ${DUTY} ns (50%)"

# 5. Enable the Output
echo 1 > "${PWM_PATH}/enable"
echo "PWM signal enabled."