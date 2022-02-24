// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZIRCON_SYSTEM_ULIB_HID_INCLUDE_HID_VISALIA_TOUCH_H_
#define ZIRCON_SYSTEM_ULIB_HID_INCLUDE_HID_VISALIA_TOUCH_H_

#include <zircon/types.h>

__BEGIN_CDECLS

// clang-format off
#define BUTTONS_RPT_ID_INPUT       0x01
// clang-format on

// TODO(puneetha): Remove bitfields.
typedef struct visalia_touch_buttons_input_rpt {
  uint8_t rpt_id;
#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint8_t volume_up : 1;
  uint8_t volume_down : 1;
  uint8_t pause : 1;
  uint8_t padding : 5;
#else
  uint8_t padding : 5;
  uint8_t pause : 1;
  uint8_t volume_down : 1;
  uint8_t volume_up : 1;
#endif
} __PACKED visalia_touch_buttons_input_rpt_t;

size_t get_visalia_touch_buttons_report_desc(const uint8_t** buf);
void fill_visalia_touch_buttons_report(uint8_t id, bool value,
                                       visalia_touch_buttons_input_rpt_t* rpt);

__END_CDECLS

#endif  // ZIRCON_SYSTEM_ULIB_HID_INCLUDE_HID_VISALIA_TOUCH_H_
