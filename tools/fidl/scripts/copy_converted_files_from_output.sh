# Copyright 2021 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#!/bin/bash

# This script works on repos with a simple mapping from src -> out paths.  For
# the more complex cases in fuchsia.git, use fidl_converter.py instead.
#
# Given a newline separate list of files as stdin (probably generated by using a
# tool like `find` or similar), perform a static transform on their paths and
# copy them from the GN output directory back into source.  For example, when
# the pwd is root, and we are building to out/default in this repo, we would
# pipe the result of
#
#   find ./out/default/. -name "*.fidl.new" -type f | xargs readlink -f
#
# into this script.

# The script takes on argument: the (escaped) "needle" string that will be
# removed from the output path to
[[ ! -z "$1" ]] && { ESCAPED_NEEDLE=$1; true; }

# Duplicate the output filepath
xargs -I {} echo 'echo cp -fT {} {}' \
# Remove the "ESCAPED_NEEDLE" from the destination path
| sed -re "s/${ESCAPED_NEEDLE}//2" \
# Remove the ".new" suffix from the FIDL file name in the destination path
| sed -re 's/.new$//' \
# Execute each line as a shell command, which performs the overwrite
# | xargs -i -t sh -c "{}"