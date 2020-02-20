# Running these lines is necessary for UDP buffer sizes close to 64k
sudo -s -H
echo 83886080 > /proc/sys/net/core/wmem_max

