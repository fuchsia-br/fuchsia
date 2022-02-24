// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {argh::FromArgs, ffx_core::ffx_command};

#[ffx_command()]
#[derive(FromArgs, Debug, PartialEq)]
#[argh(
    subcommand,
    name = "route-sources",
    description = "Verifies that routes to designated components are routed from designated sources.",
    example = "To verify route sources according to a configuration file on your current build:

        $ffx scrutiny verify route-sources --build-path $(fx get-build-dir) --zbi path/to/image.zbi --blobfs-manifest path/to/blob.manifest --config path/to/verify_route_sources_config.json"
)]
pub struct ScrutinyRouteSourcesCommand {
    /// path to root output directory of build.
    #[argh(option)]
    pub build_path: String,
    /// path to ZBI image to be verified.
    #[argh(option)]
    pub zbi: String,
    /// path to blobfs manifest file generated by the build.
    #[argh(option)]
    pub blobfs_manifest: String,
    /// path to configuration file that specifies components and their expected
    /// route sources.
    #[argh(option)]
    pub config: String,
    /// path to depfile that gathers dependencies during execution.
    #[argh(option)]
    pub depfile: Option<String>,
    /// path to stamp file to write to if and only if verification succeeds.
    #[argh(option)]
    pub stamp: Option<String>,
}
