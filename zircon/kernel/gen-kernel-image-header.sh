#!/usr/bin/env bash

# Copyright 2020 The Fuchsia Authors
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT

set -e -o pipefail

readonly NM="$1"
readonly KERNEL_IMAGE_FILE="$(<"$2")"
readonly KERNEL_SYMBOL_FILE="$(<"$3")"
readonly OUTPUT="$4"
readonly DEPFILE="$5"

readonly BASE_SYMBOL=__code_start
readonly VERSION_SYMBOL=kVersionString
readonly NM_REGEXP="$BASE_SYMBOL|$VERSION_SYMBOL"

trap 'rm -f "$DEPFILE"' ERR HUP INT TERM

grok() {
  local addr size type symbol base_addr version_string_addr version_string_size
  while read addr size type symbol; do
    if [ -z "$symbol" ]; then
      # GNU nm omits the size column when it's zero.
      symbol="$type"
      type="$size"
      size=0
    fi
    case "$symbol" in
      $BASE_SYMBOL)
        ((base_addr="0x$addr"))
        ;;
      $VERSION_SYMBOL)
        ((version_string_addr="0x$addr"))
        ((version_string_size="0x$size"))
        ;;
    esac
  done
  if [[ -z "$base_addr" || -z "$version_string_addr" || -z "$version_string_size" ]]; then
    echo >&2 "$0: cannot find version string address details"
    exit 2
  fi
  ((version_string_addr-=base_addr))
  local template='// Generated by %s.  DO NOT EDIT!
#define KERNEL_IMAGE_FILE "%s"
#define VERSION_STRING_OFFSET %#x
#define VERSION_STRING_SIZE %u
'
  local new_contents="$(printf "$template" "$0" "$KERNEL_IMAGE_FILE" $version_string_addr $version_string_size)"
  if [ ! -r "$OUTPUT" ] || [ "$(<"$OUTPUT")" != "$new_contents" ]; then
    echo "$new_contents" > "$OUTPUT"
  fi
}

echo "$OUTPUT: $KERNEL_SYMBOL_FILE" > "$DEPFILE"
"$NM" -S "$KERNEL_SYMBOL_FILE" | egrep "$NM_REGEXP" | grok