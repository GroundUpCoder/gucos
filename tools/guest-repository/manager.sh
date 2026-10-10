#!/bin/sh
# Generic runtime package manager. Catalogs are data; only build executes recipes.
set -eu
json=/usr/libexec/guc-json
fetch=/usr/libexec/guc-repo-fetch
unpack=/usr/libexec/guc-unpack
links=/usr/libexec/guc-links-v2
config=/etc/gucman/sources.d
cache=/var/cache/gucman/v1
state=/usr/local/lib/gucman
runtime=$(cat /etc/gucman/runtime)
work= lock= publishing=
cleanup() { [ -z "$work" ] || rm -rf "$work"; [ -z "$lock" ] || { if [ -z "$publishing" ]; then rm -f "$lock/owner"; rmdir "$lock"; else echo "gucman: publication pending; run gucman recover" >&2; fi; }; }
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
fail() { echo "gucman: $*" >&2; exit 1; }
name_ok() { case "$1" in ''|.|..|[!a-z0-9]*|*[!a-z0-9+.-]*) return 1;; esac; [ "${#1}" -le 100 ]; }
path_ok() { case "$1" in ''|/*|*/|*//*|.|..|./*|../*|*/./*|*/../*|*/.|*/..|*[!a-zA-Z0-9_./+-]*) return 1;; esac; }
url_ok() {
 case "$1" in *[!a-zA-Z0-9_./:+%?=\&~-]*|*'@'*|*'#'*|*'\\'*) return 1;; esac
 case "$1" in https://?*/*|http://127.0.0.1:*/*|http://localhost:*/*) return 0;; *) return 1;; esac
}
hash_ok() { [ "${#1}" -eq 64 ] || return 1; case "$1" in *[!a-f0-9]*) return 1;; esac; }
size_ok() { case "$1" in ''|*[!0-9]*) return 1;; esac; [ "${#1}" -le 9 ] && [ "$1" -gt 0 ] && [ "$1" -le 268435456 ]; }
text() { "$json" string "$@"; }
verified() (
 url=$1; hash=$2; size=$3
 url_ok "$url" && hash_ok "$hash" && size_ok "$size" || fail 'invalid object reference'
 dest=$cache/objects/$hash
 if [ ! -f "$dest" ]; then
  part=$(mktemp "$cache/objects/.fetch.XXXXXX"); trap 'rm -f "$part"' EXIT
  "$fetch" -s -q -m "$size" "$url" "$part" || fail "download failed: $url"
  actual=$(sha256sum "$part"); [ "${actual%% *}" = "$hash" ] || fail 'object checksum mismatch'
  [ "$(wc -c < "$part")" -eq "$size" ] || fail 'object size mismatch'
  mv "$part" "$dest"
 fi
 [ ! -L "$dest" ] || fail 'symlink cache object'
 actual=$(sha256sum "$dest"); [ "${actual%% *}" = "$hash" ] || fail 'cached checksum mismatch'
 [ "$(wc -c < "$dest")" -eq "$size" ] || fail 'cached size mismatch'
 printf '%s\n' "$dest"
)
finish_publication() {
 [ -f "$state/pending/ready" ] || fail 'incomplete preparation; inspect pending journal before recovery'
 pn=$(cat "$state/pending/name"); name_ok "$pn" || fail 'invalid pending name'
 pr=$state/packages/$pn; rel=$(cat "$state/pending/release")
 case "$rel" in "$pr"/release.*) ;; *) fail 'invalid pending release';; esac
 suffix=${rel#"$pr"/release.}; name_ok "$suffix" || case "$suffix" in ''|*[!a-zA-Z0-9]*) fail 'invalid release suffix';; esac
 [ -d "$rel" ] && [ ! -L "$rel" ] || fail 'missing pending release'
 sha256sum -c "$state/pending/checksums" || fail 'pending release metadata changed'
 old=$(cat "$state/pending/old"); current=none
 [ ! -L "$pr/current" ] || current=$(readlink "$pr/current")
 [ "$current" = "$old" ] || [ "$current" = "$rel" ] || fail 'current release differs from pending transaction'
 "$links" check /usr/local "$pr" "$rel/paths"
 "$links" link /usr/local "$pr" "$rel/paths"
 if [ -L "$pr/next" ]; then [ "$(readlink "$pr/next")" = "$rel" ] || fail 'unexpected next link'; else ln -s "$rel" "$pr/next"; fi
 mv -Tf "$pr/next" "$pr/current"
 sync
 rm -rf "$state/pending"
}
mkdir -p "$config" "$cache/objects" "$state/packages"
action=${1:-help}; [ "$#" -eq 0 ] || shift
case "$action" in
 recover)
  [ "$#" -eq 0 ] || fail 'usage: gucman recover'
  [ -d "$state/lock" ] || { echo 'No interrupted transaction'; exit; }
  owner=$(cat "$state/lock/owner") || fail 'missing lock owner; inspect lock manually'
  case "$owner" in ''|*[!0-9]*) fail 'invalid lock owner';; esac
  if kill -0 "$owner" 2>/dev/null; then fail 'transaction owner is still running'; fi
  mkdir "$state/lock/recovery" 2>/dev/null || fail 'recovery already active'
  if [ -d "$state/pending" ]; then finish_publication; fi
  rm -rf "$state/lock"
  echo 'Recovered package transaction'; exit;;
 repo)
  sub=${1:-list}; [ "$#" -eq 0 ] || shift
  case "$sub" in
   list) [ "$#" -eq 0 ] || fail 'usage: gucman repo list'; for f in "$config"/*.url; do [ -f "$f" ] || continue; n=${f##*/}; printf '%s\t%s\n' "${n%.url}" "$(cat "$f")"; done;;
   add) [ "$#" -eq 2 ] || fail 'usage: gucman repo add NAME CATALOG_URL'; name_ok "$1" && url_ok "$2" || fail 'invalid source name or URL'; [ ! -e "$config/$1.url" ] || fail 'source exists; remove it explicitly first'; tmp=$(mktemp "$config/.add.XXXXXX"); printf '%s\n' "$2" > "$tmp"; mv "$tmp" "$config/$1.url";;
   remove) [ "$#" -eq 1 ] && name_ok "$1" || fail 'usage: gucman repo remove NAME'; [ -f "$config/$1.url" ] || fail 'unknown source'; rm "$config/$1.url";;
   *) fail 'usage: gucman repo add|remove|list';;
  esac
  exit;;
 installed) for p in "$state/packages"/*/current/manifest.json; do [ -f "$p" ] || continue; printf '%s\t%s\t%s\n' "$(text "$p" name)" "$(text "$p" version)" "$(cat "${p%/manifest.json}/origin")"; done; exit;;
 list|refresh) [ "$#" -eq 0 ] || fail 'unexpected arguments';;
 search|info|build|install|upgrade) [ "$#" -eq 1 ] || fail "usage: gucman $action [SOURCE:]NAME";;
 *) echo 'usage: gucman repo add|remove|list; list; search TERM; info|build|install|upgrade [SOURCE:]NAME; installed'; exit 1;;
esac
work=$(mktemp -d /tmp/gucman.XXXXXX)
mkdir "$work/catalogs" "$work/plan" "$work/builds" "$work/sources"
cp "$config"/*.url "$work/sources/" 2>/dev/null || :
: > "$work/catalogs.tsv"; : > "$work/order"
failed=0
for f in "$work/sources"/*.url; do
 [ -f "$f" ] || continue
 id=${f##*/}; id=${id%.url}; name_ok "$id" || fail 'invalid source file'
 url=$(cat "$f"); url_ok "$url" || fail 'invalid configured source URL'
 if "$fetch" -s -q -m 8388608 "$url" "$work/catalogs/$id.json" &&
    [ "$("$json" get "$work/catalogs/$id.json" format)" = 1 ] &&
    "$json" keys "$work/catalogs/$id.json" packages > "$work/catalogs/$id.names"; then
  printf '%s\t%s\n' "$id" "$url" >> "$work/catalogs.tsv"
 else echo "gucman: source unavailable or invalid: $id ($url)" >&2; failed=1; fi
 done
