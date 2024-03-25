#!/bin/sh
# $Id: scull_load,v 1.4 2004/11/03 06:19:49 rubini Exp $
module="scull_p"
device="scullpipe"
mode="664"

# Group: since distributions do it differently, look for wheel or use staff
if grep -q '^staff:' /etc/group; then
    group="staff"
else
    group="wheel"
fi

# invoke insmod with all arguments we got
# and use a pathname, as insmod doesn't look in . by default
/sbin/insmod ./$module.ko $* || exit 1

# retrieve major number
major=$(grep -w "$module" /proc/devices | cut -d" " -f1)

rm -f /dev/${device}[0-3]
mknod /dev/${device}0 c $major 0 || exit 1
mknod /dev/${device}1 c $major 1 || exit 1
mknod /dev/${device}2 c $major 2 || exit 1
mknod /dev/${device}3 c $major 3 || exit 1

ln -sf ${device}pipe0 /dev/${device}pipe
chgrp $group /dev/${device}pipe[0-3]
chmod $mode  /dev/${device}pipe[0-3]

echo "Module $module loaded successfully" >> /var/log/scull_pipe.log