// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/hardware/platform/bus/c/banjo.h>
#include <lib/ddk/binding.h>
#include <lib/ddk/debug.h>
#include <lib/ddk/device.h>
#include <lib/ddk/metadata.h>
#include <lib/ddk/platform-defs.h>
#include <lib/device-protocol/display-panel.h>

#include <ddk/metadata/display.h>
#include <soc/aml-s905d2/s905d2-gpio.h>
#include <soc/aml-s905d2/s905d2-hw.h>

#include "nelson-gpios.h"
#include "nelson.h"
#include "src/devices/board/drivers/nelson/nelson_display_bind.h"

namespace nelson {

constexpr pbus_mmio_t display_mmios[] = {
    {
        // VBUS/VPU
        .base = S905D2_VPU_BASE,
        .length = S905D2_VPU_LENGTH,
    },
    {
        // TOP DSI Host Controller (Amlogic Specific)
        .base = S905D2_MIPI_TOP_DSI_BASE,
        .length = S905D2_MIPI_TOP_DSI_LENGTH,
    },
    {
        // DSI PHY
        .base = S905D2_DSI_PHY_BASE,
        .length = S905D2_DSI_PHY_LENGTH,
    },
    {
        // HHI
        .base = S905D2_HIU_BASE,
        .length = S905D2_HIU_LENGTH,
    },
    {
        // AOBUS
        .base = S905D2_AOBUS_BASE,
        .length = S905D2_AOBUS_LENGTH,
    },
    {
        // CBUS
        .base = S905D2_CBUS_BASE,
        .length = S905D2_CBUS_LENGTH,
    },
};

static const pbus_irq_t display_irqs[] = {
    {
        .irq = S905D2_VIU1_VSYNC_IRQ,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
    {
        .irq = S905D2_RDMA_DONE,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
    {
        .irq = S905D2_VID1_WR,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
};

pbus_metadata_t display_panel_metadata[] = {
    {
        .type = DEVICE_METADATA_DISPLAY_CONFIG,
        .data_buffer = nullptr,
        .data_size = 0,
    },
};

static const pbus_bti_t display_btis[] = {
    {
        .iommu_index = 0,
        .bti_id = BTI_DISPLAY,
    },
};

static pbus_dev_t display_dev = []() {
  pbus_dev_t dev = {};
  dev.name = "display";
  dev.vid = PDEV_VID_AMLOGIC;
  dev.pid = PDEV_PID_AMLOGIC_S905D2;
  dev.did = PDEV_DID_AMLOGIC_DISPLAY;
  dev.metadata_list = display_panel_metadata;
  dev.metadata_count = std::size(display_panel_metadata);
  dev.mmio_list = display_mmios;
  dev.mmio_count = std::size(display_mmios);
  dev.irq_list = display_irqs;
  dev.irq_count = std::size(display_irqs);
  dev.bti_list = display_btis;
  dev.bti_count = std::size(display_btis);
  return dev;
}();

// Composite binding rules for display driver.

zx_status_t Nelson::DisplayInit() {
  display_panel_t display_panel_info[] = {
      {
          .width = 600,
          .height = 1024,
      },
  };

  auto display_id = GetDisplayId();
  switch (display_id) {
    case 0b10:
      display_panel_info[0].panel_type = PANEL_TV070WSM_FT;
      break;
    case 0b11:
      display_panel_info[0].panel_type = PANEL_TV070WSM_FT_9365;
      break;
    case 0b01:
      display_panel_info[0].panel_type = PANEL_KD070D82_FT_9365;
      break;
    case 0b00:
      display_panel_info[0].panel_type = PANEL_KD070D82_FT;
      break;
    default:
      zxlogf(ERROR, "%s: invalid display panel detected: %d", __func__, display_id);
      return ZX_ERR_INVALID_ARGS;
  }
  display_panel_metadata[0].data_size = sizeof(display_panel_info);
  display_panel_metadata[0].data_buffer = reinterpret_cast<uint8_t*>(&display_panel_info);

  auto status = pbus_.AddComposite(&display_dev, reinterpret_cast<uint64_t>(display_fragments),
                                   std::size(display_fragments), "dsi");
  if (status != ZX_OK) {
    zxlogf(ERROR, "%s: CompositeDeviceAdd display failed: %d", __func__, status);
    return status;
  }

  return ZX_OK;
}

}  // namespace nelson
