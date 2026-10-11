#!/bin/sh
# Package state format 2. Sourced only by the bundle-owned locked manager.
# Public links retain v1 spelling; each package/current goes through one selector.
vstate=$state/v2
v2_active() { [ -f "$vstate/format" ] && [ "$(cat "$vstate/format")" = 2 ]; }
v2_generation_ok() {
 case "$1" in "$vstate"/generations/g.*) ;; *) return 1;; esac
 local suffix=${1#"$vstate"/generations/g.}
 case "$suffix" in ''|*[!a-zA-Z0-9]*) return 1;; esac
 [ -d "$1" ] && [ ! -L "$1" ]
}
v2_release_ok() {
 name_ok "$1" || return 1
 case "$2" in "$state/packages/$1"/release.*) ;; *) return 1;; esac
 local suffix=${2#"$state/packages/$1"/release.}
 case "$suffix" in ''|*[!a-zA-Z0-9]*) return 1;; esac
 [ -d "$2" ] && [ ! -L "$2" ] && [ -f "$2/manifest.json" ]
}
v2_receipt() {
 local vr=$1
 [ ! -e "$vr/payload.sha256" ] && [ ! -L "$vr/payload.sha256" ] || fail 'release integrity receipt already exists'
 sed "s|^|$vr/|" "$vr/paths" > "$vr/.checksum-paths"
 sha256sum "$vr/manifest.json" "$vr/paths" "$vr/origin" "$vr/runtime" "$vr/dependency-hashes" > "$vr/.payload.sha256"
 xargs -r -n 100 sha256sum < "$vr/.checksum-paths" >> "$vr/.payload.sha256"
 rm "$vr/.checksum-paths"
 mv "$vr/.payload.sha256" "$vr/payload.sha256"
 printf '2\n' > "$vr/managed-format"
}
v2_validate() {
 local vg=$1 vn vr extra previous= vcount=0 dep depname depmanifest dephash oldhash
 v2_generation_ok "$vg" || fail 'invalid generation'
 [ "${2:-}" = preparing ] || [ -f "$vg/ready" ] || fail 'incomplete generation'
 sha256sum -c "$vg/metadata.sha256" > "$vg/verification.log" || fail 'generation metadata changed'
 set --
 while IFS="$(printf '\t')" read -r vn vr extra; do
  [ -n "$vn" ] && [ -z "$extra" ] && v2_release_ok "$vn" "$vr" || fail 'invalid generation selection'
  [ "$previous" != "$vn" ] || fail 'duplicate generation package'; previous=$vn
  [ "$(readlink "$vg/packages/$vn")" = "$vr" ] || fail 'generation link differs from selection'
  [ "$(text "$vr/manifest.json" name)" = "$vn" ] || fail 'release identity differs from selection'
  sha256sum -c "$vr/payload.sha256" > "$vg/$vn.verification.log" || fail 'retained package files changed'
  "$json" strings "$vr/manifest.json" dependencies > "$vg/$vn.dependencies"
  while IFS= read -r dep; do
   depname=${dep#*:};name_ok "$depname" || fail 'invalid retained dependency'
   depmanifest=$vg/packages/$depname/manifest.json
   [ -f "$depmanifest" ] || fail "generation dependency missing: $vn -> $depname"
   dephash=$(sha256sum "$depmanifest");dephash=${dephash%% *}
   "$json" dependency "$vr/manifest.json" "$dep" "$(text "$depmanifest" version)" "$dephash" || fail "generation dependency incompatible: $vn"
   oldhash=$(awk -v name="$depname" '$1==name {print $2; exit}' "$vr/dependency-hashes")
   [ -z "$oldhash" ] || [ "$oldhash" = "$dephash" ] || fail "retained dependency changed; reinstall or remove consumer before source rebuilding: $vn"
  done < "$vg/$vn.dependencies"
  set -- "$@" "$vr/paths"; vcount=$((vcount + 1))
 done < "$vg/selection"
 [ "$vcount" -eq "$(wc -l < "$vg/selection")" ] || fail 'unterminated generation selection'
 [ "$#" -eq 0 ] || "$links" closure "$@" || fail 'generation ownership conflict'
}
v2_new() {
 local input=$1 vg vn vr extra
 vg=$(mktemp -d "$vstate/generations/g.XXXXXX")
 printf '2\n' > "$vg/managed-format"
 mkdir "$vg/packages"
 sort "$input" > "$vg/selection"
 date +%s > "$vg/created"
 : > "$vg/metadata.sha256"
 while IFS="$(printf '\t')" read -r vn vr extra; do
  [ -n "$vn" ] && [ -z "$extra" ] && v2_release_ok "$vn" "$vr" || fail 'invalid proposed generation'
  ln -s "$vr" "$vg/packages/$vn"
  sha256sum "$vr/payload.sha256" >> "$vg/metadata.sha256"
 done < "$vg/selection"
 sha256sum "$vg/selection" "$vg/created" >> "$vg/metadata.sha256"
 v2_validate "$vg" preparing
 : > "$vg/ready"
 printf '%s\n' "$vg"
}
v2_select() {
 local target=$1
 if [ -L "$vstate/next" ]; then [ "$(readlink "$vstate/next")" = "$target" ] || fail 'unexpected generation next link'
 else [ ! -e "$vstate/next" ] || fail 'unmanaged generation next path'; ln -s "$target" "$vstate/next"; fi
 mv -Tf "$vstate/next" "$vstate/current"
 sync
}
v2_view() {
 local vn=$1 old=${2:-} root=$state/packages/$1 target=$vstate/current/packages/$1
 mkdir -p "$root"
 if [ -L "$root/current" ]; then
  local existing=$(readlink "$root/current")
  [ "$existing" != "$target" ] || return
  [ -n "$old" ] && [ "$existing" = "$old" ] || fail 'unexpected package compatibility link'
 else [ ! -e "$root/current" ] || fail 'unmanaged package compatibility path'; fi
 if [ -L "$root/v2-next" ]; then [ "$(readlink "$root/v2-next")" = "$target" ] || fail 'unexpected package next link'
 else [ ! -e "$root/v2-next" ] || fail 'unmanaged package next path'; ln -s "$target" "$root/v2-next"; fi
 mv -Tf "$root/v2-next" "$root/current"
}
v2_conversion_finish() {
 local vg vn vr
 [ -f "$vstate/converting/ready" ] || fail 'incomplete package conversion preparation'
 vg=$(cat "$vstate/converting/generation"); v2_validate "$vg"
 # ready is durable before this fence is created. A crash between mkdir and
 # marker write may leave this empty directory; the conversion journal owns it.
 [ ! -f "$state/pending/ready" ] || fail 'legacy transaction appeared during conversion'
 mkdir -p "$state/pending"
 printf '2\n' > "$state/pending/format-fence"
 sync
 if [ -L "$vstate/current" ]; then [ "$(readlink "$vstate/current")" = "$vg" ] || fail 'unexpected conversion selector'
 else [ ! -e "$vstate/current" ] || fail 'unmanaged generation selector'; v2_select "$vg"; fi
 while IFS="$(printf '\t')" read -r vn vr; do v2_view "$vn" "$vr"; done < "$vg/selection"
 printf '2\n' > "$vstate/.format"
 mv "$vstate/.format" "$vstate/format"
 sync
 rm -rf "$vstate/converting"
}
v2_ensure() {
 local root vn vr vg
 if [ -d "$vstate/converting" ]; then
  if [ -f "$vstate/converting/ready" ]; then v2_conversion_finish
  else
   [ ! -e "$state/pending" ] || fail 'unexpected legacy journal during conversion preparation'
   rm -rf "$vstate/converting"
  fi
 fi
 if v2_active; then
  [ -f "$state/pending/format-fence" ] && [ ! -f "$state/pending/ready" ] || fail 'missing old-manager downgrade fence'
  return
 fi
 [ ! -e "$vstate/format" ] || fail 'unsupported package state format'
 [ ! -e "$state/pending" ] || fail 'legacy publication unfinished; run gucman recover first'
 progress 'Converting package selection to format 2; older managers will refuse mutations'
 mkdir -p "$vstate/generations"
 mkdir "$vstate/converting"
 : > "$vstate/converting/selection"
 for root in "$state/packages"/*; do
  [ -f "$root/current/manifest.json" ] || continue
  vn=${root##*/};vr=$(readlink "$root/current")
  v2_release_ok "$vn" "$vr" || fail 'invalid legacy installed release'
  # Imported contents are an observed baseline, not a new upstream authenticity claim.
  [ -f "$vr/dependency-hashes" ] || : > "$vr/dependency-hashes"
  if [ ! -f "$vr/payload.sha256" ]; then v2_receipt "$vr"; fi
  printf '%s\t%s\n' "$vn" "$vr" >> "$vstate/converting/selection"
 done
 vg=$(v2_new "$vstate/converting/selection")
 printf '%s\n' "$vg" > "$vstate/converting/generation"
 sync
 : > "$vstate/converting/ready"
 sync
 v2_conversion_finish
}
v2_finish() {
 local old new current vn vr previous sorted_old sorted_new removed
 [ -f "$vstate/pending/ready" ] || fail 'incomplete generation preparation'
 sha256sum -c "$vstate/pending/checksums" > "$vstate/pending/verification.log" || fail 'generation journal changed'
 old=$(cat "$vstate/pending/old"); new=$(cat "$vstate/pending/new")
 v2_generation_ok "$old" && v2_generation_ok "$new" || fail 'invalid pending generation'
 current=$(readlink "$vstate/current")
 [ "$current" = "$old" ] || [ "$current" = "$new" ] || fail 'selector differs from pending transaction'
 v2_validate "$new"
 while IFS="$(printf '\t')" read -r vn vr; do
  v2_view "$vn"
  "$links" check /usr/local "$state/packages/$vn" "$vr/paths"
 done < "$new/selection"
 # Additions are precreated dangling links. Atomic selection does not promise
 # atomic prefix directory enumeration or snapshot isolation for running code.
 while IFS="$(printf '\t')" read -r vn vr; do "$links" link /usr/local "$state/packages/$vn" "$vr/paths"; done < "$new/selection"
 progress 'Activating package generation'
 v2_select "$new"
 # Remove only links whose exact old ownership target is still present.
 while IFS="$(printf '\t')" read -r vn vr; do
  previous=$new/packages/$vn
  sort "$vr/paths" > "$vstate/pending/old-paths"
  if [ -f "$previous/paths" ]; then sort "$previous/paths" > "$vstate/pending/new-paths"; else : > "$vstate/pending/new-paths"; fi
  comm -23 "$vstate/pending/old-paths" "$vstate/pending/new-paths" > "$vstate/pending/removed-paths"
  "$links" unlink /usr/local "$state/packages/$vn" "$vstate/pending/removed-paths"
 done < "$old/selection"
 sync
 rm -rf "$vstate/pending"
}
v2_publish() {
 local new=$1
 v2_generation_ok "$new" || fail 'invalid activation target'
 [ ! -e "$vstate/pending" ] || fail 'unfinished generation publication; run gucman recover'
 publishing=1
 mkdir "$vstate/pending"
 readlink "$vstate/current" > "$vstate/pending/old"
 printf '%s\n' "$new" > "$vstate/pending/new"
 sha256sum "$vstate/pending/old" "$vstate/pending/new" "$new/metadata.sha256" > "$vstate/pending/checksums"
 sync
 : > "$vstate/pending/ready"
 sync
 v2_finish
 publishing=
}
v2_map_replace() {
 local file=$1 vn=$2 vr=$3
 awk -F '\t' -v name="$vn" '$1 != name' "$file" > "$file.next"
 [ -z "$vr" ] || printf '%s\t%s\n' "$vn" "$vr" >> "$file.next"
 mv "$file.next" "$file"
}
v2_list() {
 local vg current
 v2_active || { echo 'Package generations are not enabled yet'; return; }
 current=$(readlink "$vstate/current")
 for vg in "$vstate"/generations/g.*; do
  v2_generation_ok "$vg" || continue
  [ -f "$vg/ready" ] || continue
  printf '%s\t%s\t%s packages\t' "${vg##*/}" "$(cat "$vg/created")" "$(wc -l < "$vg/selection")"
  if [ "$vg" = "$current" ]; then echo current; else echo retained; fi
 done
}
v2_gc() {
 local vg vn vr root keep
 v2_ensure
 [ ! -e "$vstate/pending" ] || fail 'recover before garbage collection'
 local scratch=$(mktemp -d /tmp/gucman-gc.XXXXXX)
 : > "$scratch/roots"
 for vg in "$vstate"/generations/g.*; do
  v2_generation_ok "$vg" || fail 'unexpected generation storage'
  if [ ! -f "$vg/ready" ]; then
   [ "$vg" != "$(readlink "$vstate/current")" ] && [ "$(cat "$vg/managed-format")" = 2 ] || fail 'invalid incomplete generation'
   rm -rf "$vg"; continue
  fi
  v2_validate "$vg"
  cut -f2 "$vg/selection" >> "$scratch/roots"
 done
 for root in "$state/packages"/*; do
  [ -d "$root" ] && [ ! -L "$root" ] || continue
  vn=${root##*/};name_ok "$vn" || continue
  for vr in "$root"/release.*; do
   [ -d "$vr" ] && [ ! -L "$vr" ] && [ -f "$vr/managed-format" ] || continue
   [ "$(cat "$vr/managed-format")" = 2 ] || continue
   grep -Fxq "$vr" "$scratch/roots" && continue
   case "$vr" in "$state/packages/$vn"/release.*) ;; *) fail 'invalid garbage collection path';; esac
   rm -rf "$vr"; echo "Collected $vn ${vr##*/}"
  done
 done
 rm -rf "$scratch"
}
