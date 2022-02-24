// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_GRAPHICS_LIB_COMPUTE_SPINEL2_PLATFORMS_VK_TARGETS_VENDORS_INTEL_GEN8_CONFIG_H_
#define SRC_GRAPHICS_LIB_COMPUTE_SPINEL2_PLATFORMS_VK_TARGETS_VENDORS_INTEL_GEN8_CONFIG_H_

//
// GLSL EXTENSIONS
//
// clang-format off
//
#ifdef VULKAN

#define SPN_EXT_ENABLE_SUBGROUP_UNIFORM                           1

#endif

//
// DEVICE-SPECIFIC
//

#define SPN_DEVICE_INTEL_GEN8                                     1

#define SPN_DEVICE_INTEL_SIMD8_LOG2                               3
#define SPN_DEVICE_INTEL_SIMD16_LOG2                              4
#define SPN_DEVICE_INTEL_SIMD32_LOG2                              5

#define SPN_DEVICE_MAX_PUSH_CONSTANTS_SIZE                        128 // bytes
#define SPN_DEVICE_SMEM_PER_SUBGROUP_DWORDS                       292

//
// TILE CONFIGURATION
//
#define SPN_DEVICE_TILE_WIDTH_LOG2                                3
#define SPN_DEVICE_TILE_HEIGHT_LOG2                               3

//
// BLOCK POOL CONFIGURATION
//
#define SPN_DEVICE_BLOCK_POOL_BLOCK_DWORDS_LOG2                   5
#define SPN_DEVICE_BLOCK_POOL_SUBBLOCK_DWORDS_LOG2                SPN_DEVICE_TILE_HEIGHT_LOG2

//
// KERNEL: BLOCK POOL INIT
//
#define SPN_DEVICE_BLOCK_POOL_INIT_SUBGROUP_SIZE_LOG2             0
#define SPN_DEVICE_BLOCK_POOL_INIT_WORKGROUP_SIZE                 128

#define SPN_DEVICE_BLOCK_POOL_INIT_BP_IDS_PER_INVOCATION          16

//
// KERNEL: PATHS ALLOC
//
// Note that this workgroup only uses one lane but, depending on the
// target, it might be necessary to launch at least a subgroup.
//
#define SPN_DEVICE_PATHS_ALLOC_SUBGROUP_SIZE_LOG2                 0
#define SPN_DEVICE_PATHS_ALLOC_WORKGROUP_SIZE                     1

//
// KERNEL: PATHS COPY
//
#define SPN_DEVICE_PATHS_COPY_SUBGROUP_SIZE_LOG2                  SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_PATHS_COPY_WORKGROUP_SIZE                      ((1 << SPN_DEVICE_PATHS_COPY_SUBGROUP_SIZE_LOG2) * 1)

//
// KERNEL: FILL SCAN
//
#define SPN_DEVICE_FILL_SCAN_SUBGROUP_SIZE_LOG2                   SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_FILL_SCAN_WORKGROUP_SIZE                       ((1 << SPN_DEVICE_FILL_SCAN_SUBGROUP_SIZE_LOG2) * 1)

#define SPN_DEVICE_FILL_SCAN_ROWS                                 4
#define SPN_DEVICE_FILL_SCAN_EXPAND()                             SPN_EXPAND_4()
#define SPN_DEVICE_FILL_SCAN_EXPAND_I_LAST                        3

//
// KERNEL: FILL EXPAND
//
#define SPN_DEVICE_FILL_EXPAND_SUBGROUP_SIZE_LOG2                 SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_FILL_EXPAND_WORKGROUP_SIZE                     ((1 << SPN_DEVICE_FILL_EXPAND_SUBGROUP_SIZE_LOG2) * 1)

//
// KERNEL: FILL DISPATCH
//
#define SPN_DEVICE_FILL_DISPATCH_SUBGROUP_SIZE_LOG2               SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_FILL_DISPATCH_WORKGROUP_SIZE                   ((1 << SPN_DEVICE_FILL_DISPATCH_SUBGROUP_SIZE_LOG2) * 1)

//
// KERNEL: RASTERIZE_[LINES|QUADS|CUBICS|...]
//
// Cohort size can be reduced to force earlier launches of smaller grids.
//
#define SPN_DEVICE_RASTERIZE_SUBGROUP_SIZE_LOG2                   SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_RASTERIZE_WORKGROUP_SIZE                       ((1 << SPN_DEVICE_RASTERIZE_SUBGROUP_SIZE_LOG2) * 1)

#define SPN_DEVICE_RASTERIZE_COHORT_SIZE                          SPN_RASTER_COHORT_MAX_SIZE

