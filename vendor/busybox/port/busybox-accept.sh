set -e
gucman install busybox
printf 'pear\napple\npear\n' > /tmp/fruit
busybox sort /tmp/fruit | busybox uniq > /tmp/unique
printf 'apple\npear\n' > /tmp/expected
busybox cmp /tmp/unique /tmp/expected
busybox tar cf /tmp/fruit.tar -C /tmp fruit
mkdir /tmp/unpacked
busybox tar xf /tmp/fruit.tar -C /tmp/unpacked
busybox cmp /tmp/fruit /tmp/unpacked/fruit

echo BUSYBOX-ACCEPTANCE-PASS
