// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#ifndef ZIRCON_KERNEL_HYPERVISOR_INCLUDE_HYPERVISOR_GUEST_PHYSICAL_ADDRESS_SPACE_H_
#define ZIRCON_KERNEL_HYPERVISOR_INCLUDE_HYPERVISOR_GUEST_PHYSICAL_ADDRESS_SPACE_H_

#include <ktl/move.h>
#include <ktl/unique_ptr.h>
#include <vm/vm_address_region.h>
#include <vm/vm_aspace.h>
#include <vm/vm_object.h>

namespace hypervisor {

// RAII object that holds a mapping of guest physical address space to the host
// kernel virtual address space. Can be used to map a frequently accessed
// portion of guest physical memory for faster access.
class GuestPtr {
 public:
  GuestPtr() = default;
  GuestPtr(fbl::RefPtr<VmMapping> mapping, zx_vaddr_t offset)
      : mapping_(ktl::move(mapping)), offset_(offset) {}
  GuestPtr(GuestPtr&& o) : mapping_(ktl::move(o.mapping_)), offset_(o.offset_) {}
  ~GuestPtr() { reset(); }
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GuestPtr);

  GuestPtr& operator=(GuestPtr&& o) {
    mapping_ = ktl::move(o.mapping_);
    offset_ = o.offset_;
    return *this;
  }

  void reset() {
    if (mapping_) {
      mapping_->Destroy();
      mapping_.reset();
    }
  }

  template <typename T>
  T* as() const {
    if (offset_ + sizeof(T) > mapping_->size()) {
      return nullptr;
    }
    return reinterpret_cast<T*>(mapping_->base() + offset_);
  }

 private:
  fbl::RefPtr<VmMapping> mapping_;
  zx_vaddr_t offset_;
};

class GuestPhysicalAddressSpace {
 public:
  static zx_status_t Create(
#ifdef ARCH_ARM64
      uint8_t vmid,
#endif
      ktl::unique_ptr<GuestPhysicalAddressSpace>* gpas);

  ~GuestPhysicalAddressSpace();

  size_t size() const { return guest_aspace_->size(); }
  ArchVmAspace* arch_aspace() { return &guest_aspace_->arch_aspace(); }
  fbl::RefPtr<VmAddressRegion> RootVmar() { return guest_aspace_->RootVmar(); }

  zx_status_t MapInterruptController(zx_gpaddr_t guest_paddr, zx_paddr_t host_paddr, size_t len);
  zx_status_t UnmapRange(zx_gpaddr_t guest_paddr, size_t len);
  zx_status_t GetPage(zx_gpaddr_t guest_paddr, zx_paddr_t* host_paddr);
  zx_status_t PageFault(zx_gpaddr_t guest_paddr);
  zx_status_t QueryFlags(zx_gpaddr_t guest_paddr, uint* mmu_flags);
  zx_status_t CreateGuestPtr(zx_gpaddr_t guest_paddr, size_t len, const char* name,
                             GuestPtr* guest_ptr);

 private:
  fbl::RefPtr<VmAspace> guest_aspace_;
};

}  // namespace hypervisor

#endif  // ZIRCON_KERNEL_HYPERVISOR_INCLUDE_HYPERVISOR_GUEST_PHYSICAL_ADDRESS_SPACE_H_