//
// KERNEL: TTRKS SEGMENT
//
#define SPN_DEVICE_TTRKS_SEGMENT_SUBGROUP_SIZE_LOG2               SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_TTRKS_SEGMENT_WORKGROUP_SIZE                   ((1 << SPN_DEVICE_TTRKS_SEGMENT_SUBGROUP_SIZE_LOG2) * 1)
#define SPN_DEVICE_TTRKS_SEGMENT_ROWS                             1

//
// KERNEL: TTRKS SEGMENT DISPATCH
//
#define SPN_DEVICE_TTRKS_SEGMENT_DISPATCH_SUBGROUP_SIZE_LOG2      0
#define SPN_DEVICE_TTRKS_SEGMENT_DISPATCH_WORKGROUP_SIZE          1

//
// KERNEL: RASTERS ALLOC
//
#define SPN_DEVICE_RASTERS_ALLOC_SUBGROUP_SIZE_LOG2               SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_RASTERS_ALLOC_WORKGROUP_SIZE                   ((1 << SPN_DEVICE_RASTERS_ALLOC_SUBGROUP_SIZE_LOG2) * 1)

//
// KERNEL: RASTERS PREFIX
//
#define SPN_DEVICE_RASTERS_PREFIX_SUBGROUP_SIZE_LOG2              SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_RASTERS_PREFIX_WORKGROUP_SIZE                  ((1 << SPN_DEVICE_RASTERS_PREFIX_SUBGROUP_SIZE_LOG2) * 1)

//
// KERNEL: PLACE TTPK & TTSK
//
#define SPN_DEVICE_PLACE_SUBGROUP_SIZE_LOG2                       SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_PLACE_WORKGROUP_SIZE                           ((1 << SPN_DEVICE_PLACE_SUBGROUP_SIZE_LOG2) * 1)

//
// KERNEL: TTCKS SEGMENT
//
#define SPN_DEVICE_TTCKS_SEGMENT_SUBGROUP_SIZE_LOG2               SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_TTCKS_SEGMENT_WORKGROUP_SIZE                   ((1 << SPN_DEVICE_TTCKS_SEGMENT_SUBGROUP_SIZE_LOG2) * 1)
#define SPN_DEVICE_TTCKS_SEGMENT_ROWS                             1

//
// KERNEL: TTCKS SEGMENT DISPATCH
//
#define SPN_DEVICE_TTCKS_SEGMENT_DISPATCH_SUBGROUP_SIZE_LOG2      0
#define SPN_DEVICE_TTCKS_SEGMENT_DISPATCH_WORKGROUP_SIZE          1

//
// KERNEL: RENDER
//
#define SPN_DEVICE_RENDER_SUBGROUP_SIZE_LOG2                      SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_RENDER_WORKGROUP_SIZE                          ((1 << SPN_DEVICE_RENDER_SUBGROUP_SIZE_LOG2) * 1)

#define SPN_DEVICE_RENDER_LGF_USE_SHUFFLE
#define SPN_DEVICE_RENDER_TTCKS_USE_SHUFFLE
#define SPN_DEVICE_RENDER_STYLING_CMDS_USE_SHUFFLE

#define SPN_DEVICE_RENDER_TILE_CHANNEL_IS_FLOAT16

#define SPN_DEVICE_RENDER_SURFACE_TYPE                            rgba8

#if defined(__Fuchsia__) // TODO(allanmac): The tile copy pass will eventually take care of this
#define SPN_DEVICE_RENDER_SURFACE_SWIZZLE(rgba_)                  (rgba_)
#else
#define SPN_DEVICE_RENDER_SURFACE_SWIZZLE(rgba_)                  (rgba_).bgra
#endif

//
// KERNEL: RENDER DISPATCH
//
#define SPN_DEVICE_RENDER_DISPATCH_SUBGROUP_SIZE_LOG2             0
#define SPN_DEVICE_RENDER_DISPATCH_WORKGROUP_SIZE                 1

//
// KERNEL: PATHS RECLAIM
//
#define SPN_DEVICE_PATHS_RECLAIM_SUBGROUP_SIZE_LOG2               SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_PATHS_RECLAIM_WORKGROUP_SIZE                   ((1 << SPN_DEVICE_PATHS_RECLAIM_SUBGROUP_SIZE_LOG2) * 1)

//
// KERNEL: RASTERS RECLAIM
//
#define SPN_DEVICE_RASTERS_RECLAIM_SUBGROUP_SIZE_LOG2             SPN_DEVICE_INTEL_SIMD8_LOG2
#define SPN_DEVICE_RASTERS_RECLAIM_WORKGROUP_SIZE                 ((1 << SPN_DEVICE_RASTERS_RECLAIM_SUBGROUP_SIZE_LOG2) * 1)

//
// clang-format on
//

#endif  // SRC_GRAPHICS_LIB_COMPUTE_SPINEL2_PLATFORMS_VK_TARGETS_VENDORS_INTEL_GEN8_CONFIG_H_
