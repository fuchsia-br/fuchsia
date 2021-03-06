// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_GRAPHICS_BIN_OPENCL_LOADER_LOADER_H_
#define SRC_GRAPHICS_BIN_OPENCL_LOADER_LOADER_H_
#include <fuchsia/opencl/loader/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fidl/cpp/interface_handle.h>

#include <string>

#include "src/graphics/bin/opencl_loader/app.h"

// Implements the opencl loader's Loader service which provides the client
// driver portion to the loader as a VMO.
class LoaderImpl final : public fuchsia::opencl::loader::Loader, public LoaderApp::Observer {
 public:
  explicit LoaderImpl(LoaderApp* app) : app_(app) {}
  ~LoaderImpl() final;

  // Adds a binding for fuchsia::opencl::loader::Loader to |outgoing|. Will create a new loader for
  // every connection.
  static void Add(LoaderApp* app, const std::shared_ptr<sys::OutgoingDirectory>& outgoing);

  // LoaderApp::Observer implementation.
  void OnIcdListChanged(LoaderApp* app) override;

 private:
  // fuchsia::opencl::loader::Loader impl
  void Get(std::string name, GetCallback callback) override;
  void ConnectToDeviceFs(zx::channel channel) override;
  void ConnectToManifestFs(fuchsia::opencl::loader::ConnectToManifestOptions options,
                           zx::channel channel) override;
  void GetSupportedFeatures(GetSupportedFeaturesCallback callback) override;

  void AddCallback(std::string name, fit::function<void(zx::vmo)> callback);

  bool waiting_for_callbacks() const {
    return !callbacks_.empty() || !connect_manifest_handles_.empty();
  }

  LoaderApp* app_;

  fidl::BindingSet<fuchsia::opencl::loader::Loader,
                   std::unique_ptr<fuchsia::opencl::loader::Loader>>
      bindings_;

  std::list<std::pair<std::string, fit::function<void(zx::vmo)>>> callbacks_;
  std::vector<zx::channel> connect_manifest_handles_;
};

#endif  // SRC_GRAPHICS_BIN_OPENCL_LOADER_LOADER_H_
