// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_UI_LIB_ESCHER_RESOURCES_RESOURCE_MANAGER_H_
#define SRC_UI_LIB_ESCHER_RESOURCES_RESOURCE_MANAGER_H_

#include "src/ui/lib/escher/base/owner.h"
#include "src/ui/lib/escher/forward_declarations.h"
#include "src/ui/lib/escher/resources/resource.h"
#include "src/ui/lib/escher/vk/vulkan_context.h"
#include "src/ui/lib/escher/vk/vulkan_device_queues.h"

namespace escher {

// ResourceManager is responsible for deciding whether to reuse or destroy
// resources that are returned to it.  The only restriction is that the
// manager must wait until it is "safe" to destroy the resource; the definition
// of safety depends on the context, but typically means something like not
// destroying resources while they are used by pending Vulkan command-buffers.
//
// Not thread-safe.
class ResourceManager : public Owner<Resource, ResourceTypeInfo> {
 public:
  explicit ResourceManager(EscherWeakPtr escher);

  Escher* escher() { return escher_.get(); }
  EscherWeakPtr GetEscherWeakPtr() { return escher_; }
  const VulkanContext& vulkan_context() const { return vulkan_context_; }
  vk::Device vk_device() const { return vulkan_context_.device; }
  const VulkanDeviceQueues::Caps& caps() const;

 private:
  friend class Resource;

  // Must be implemented by subclasses.
  virtual void OnReceiveOwnable(std::unique_ptr<Resource> resource) = 0;

  const EscherWeakPtr escher_;
  VulkanContext vulkan_context_;

  FXL_DISALLOW_COPY_AND_ASSIGN(ResourceManager);
};

}  // namespace escher

#endif  // SRC_UI_LIB_ESCHER_RESOURCES_RESOURCE_MANAGER_H_
