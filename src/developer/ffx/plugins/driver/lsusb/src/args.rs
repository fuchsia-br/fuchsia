// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    argh::{FromArgValue, FromArgs},
    ffx_core::ffx_command,
};

#[derive(Debug, PartialEq)]
struct UsbDevice {
    vendor_id: u16,
    product_id: Option<u16>,
}

impl FromArgValue for UsbDevice {
    fn from_arg_value(value: &str) -> Result<Self, String> {
        if let Some((vid, pid)) = value.split_once(':') {
            let vid = u16::from_str_radix(vid, 16)
                .map_err(|_| "Vendor ID is not a valid hexadecimal number'.".to_string())?;

            let pid = u16::from_str_radix(pid, 16)
                .map_err(|_| "Product ID is not a valid hexadecimal number'.".to_string())?;

            Ok(UsbDevice { vendor_id: vid, product_id: Some(pid) })
        } else {
            let vid = u16::from_str_radix(value, 16)
                .map_err(|_| "Vendor id is not a valid hexadecimal number'.".to_string())?;

            Ok(UsbDevice { vendor_id: vid, product_id: None })
        }
    }
}

impl std::convert::Into<lsusb::args::UsbDevice> for UsbDevice {
    fn into(self) -> lsusb::args::UsbDevice {
        lsusb::args::UsbDevice { vendor_id: self.vendor_id, product_id: self.product_id }
    }
}

#[ffx_command()]
#[derive(FromArgs, Debug, PartialEq)]
#[argh(
    subcommand,
    name = "lsusb",
    description = "Print the device tree of the target to stdout",
    example = "To show the device tree:

    $ ffx driver lsusb",
    error_code(1, "Failed to connect to the device manager service")
)]
pub struct DriverLsusbCommand {
    #[argh(switch, short = 't')] // TODO
    /// prints USB device tree
    tree: bool,
    #[argh(switch, short = 'v')]
    /// verbose output (prints descriptors)
    verbose: bool,
    #[argh(option, short = 'c')]
    /// prints configuration descriptor for specified configuration (rather than
    /// current configuration)
    configuration: Option<u8>,
    #[argh(option, short = 'd')]
    /// shows only devices with the specified vendor and product ID numbers (in hexadecimal)
    /// UsbDevice must be in format vendor[:product]
    device: Option<UsbDevice>,
}

impl std::convert::Into<lsusb::args::Args> for DriverLsusbCommand {
    fn into(self) -> lsusb::args::Args {
        lsusb::args::Args {
            tree: self.tree,
            verbose: self.verbose,
            configuration: self.configuration,
            device: self.device.map(|d| d.into()),
        }
    }
}
