// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/virtualization/bin/vmm/pci.h"

#include <endian.h>
#include <lib/async/default.h>
#include <lib/pci/hw.h>

#include <gtest/gtest.h>

#include "src/virtualization/bin/vmm/bits.h"

namespace {

constexpr uint16_t kPciConfigAddrPortBase = 0;
constexpr uint16_t kPciConfigDataPortBase = 4;

// A simple 4-byte PCI capability containing the id/next fields,
// and 2 bytes of data.
struct SimplePciCap {
  uint8_t id;
  uint8_t next;
  uint8_t data[2];
};

// A publicly configurable PciDevice.
class TestPciDevice : public PciDevice {
 public:
  TestPciDevice() : PciDevice({.name = "Test PCI device"}) {}

  // Add a bar of the given size and type. Return the index.
  size_t AddBar(size_t size, TrapType type) {
    return PciDevice::AddBar(PciBar(this, size, type, nullptr)).value();
  }

  // Add the given POD type as a capability.
  template <typename T>
  zx_status_t AddCapability(const T& capability) {
    return PciDevice::AddCapability(capability);
  }

  // `PciDevice` implementation.
  bool HasPendingInterrupt() const override { return false; }
};

constexpr uint64_t pci_type1_addr(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg) {
  return 0x80000000 | (bus << 16) | (device << 11) | (function << 8) | pci_type1_register(reg);
}

// Test we can read multiple fields in 1 32-bit word.
TEST(PciDeviceTest, ReadConfigRegister) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());
  PciDevice* device = bus.root_complex();

  // Access Vendor/Device ID as a single 32bit read.
  IoValue value = {};
  value.access_size = 4;
  EXPECT_EQ(device->ReadConfig(PCI_CONFIG_VENDOR_ID, &value), ZX_OK);
  EXPECT_EQ(value.u32, PCI_VENDOR_ID_INTEL | (PCI_DEVICE_ID_INTEL_Q35 << 16));
}

// Verify we can read portions of a 32 bit word, one byte at a time.
TEST(PciDeviceTest, ReadConfigRegisterBytewise) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());
  PciDevice* device = bus.root_complex();

  uint32_t expected_device_vendor = PCI_VENDOR_ID_INTEL | (PCI_DEVICE_ID_INTEL_Q35 << 16);
  for (int i = 0; i < 4; ++i) {
    uint16_t reg = static_cast<uint16_t>(PCI_CONFIG_VENDOR_ID + i);
    IoValue value = {};
    value.access_size = 1;
    EXPECT_EQ(device->ReadConfig(reg, &value), ZX_OK);
    EXPECT_EQ(value.u32, bits_shift(expected_device_vendor, i * 8 + 7, i * 8));
  }
}

// Test unaligned reads are reported as errors.
TEST(PciDeviceTest, ReadConfigRegisterUnaligned) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());
  PciDevice* device = bus.root_complex();

  // Attempt to read 16-bits with an offset of 1.
  IoValue value = IoValue::FromU16(0);
  EXPECT_EQ(device->ReadConfig(/*reg=*/0x1, &value), ZX_ERR_IO);

  // Attempt to write 32-bits with an offset of 2.
  EXPECT_EQ(device->WriteConfig(/*reg=*/0x2, IoValue::FromU32(0)), ZX_ERR_IO);
}

