// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVICES_LIB_AMLOGIC_INCLUDE_SOC_AML_A311D_A311D_GPIO_H_
#define SRC_DEVICES_LIB_AMLOGIC_INCLUDE_SOC_AML_A311D_A311D_GPIO_H_

#define A311D_GPIOZ_COUNT 16
#define A311D_GPIOA_COUNT 16
#define A311D_GPIOBOOT_COUNT 16
#define A311D_GPIOC_COUNT 8
#define A311D_GPIOX_COUNT 20
#define A311D_GPIOH_COUNT 9
#define A311D_GPIOAO_COUNT 12
#define A311D_GPIOE_COUNT 3

#define A311D_GPIOZ_START 0
#define A311D_GPIOA_START A311D_GPIOZ_COUNT
#define A311D_GPIOBOOT_START (A311D_GPIOA_START + A311D_GPIOA_COUNT)
#define A311D_GPIOC_START (A311D_GPIOBOOT_START + A311D_GPIOBOOT_COUNT)
#define A311D_GPIOX_START (A311D_GPIOC_START + A311D_GPIOC_COUNT)
#define A311D_GPIOH_START (A311D_GPIOX_START + A311D_GPIOX_COUNT)
#define A311D_GPIOAO_START (A311D_GPIOH_START + A311D_GPIOH_COUNT)
#define A311D_GPIOE_START (A311D_GPIOAO_START + A311D_GPIOAO_COUNT)

#define A311D_GPIOZ(n) (A311D_GPIOZ_START + n)
#define A311D_GPIOA(n) (A311D_GPIOA_START + n)
#define A311D_GPIOBOOT(n) (A311D_GPIOBOOT_START + n)
#define A311D_GPIOC(n) (A311D_GPIOC_START + n)
#define A311D_GPIOX(n) (A311D_GPIOX_START + n)
#define A311D_GPIOH(n) (A311D_GPIOH_START + n)
#define A311D_GPIOAO(n) (A311D_GPIOAO_START + n)
#define A311D_GPIOE(n) (A311D_GPIOE_START + n)

// GPIOBOOT pin alternate functions
#define A311D_GPIOBOOT_0_EMMC_D0_FN 1
#define A311D_GPIOBOOT_1_EMMC_D1_FN 1
#define A311D_GPIOBOOT_2_EMMC_D2_FN 1
#define A311D_GPIOBOOT_3_EMMC_D3_FN 1
#define A311D_GPIOBOOT_4_EMMC_D4_FN 1
#define A311D_GPIOBOOT_5_EMMC_D5_FN 1
#define A311D_GPIOBOOT_6_EMMC_D6_FN 1
#define A311D_GPIOBOOT_7_EMMC_D7_FN 1
#define A311D_GPIOBOOT_8_EMMC_CLK_FN 1
#define A311D_GPIOBOOT_10_EMMC_CMD_FN 1
#define A311D_GPIOBOOT_13_EMMC_DS_FN 1

// GPIOA pin alternate functions
#define A311D_GPIOA_1_TDMB_SCLK_FN 1
#define A311D_GPIOA_1_TDMB_SLV_SCLK_FN 2
#define A311D_GPIOA_2_TDMB_FS_FN 1
#define A311D_GPIOA_2_TDMB_SLV_FS_FN 2
#define A311D_GPIOA_3_TDMB_D0_FN 1
#define A311D_GPIOA_3_TDMB_DIN0_FN 2
#define A311D_GPIOA_6_PDM_DIN2_FN 1
#define A311D_GPIOA_6_TDMB_DIN3_FN 2
#define A311D_GPIOA_6_TDMB_D3_FN 3
#define A311D_GPIOA_7_PDM_DCLK_FN 1
#define A311D_GPIOA_7_TDMC_D3_FN 2
#define A311D_GPIOA_7_TDMC_DIN3_FN 3
#define A311D_GPIOA_8_PDM_DIN0_FN 1
#define A311D_GPIOA_8_TDMC_D2_FN 2
#define A311D_GPIOA_8_TDMC_DIN2_FN 3
#define A311D_GPIOA_9_PDM_DIN1_FN 1
#define A311D_GPIOA_9_TDMC_D1_FN 2
#define A311D_GPIOA_9_TDMC_DIN1_FN 3
#define A311D_GPIOA_14_I2C_EE_M3_SDA_FN 2
#define A311D_GPIOA_15_I2C_EE_M3_SCL_FN 2