if [ "$action" = list ] || [ "$action" = search ] || [ "$action" = refresh ]; then
 while IFS="$(printf '\t')" read -r id url; do
  while IFS= read -r n; do
   name_ok "$n" || fail 'invalid catalog package name'
   row=$id:$n
   if [ "$action" = search ]; then case "$row" in *"$1"*) printf '%s\n' "$row";; esac
   elif [ "$action" != refresh ]; then printf '%s\n' "$row"; fi
  done < "$work/catalogs/$id.names"
 done < "$work/catalogs.tsv"
 exit "$failed"
fi
# Mutations never resolve an incomplete view of the configured sources.
[ "$failed" -eq 0 ] || fail 'restore or remove unavailable sources before resolving packages'
select_package() {
 requested=$1; preferred=$2
 case "$requested" in *:*) chosen=${requested%%:*}; pname=${requested#*:}; name_ok "$chosen" || fail 'invalid source';;
 *) pname=$requested; chosen=$preferred;; esac
 name_ok "$pname" || fail 'invalid package name'
 if [ -n "$chosen" ]; then
  [ -f "$work/catalogs/$chosen.names" ] && grep -Fxq "$pname" "$work/catalogs/$chosen.names" || fail "package not found: $chosen:$pname"
 else
  while IFS="$(printf '\t')" read -r sid surl; do
   if grep -Fxq "$pname" "$work/catalogs/$sid.names"; then [ -z "$chosen" ] || fail "ambiguous package $pname; use SOURCE:$pname"; chosen=$sid; fi
  done < "$work/catalogs.tsv"
  [ -n "$chosen" ] || fail "package not found: $pname"
 fi
 catfile=$work/catalogs/$chosen.json
 manifest=$(verified "$(text "$catfile" packages "$pname" url)" "$(text "$catfile" packages "$pname" sha256)" "$("$json" get "$catfile" packages "$pname" size)")
 [ "$("$json" get "$manifest" format)" = 1 ] && [ "$(text "$manifest" name)" = "$pname" ] || fail 'manifest identity mismatch'
 version=$(text "$manifest" version); name_ok "$version" || fail 'invalid version'
 origin=$(cat "$work/sources/$chosen.url")
}
if [ "$action" = info ]; then select_package "$1" ''; cat "$manifest"; exit; fi
# Subshell recursion avoids clobbering parent resolution variables.
resolve_package() (
 select_package "$1" "$2"
 dir=$work/plan/$pname
 if [ -d "$dir" ]; then
  [ "$(cat "$dir/source")" = "$chosen" ] || fail 'dependency closure mixes origins for the same package'
  [ -f "$dir/done" ] || fail 'dependency cycle'; exit
 fi
 mkdir "$dir"; printf '%s\n' "$chosen" > "$dir/source"; printf '%s\n' "$origin" > "$dir/origin"; cp "$manifest" "$dir/manifest.json"
 "$json" strings "$manifest" dependencies > "$dir/dependencies"
 "$json" rows "$manifest" outputs path mode > "$dir/outputs"
 [ -s "$dir/outputs" ] || fail 'no package outputs'
 : > "$dir/paths"
 while IFS="$(printf '\t')" read -r path mode; do
  path_ok "$path" || fail 'unsafe output path'
  case "$path" in bin/*|sbin/*|include/*|share/*|libexec/*|lib/*) ;; *) fail 'output outside allowed prefix';; esac
  case "$path" in lib/gucman|lib/gucman/*|lib/wasm-posix|lib/wasm-posix/*) fail 'reserved package state';; esac
  case "$mode" in 420|493) ;; *) fail 'invalid output mode';; esac
  grep -Fxq "$path" "$dir/paths" && fail 'duplicate output'
  printf '%s\n' "$path" >> "$dir/paths"
 done < "$dir/outputs"
 while IFS= read -r dep; do resolve_package "$dep" "$chosen"; done < "$dir/dependencies"
 "$json" strings "$manifest" licenses > "$dir/licenses"
 [ -s "$dir/licenses" ] || fail 'package must declare installed license files'
 while IFS= read -r license; do
  case "$license" in share/licenses/"$pname"/*) ;; *) fail 'license must be package-owned';; esac
  grep -Fxq "$license" "$dir/paths" || fail 'license is not a declared output'
 done < "$dir/licenses"
 : > "$dir/done"; printf '%s\n' "$pname" >> "$work/order"
)
resolve_package "$1" ''
lock=$state/lock
if ! mkdir "$lock" 2>/dev/null; then lock=; fail 'package transaction active or interrupted; inspect /usr/local/lib/gucman/lock'; fi
printf '%s\n' "$$" > "$lock/owner"
# Validate ownership for the entire closure before executing any package.
while IFS= read -r name; do
 plan=$work/plan/$name; root=$state/packages/$name
 mkdir -p "$root"
 if [ -L "$root/current" ]; then
  [ "$(cat "$root/current/origin")" = "$(cat "$plan/origin")" ] || fail 'installed package belongs to another source URL'
  sort "$plan/paths" > "$plan/sorted"; sort "$root/current/paths" > "$plan/old-sorted"
  cmp -s "$plan/sorted" "$plan/old-sorted" || fail 'v1 updates must preserve installed path set'
 fi
 "$links" check /usr/local "$root" "$plan/paths" || fail 'package file ownership conflict'
 # Detect conflicts between packages that are not installed yet.
 while IFS= read -r path; do
  for other in "$work/plan"/*; do [ "$other" = "$plan" ] && continue; grep -Fxq "$path" "$other/paths" && fail 'dependency output ownership conflict'; done
 done < "$plan/paths"
done < "$work/order"
while IFS= read -r name; do
 plan=$work/plan/$name; manifest=$plan/manifest.json; root=$state/packages/$name
 digest=$(sha256sum "$manifest"); digest=${digest%% *}
 if [ -f "$root/current/manifest.json" ] && cmp -s "$manifest" "$root/current/manifest.json" && [ "$(cat "$root/current/runtime")" = "$runtime" ]; then echo "Already installed: $name"; continue; fi
 stage=$work/builds/$name.stage
 if [ "$action" = build ]; then
  mkdir "$stage" "$work/builds/$name"; source=$work/builds/$name
  "$json" rows "$manifest" files path url sha256 size > "$plan/files"
  while IFS="$(printf '\t')" read -r path url hash size; do
   path_ok "$path" || fail 'unsafe source path'; [ ! -e "$source/$path" ] || fail 'duplicate input'
   object=$(verified "$url" "$hash" "$size"); mkdir -p "$(dirname "$source/$path")"; cp "$object" "$source/$path"
  done < "$plan/files"
  recipe=$(text "$manifest" build); testscript=$(text "$manifest" test)
  path_ok "$recipe" && path_ok "$testscript" || fail 'unsafe recipe path'
  [ -f "$source/$recipe" ] && [ -f "$source/$testscript" ] || fail 'missing recipe/test'
  echo "Building $(cat "$plan/source"):$name; running trusted recipe with access to your guest files"
  (cd "$source"; export DESTDIR="$stage" PREFIX=/usr/local; sh "$recipe"; sh "$testscript")
 else
  [ "$(text "$manifest" runtime)" = "$runtime" ] || fail 'binary runtime mismatch; use build or a compatible binary'
  object=$(verified "$(text "$manifest" binary url)" "$(text "$manifest" binary sha256)" "$("$json" get "$manifest" binary size)")
  "$unpack" "$object" "$stage"
 fi
 release=$(mktemp -d "$root/release.XXXXXX")
 while IFS="$(printf '\t')" read -r path mode; do
  [ -f "$stage/$path" ] && [ ! -L "$stage/$path" ] || fail "missing or nonregular output: $path"
  # Reject any symlink parent before copying staged bytes.
  parent=${path%/*}; while [ "$parent" != . ]; do [ ! -L "$stage/$parent" ] || fail 'symlink output parent'; case "$parent" in */*) parent=${parent%/*};; *) parent=.;; esac; done
  mkdir -p "$(dirname "$release/$path")"; cp "$stage/$path" "$release/$path"
  case "$mode" in 493) chmod 755 "$release/$path";; *) chmod 644 "$release/$path";; esac
 done < "$plan/outputs"
 cp "$manifest" "$release/manifest.json"; cp "$plan/origin" "$release/origin"; cp "$plan/paths" "$release/paths"
 printf '%s\n' "$runtime" > "$release/runtime"
 # No public files change before all output checks. Existing links refer through
 # current; the rename changes the installed release as a unit.
 publishing=1
 mkdir "$state/pending"
 printf '%s\n' "$name" > "$state/pending/name"
 printf '%s\n' "$release" > "$state/pending/release"
 if [ -L "$root/current" ]; then readlink "$root/current" > "$state/pending/old"; else echo none > "$state/pending/old"; fi
 sha256sum "$release/manifest.json" "$release/paths" "$release/origin" "$release/runtime" > "$state/pending/checksums"
 sync
 : > "$state/pending/ready"
 sync
 finish_publication
 publishing=
 echo "Installed $(cat "$plan/source"):$name $(text "$manifest" version)"
done < "$work/order"
