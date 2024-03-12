#!/bin/sh
module="char_scull_main"
device="scull"
mode="664"
# Invoke insmod with all arguments we got
# and use a pathname, as newer modutils don't look in . by default
/sbin/insmod ./$module.ko $* || exit 1

# Remove stale nodes
rm -f /dev/${device}0 /dev/${device}1 /dev/${device}2 /dev/${device}3

# Extract the major number
major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices)

# Exit if major number not found
if [ -z "$major" ]; then
    echo "Error: Could not get major number for $module"
    exit 1
fi

# Create device nodes
for i in 0 1 2 3; do
    mknod /dev/${device}$i c $major $i
done

# Determine the correct group and assign it, then set permissions
group="staff"
if ! grep -q '^staff:' /etc/group; then
    group="wheel"
fi

# Change group and permissions for device nodes
for i in 0 1 2 3; do
    chgrp $group /dev/${device}$i
    chmod $mode /dev/${device}$i
done