// PCI devices BAR sizes must be a power of 2 and must not support setting any
// bits in the BAR that are not size aligned. Software often relies on this to
// read the bar size by writing all 1's to the register and reading back the
// value.
//
// This tests that we properly mask the lowest bits so software can compute the
// BAR size.
TEST(PciDeviceTest, ReadBarSize) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());

  // Add a PCI device with a single BAR.
  TestPciDevice test_device{};
  test_device.AddBar(/*size=*/0x10, TrapType::MMIO_SYNC);
  bus.Connect(&test_device, async_get_default_dispatcher());

  // Set all bits in the BAR register. The device will ignore writes to the
  // LSBs which we can read out to determine the size.
  IoValue value;
  value.access_size = 4;
  value.u32 = UINT32_MAX;
  EXPECT_EQ(test_device.WriteConfig(PCI_CONFIG_BASE_ADDRESSES + 0, value), ZX_OK);
  EXPECT_EQ(test_device.WriteConfig(PCI_CONFIG_BASE_ADDRESSES + 4, value), ZX_OK);

  // PCI Express Base Specification, Rev. 4.0 Version 1.0, Section 7.5.1.2.1:
  // Base Address Registers
  //
  // Software saves the original value of the Base Address register, writes a
  // value of all 1's to the register, then reads it back. Size calculation can
  // be done from the 32 bit value read by first clearing encoding information
  // bits (bits 1:0 for I/O, bits 3:0 for memory), inverting all 32 bits
  // (logical NOT), then incrementing by 1. The resultant 32-bit value is the
  // memory/I/O range size decoded by the register. Note that the upper 16 bits
  // of the result is ignored if the Base Address register is for I/O and bits
  // 31:16 returned zero upon read. The original value in the Base Address
  // register is restored before re-enabling decode in the Command register of
  // the Function.
  //
  // 64-bit (memory) Base Address registers can be handled the same, except that
  // the second 32 bit register is considered an extension of the first (i.e.,
  // bits 63:32). Software writes a value of all 1's to both registers, reads
  // them back, and combines the result into a 64-bit value. Size calculation is
  // done on the 64-bit value.

  // Read out BAR and compute size.
  value.access_size = 4;
  value.u32 = 0;
  EXPECT_EQ(test_device.ReadConfig(PCI_CONFIG_BASE_ADDRESSES + 0, &value), ZX_OK);
  uint64_t reg = value.u32;
  EXPECT_EQ(test_device.ReadConfig(PCI_CONFIG_BASE_ADDRESSES + 4, &value), ZX_OK);
  reg |= static_cast<uint64_t>(value.u32) << 32;

  EXPECT_EQ(reg & ~kPciBarMmioAddrMask, kPciBarMmioType64Bit | kPciBarMmioAccessSpace);
  const PciBar* bar = test_device.bar(0);
  EXPECT_TRUE(bar != nullptr);
  EXPECT_EQ(~(reg & kPciBarMmioAddrMask) + 1, bar->size());
}

// The 8-bit interrupt line register should be read/write.
TEST(PciDeviceTest, ReadWriteInterruptLine) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());
  PciDevice* device = bus.root_complex();

  // Write/read the 8-bit value directly.
  {
    EXPECT_EQ(device->WriteConfig(PCI_CONFIG_INTERRUPT_LINE, IoValue::FromU8(0xaa)), ZX_OK);
    IoValue value = IoValue::FromU8(0);
    EXPECT_EQ(device->ReadConfig(PCI_CONFIG_INTERRUPT_LINE, &value), ZX_OK);
    EXPECT_EQ(value.u8, 0xaa);
  }

  // Write/read 32-bits. The interrupt line register should be updated, but not
  // the neighbouring registers.
  {
    IoValue original_value = IoValue::FromU32(0);
    EXPECT_EQ(device->ReadConfig(PCI_CONFIG_INTERRUPT_LINE, &original_value), ZX_OK);

    EXPECT_EQ(device->WriteConfig(PCI_CONFIG_INTERRUPT_LINE, IoValue::FromU32(0x55555555)), ZX_OK);

    IoValue value = IoValue::FromU32(0);
    EXPECT_EQ(device->ReadConfig(PCI_CONFIG_INTERRUPT_LINE, &value), ZX_OK);
    EXPECT_EQ(value.u32, clear_bits(original_value.u32, 8, 0) | 0x55);
  }
}

