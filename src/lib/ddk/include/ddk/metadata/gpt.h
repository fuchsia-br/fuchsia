// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_LIB_DDK_INCLUDE_DDK_METADATA_GPT_H_
#define SRC_LIB_DDK_INCLUDE_DDK_METADATA_GPT_H_

#include <zircon/hw/gpt.h>

typedef struct guid_map {
  const char name[GPT_NAME_LEN];
  const uint8_t guid[GPT_GUID_LEN];
} guid_map_t;

#endif  // SRC_LIB_DDK_INCLUDE_DDK_METADATA_GPT_H_
