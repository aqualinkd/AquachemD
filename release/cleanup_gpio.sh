#!/bin/bash
#
# cleanup_gpio.sh — force all GPIO relays to OFF state
# Usage: cleanup_gpio.sh /etc/aquachemd.conf
#
# Designed to be used on crash/core dump/kill -9 as a last resort to make sure GPIO relays are off(ie doser)
#
# [Service]
# ExecStart=/usr/bin/aquachemd --config /etc/aquachemd.conf
# ExecStopPost=/usr/bin/cleanup_gpio.sh /etc/aquachemd.conf
#
#
#########################################################################
#
# SERVICE_RESULT  — "success", "exit-code", "signal", "core-dump" etc.
# EXIT_CODE       — "exited" or "killed"
# EXIT_STATUS     — the signal number or exit code
#

[[ "$SERVICE_RESULT" == "success" || "$SERVICE_RESULT" == "exit-code" ]] && exit 0  # clean shutdown, aquachemd handled it

echo "cleanup_gpio: abnormal exit (${SERVICE_RESULT}/${EXIT_CODE}/${EXIT_STATUS}), forcing relays off"



CONFIG="${1:-/etc/aquachemd.conf}"

if [[ ! -f "$CONFIG" ]]; then
    echo "cleanup_gpio: config not found: $CONFIG" >&2
    exit 1
fi

# Read the config once, process doser blocks
# Reset state variables at each new doser_label
label="" type="" pin="" mode="" 

release_if_gpio() {
    [[ "$type" != "gpio" || -z "$pin" ]] && return

    # de-asserted = relay OFF
    # active_low  → physical 1 = de-asserted
    # active_high → physical 0 = de-asserted
    local off_val=0
    [[ "$mode" == "active_low" ]] && off_val=1

    echo "cleanup_gpio: '${label}' pin ${pin} (${mode}) → setting to ${off_val} (OFF)"
    gpioset --chip /dev/gpiochip0 "${pin}=${off_val}"
}

while IFS='=' read -r key val; do
    # Strip whitespace and comments
    key=$(echo "$key" | tr -d '[:space:]')
    val=$(echo "$val" | sed 's/#.*//' | tr -d '[:space:]')

    [[ -z "$key" || "$key" == \#* ]] && continue

    case "$key" in
        doser_label)
            # New block — flush previous if it was a gpio doser
            release_if_gpio
            label="$val" type="" pin="" mode=""
            ;;
        doser_type)  type="$val" ;;
        doser_pin)   pin="$val"  ;;
        doser_pin_mode) mode="$val" ;;
    esac
done < "$CONFIG"

# Flush the last block
release_if_gpio

echo "cleanup_gpio: done"