// Verify stats & cap registers correctly show present capabilities and that
// capability data is readable.
TEST(PciDeviceTest, ReadCapability) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());
  TestPciDevice test_device{};
  bus.Connect(&test_device, async_get_default_dispatcher());

  // PCI Local Bus Spec 3.0 Table 6-2: Status Register Bits
  //
  // This optional read-only bit indicates whether or not this device
  // implements the pointer for a New Capabilities linked list at offset 34h.
  // A value of zero indicates that no New Capabilities linked list is
  // available. A value of one indicates that the value read at offset 34h is
  // a pointer in Configuration Space to a linked list of new capabilities.
  // Refer to Section 6.7 for details on New Capabilities.
  //
  // Initially expect the bit to be 0, as we have no caps.
  IoValue status = IoValue::FromU16(0);
  EXPECT_EQ(test_device.ReadConfig(PCI_CONFIG_STATUS, &status), ZX_OK);
  EXPECT_FALSE(status.u16 & PCI_STATUS_NEW_CAPS);

  // Create and install a simple capability.
  EXPECT_EQ(test_device.AddCapability(SimplePciCap{.id = 0x9, .next = 0, .data = {0x11, 0x22}}),
            ZX_OK);

  // The PCI_STATUS_NEW_CAPS bit should now be set.
  EXPECT_EQ(test_device.ReadConfig(PCI_CONFIG_STATUS, &status), ZX_OK);
  EXPECT_TRUE(status.u16 & PCI_STATUS_NEW_CAPS);

  // Read the cap pointer from config space. Here just verify it points to
  // some location beyond the pre-defined header.
  IoValue cap_ptr = IoValue::FromU8(0);
  EXPECT_EQ(test_device.ReadConfig(PCI_CONFIG_CAPABILITIES, &cap_ptr), ZX_OK);
  EXPECT_LT(0x40u, cap_ptr.u8);

  // Read the capability. This will be the Cap ID (9), next pointer (0), followed
  // by data bytes (starting at index 2).
  IoValue cap_value = IoValue::FromU32(0);
  EXPECT_EQ(test_device.ReadConfig(cap_ptr.u8, &cap_value), ZX_OK);
  EXPECT_EQ(be32toh(0x09'00'11'22), cap_value.u32);
}

// Build a list of capabilities with no data (only the required ID/next
// fields). Verify the next pointers are correctly wired up to traverse
// the linked list.
TEST(PciDeviceTest, ReadChainedCapability) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());
  TestPciDevice test_device{};
  bus.Connect(&test_device, async_get_default_dispatcher());

  // Build list of caps.
  const int kNumCaps = 5;
  for (uint8_t i = 0; i < kNumCaps; i++) {
    EXPECT_EQ(test_device.AddCapability(SimplePciCap{.id = i}), ZX_OK);
  }

  IoValue cap_ptr = IoValue::FromU8(0);
  EXPECT_EQ(test_device.ReadConfig(PCI_CONFIG_CAPABILITIES, &cap_ptr), ZX_OK);
  for (uint8_t i = 0; i < kNumCaps; ++i) {
    IoValue cap_header = IoValue::FromU32(0);

    // Read the current capability.
    EXPECT_EQ(test_device.ReadConfig(cap_ptr.u8, &cap_header), ZX_OK);
    // ID is the first byte.
    EXPECT_EQ(i, cap_header.u32 & UINT8_MAX);
    // Next pointer is the second byte.
    cap_ptr.u8 = static_cast<uint8_t>(cap_header.u32 >> 8);
  }
  EXPECT_EQ(0u, cap_ptr.u8);
}

// Test accesses to the PCI config address ports.
//
// Access to the 32-bit PCI config address port is provided by the IO ports
// 0xcf8 - 0xcfb. Accesses to each port must have the same alignment as the
// port address used.
//
// The device operates on relative port addresses so we'll use 0-3 instead of
// 0cf8-0xcfb
//
// Ex:
//  -------------------------------------
// | port  | valid access widths (bytes) |
// --------------------------------------|
// |   0   | 1, 2, 4                     |
// |   1   | 1                           |
// |   2   | 1, 2                        |
// |   3   | 1                           |
//  -------------------------------------
TEST(PciBusTest, WriteConfigAddressPort) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());

  // 32 bit write.
  IoValue value;
  value.access_size = 4;
  value.u32 = 0x12345678;
  EXPECT_EQ(bus.WriteIoPort(kPciConfigAddrPortBase, value), ZX_OK);
  EXPECT_EQ(bus.config_addr(), 0x12345678u);

  // 16 bit write to bits 31..16. Other bits remain unchanged.
  value.access_size = 2;
  value.u16 = 0xFACE;
  EXPECT_EQ(bus.WriteIoPort(kPciConfigAddrPortBase + 2, value), ZX_OK);
  EXPECT_EQ(bus.config_addr(), 0xFACE5678u);

  // 8 bit write to bits (15..8). Other bits remain unchanged.
  value.access_size = 1;
  value.u8 = 0x99;
  EXPECT_EQ(bus.WriteIoPort(kPciConfigAddrPortBase + 1, value), ZX_OK);
  EXPECT_EQ(bus.config_addr(), 0xFACE9978u);
}