// GPIOC pin alternate functions
#define A311D_GPIOC_0_SDCARD_D0_FN 1
#define A311D_GPIOC_1_SDCARD_D1_FN 1
#define A311D_GPIOC_2_SDCARD_D2_FN 1
#define A311D_GPIOC_3_SDCARD_D3_FN 1
#define A311D_GPIOC_4_SDCARD_CLK_FN 1
#define A311D_GPIOC_5_SDCARD_CMD_FN 1

// GPIOC pin alternate functions
#define A311D_GPIOX_0_SDIO_D0_FN 1
#define A311D_GPIOX_1_SDIO_D1_FN 1
#define A311D_GPIOX_2_SDIO_D2_FN 1
#define A311D_GPIOX_3_SDIO_D3_FN 1
#define A311D_GPIOX_4_SDIO_CLK_FN 1
#define A311D_GPIOX_5_SDIO_CMD_FN 1

// GPIOZ pin alternate functions
#define A311D_GPIOZ_0_ETH_MDIO_FN 1
#define A311D_GPIOZ_1_ETH_MDC_FN 1
#define A311D_GPIOZ_2_ETH_RX_CLK_FN 1
#define A311D_GPIOZ_2_TDMC_D0_FN 4
#define A311D_GPIOZ_3_ETH_RX_DV_FN 1
#define A311D_GPIOZ_3_TDMC_D1_FN 4
#define A311D_GPIOZ_4_ETH_RXD0_FN 1
#define A311D_GPIOZ_4_TDMC_D2_FN 4
#define A311D_GPIOZ_5_ETH_RXD1_FN 1
#define A311D_GPIOZ_5_TDMC_D3_FN 4
#define A311D_GPIOZ_6_ETH_RXD2_FN 1
#define A311D_GPIOZ_6_TDMC_FS_FN 4
#define A311D_GPIOZ_7_ETH_RXD3_FN 1
#define A311D_GPIOZ_7_TDMC_SCLK_FN 4
#define A311D_GPIOZ_8_ETH_TX_CLK_FN 1
#define A311D_GPIOZ_9_ETH_TX_EN_FN 1
#define A311D_GPIOZ_10_ETH_TXD0_FN 1
#define A311D_GPIOZ_11_ETH_TXD1_FN 1
#define A311D_GPIOZ_12_ETH_TXD2_FN 1
#define A311D_GPIOZ_13_ETH_TXD3_FN 1

// GPIOAO pin alternate functions
#define A311D_GPIOAO_2_M0_SCL_FN 1
#define A311D_GPIOAO_3_M0_SDA_FN 1
#define A311D_GPIOAO_9_MCLK_FN 5

// GPIOE pin alternate functions
#define A311D_GPIOE_1_PWM_D_FN 3
#define A311D_GPIOE_2_PWM_D_FN 3

#define A311D_PAD_PULL_UP_EN_REG0 0x48
#define A311D_PAD_PULL_UP_EN_REG1 0x49
#define A311D_PAD_PULL_UP_EN_REG2 0x4a
#define A311D_PAD_PULL_UP_EN_REG3 0x4b
#define A311D_PAD_PULL_UP_EN_REG4 0x4c
#define A311D_PAD_PULL_UP_EN_REG5 0x4d

#define A311D_PULL_UP_REG0 0x3a
#define A311D_PULL_UP_REG1 0x3b
#define A311D_PULL_UP_REG2 0x3c
#define A311D_PULL_UP_REG3 0x3d
#define A311D_PULL_UP_REG4 0x3e
#define A311D_PULL_UP_REG5 0x3f

#define A311D_AO_PAD_DS_A 0x07
#define A311D_AO_PAD_DS_B 0x08
#define A311D_PAD_DS_REG0A 0xd0
#define A311D_PAD_DS_REG1A 0xd1
#define A311D_PAD_DS_REG2A 0xd2
#define A311D_PAD_DS_REG2B 0xd3
#define A311D_PAD_DS_REG3A 0xd4
#define A311D_PAD_DS_REG4A 0xd5
#define A311D_PAD_DS_REG5A 0xd6

#endif  // SRC_DEVICES_LIB_AMLOGIC_INCLUDE_SOC_AML_A311D_A311D_GPIO_H_
