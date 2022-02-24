// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vmo_writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <zircon/status.h>

#include "src/devices/lib/log/log.h"

void VmoWriter::Printf(const char* fmt, ...) {
  if (status_ != ZX_OK) {
    return;
  }

  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  const size_t length = strlen(buf);

  if (add_overflow(available_, length, &available_)) {
    status_ = ZX_ERR_INTERNAL;
    return;
  }

  size_t new_written;
  if (add_overflow(written_, length, &new_written) || size_ <= new_written) {
    status_ = ZX_ERR_BUFFER_TOO_SMALL;
    return;
  }

  auto status = vmo_.write(buf, written_, length);
  if (status != ZX_OK) {
    status_ = status;
    LOGF(ERROR, "Failed to write to VMO: %s", zx_status_get_string(status));
    return;
  }

  written_ = new_written;
}
