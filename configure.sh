#!/usr/bin/env bash
#
# configure.sh - set build-time configuration for the iot-thermometer project
#
# Usage:
#   ./configure.sh [--ssid SSID] [--password PASSWORD] [--channel N] [--power DBM]
#
#   --ssid SSID          Wi-Fi network name for the base station (base_station only).
#   --password PASSWORD  Wi-Fi password for the base station (base_station only).
#                         --ssid and --password must be given together.
#   --channel N           nRF24L01+ RF channel, an integer 0-125 (shared by both
#                         boards; the actual frequency is 2400 + N MHz).
#   --power DBM           nRF24L01+ output power in dBm: one of -18, -12, -6, 0
#                         (shared by both boards).
#
# Wi-Fi credentials are applied by (re)configuring the base_station CMake build
# (see base_station/CMakeLists.txt). Radio channel/power are shared settings
# and are applied by editing
# common/nrf24l01/driver_nrf24l01_temperature.h directly - since that file is
# shared, BOTH boards must be rebuilt afterwards for the sensor and base
# station to keep talking to each other on the same channel.
#
# Examples:
#   ./configure.sh --ssid "MyNetwork" --password "hunter2"
#   ./configure.sh --channel 40 --power -6
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RADIO_HEADER="$SCRIPT_DIR/common/nrf24l01/driver_nrf24l01_temperature.h"

usage() {
    sed -n '3,25p' "$0" | sed 's/^# \{0,1\}//'
}

SSID=""
PASSWORD=""
CHANNEL=""
POWER=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ssid)
            SSID="${2:?--ssid requires a value}"
            shift 2
            ;;
        --password)
            PASSWORD="${2:?--password requires a value}"
            shift 2
            ;;
        --channel)
            CHANNEL="${2:?--channel requires a value}"
            shift 2
            ;;
        --power)
            POWER="${2:?--power requires a value}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option '$1'" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$SSID" && -z "$PASSWORD" && -z "$CHANNEL" && -z "$POWER" ]]; then
    usage
    exit 0
fi

if [[ -n "$SSID" || -n "$PASSWORD" ]]; then
    if [[ -z "$SSID" || -z "$PASSWORD" ]]; then
        echo "error: --ssid and --password must be given together" >&2
        exit 1
    fi

    echo "Configuring Wi-Fi credentials for base_station..."
    cmake -S "$SCRIPT_DIR/base_station" -B "$SCRIPT_DIR/base_station/build" -G Ninja \
        -DWIFI_SSID="$SSID" -DWIFI_PASSWORD="$PASSWORD" >/dev/null
    echo "  base_station/build configured. Run: cmake --build base_station/build"
fi

if [[ -n "$CHANNEL" ]]; then
    if ! [[ "$CHANNEL" =~ ^[0-9]+$ ]] || (( CHANNEL > 125 )); then
        echo "error: --channel must be an integer from 0 to 125" >&2
        exit 1
    fi

    echo "Setting nRF24L01+ RF channel to $CHANNEL (2$((400 + CHANNEL)) MHz)..."
    sed -i -E \
        "s/(#define NRF24L01_TEMPERATURE_CHANNEL_FREQUENCY +)[0-9]+U/\1${CHANNEL}U/" \
        "$RADIO_HEADER"
fi

if [[ -n "$POWER" ]]; then
    case "$POWER" in
        -18) POWER_CONST=NRF24L01_OUTPUT_POWER_NEGATIVE_18_DBM ;;
        -12) POWER_CONST=NRF24L01_OUTPUT_POWER_NEGATIVE_12_DBM ;;
        -6)  POWER_CONST=NRF24L01_OUTPUT_POWER_NEGATIVE_6_DBM ;;
        0)   POWER_CONST=NRF24L01_OUTPUT_POWER_0_DBM ;;
        *)
            echo "error: --power must be one of -18, -12, -6, 0 (dBm)" >&2
            exit 1
            ;;
    esac

    echo "Setting nRF24L01+ output power to ${POWER} dBm..."
    sed -i -E \
        "s/^( *)NRF24L01_OUTPUT_POWER_[A-Z0-9_]+$/\1${POWER_CONST}/" \
        "$RADIO_HEADER"
fi

if [[ -n "$CHANNEL" || -n "$POWER" ]]; then
    echo "  $RADIO_HEADER updated. Rebuild BOTH boards for the change to take effect."
fi