// Test reading the PCI config address ports.
//
// See pci_bus_write_config_addr_port for more details.
TEST(PciBusTest, ReadConfigAddressPort) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());
  bus.set_config_addr(0x12345678);

  // 32 bit read (bits 31..0).
  IoValue value = {};
  value.access_size = 4;
  EXPECT_EQ(bus.ReadIoPort(kPciConfigAddrPortBase, &value), ZX_OK);
  EXPECT_EQ(value.access_size, 4);
  EXPECT_EQ(value.u32, 0x12345678u);

  // 16 bit read (bits 31..16).
  value.access_size = 2;
  value.u16 = 0;
  EXPECT_EQ(bus.ReadIoPort(kPciConfigAddrPortBase + 2, &value), ZX_OK);
  EXPECT_EQ(value.access_size, 2);
  EXPECT_EQ(value.u16, 0x1234u);

  // 8 bit read (bits 15..8).
  value.access_size = 1;
  value.u8 = 0;
  EXPECT_EQ(bus.ReadIoPort(kPciConfigAddrPortBase + 1, &value), ZX_OK);
  EXPECT_EQ(value.access_size, 1);
  EXPECT_EQ(value.u8, 0x56u);
}

// The address written to the data port (0xcf8) is 4b aligned. The offset into
// the data port range 0xcfc-0xcff is added to the address to access partial
// words.
TEST(PciBusTest, ReadConfigDataPort) {
  Guest guest;
  PciBus bus(&guest, nullptr);
  bus.Init(async_get_default_dispatcher());
  IoValue value = {};

  // 16-bit read.
  bus.set_config_addr(pci_type1_addr(0, 0, 0, 0));
  value.access_size = 2;
  EXPECT_EQ(bus.ReadIoPort(kPciConfigDataPortBase, &value), ZX_OK);
  EXPECT_EQ(value.access_size, 2);
  EXPECT_EQ(value.u16, PCI_VENDOR_ID_INTEL);

  // 32-bit read from same address. Result should now contain the Device ID
  // in the upper 16 bits
  value.access_size = 4;
  value.u32 = 0;
  EXPECT_EQ(bus.ReadIoPort(kPciConfigDataPortBase, &value), ZX_OK);
  EXPECT_EQ(value.access_size, 4);
  EXPECT_EQ(value.u32, PCI_VENDOR_ID_INTEL | (PCI_DEVICE_ID_INTEL_Q35 << 16));

  // 16-bit read of upper half-word.
  //
  // Device ID is 2b aligned and the PCI config address register can only hold
  // a 4b aligned address. The offset into the word addressed by the PCI
  // address port is added to the data port address.
  value.access_size = 2;
  value.u16 = 0;
  bus.set_config_addr(pci_type1_addr(0, 0, 0, PCI_CONFIG_DEVICE_ID));
  // Verify we're using a 4b aligned register address.
  EXPECT_EQ(bus.config_addr() & bit_mask<uint32_t>(2), 0u);
  // Add the register offset to the data port base address.
  EXPECT_EQ(bus.ReadIoPort(kPciConfigDataPortBase + (PCI_CONFIG_DEVICE_ID & bit_mask<uint32_t>(2)),
                           &value),
            ZX_OK);
  EXPECT_EQ(value.access_size, 2);
  EXPECT_EQ(value.u16, PCI_DEVICE_ID_INTEL_Q35);
}

}  // namespace
