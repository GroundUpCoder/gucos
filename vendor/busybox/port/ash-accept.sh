set -e
gucman install ash
ash -c 'set -e
f() { echo "$(( $1 + $2 ))"; }
f 17 25 | cat > /tmp/shell-result
test "$(cat /tmp/shell-result)" = 42
status=0
(exit 7) || status=$?
test "$status" = 7'

echo ASH-ACCEPTANCE-PASS
