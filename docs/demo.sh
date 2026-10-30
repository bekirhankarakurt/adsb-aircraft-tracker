#!/usr/bin/env bash
# Scripted terminal session recorded for docs/demo.gif (run from the repository root).
type_cmd() { printf '$ '; for ((i = 0; i < ${#1}; i++)); do printf '%s' "${1:$i:1}"; sleep 0.03; done; printf '\n'; sleep 0.4; }
type_cmd "./build/adsb-tracker samples/sample.iq"
./build/adsb-tracker samples/sample.iq 2>&1
sleep 1.5
type_cmd "./build/adsb-tracker -v -q samples/reference.hex"
./build/adsb-tracker -v -q samples/reference.hex 2>&1
sleep 1.5
type_cmd "./build/adsb-tracker --no-fix samples/sample.iq | tail -n 1"
./build/adsb-tracker --no-fix samples/sample.iq 2>&1 | tail -n 1
sleep 2
