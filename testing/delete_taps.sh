#!/bin/bash

# Find all network interfaces that start with "tap"
tap_interfaces=$(ip -o link show | awk -F': ' '{print $2}' | grep '^tap')

# Loop through each tap interface and delete it
for iface in $tap_interfaces; {
    echo "Deleting interface: $iface"
    ip link delete $iface
}

echo "All 'tap' interfaces have been deleted."
