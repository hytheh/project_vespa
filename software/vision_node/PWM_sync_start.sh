#!/bin/bash

# Configuration for Jetson Orin Nano Pin 32 (PWM7 -> pwmchip3)
PWM_CHIP="pwmchip3"
PWM_NUM="0"
PWM_DIR="/sys/class/pwm/${PWM_CHIP}"
PWM_PATH="${PWM_DIR}/pwm${PWM_NUM}"

# 60Hz Calculation
# Period = 1 sec / 60 = 0.0166 sec = 16,666,666 ns
PERIOD=16666666
# 10% Duty Cycle = 1,666,666 ns
DUTY=1666666

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
echo "Period set to ${PERIOD} ns"

# 4. Configure Duty Cycle
echo $DUTY > "${PWM_PATH}/duty_cycle"
echo "Duty Cycle set to ${DUTY} ns"

# 5. Enable the Output
echo 1 > "${PWM_PATH}/enable"
echo "PWM signal enabled."