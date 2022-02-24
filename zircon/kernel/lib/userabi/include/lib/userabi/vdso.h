// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#ifndef ZIRCON_KERNEL_LIB_USERABI_INCLUDE_LIB_USERABI_VDSO_H_
#define ZIRCON_KERNEL_LIB_USERABI_INCLUDE_LIB_USERABI_VDSO_H_

#include <lib/userabi/rodso.h>
#include <lib/userabi/userboot.h>

#include <vm/vm_object.h>

class VmMapping;

class VDso : public RoDso {
 public:
  // This is called only once, at boot time.
  //
  // The created VDso will retain RefPtrs to the created VmObjectDispatchers,
  // but ownership of the wrapping handles are given to the caller.
  //
  // The RoDso VMO is created in vmo_kernel_handles[Variant::NEXT]
  // with the VDso variants in the other slots.
  static const VDso* Create(KernelHandle<VmObjectDispatcher>* vmo_kernel_handles);

  static bool vmo_is_vdso(const fbl::RefPtr<VmObject>& vmo) {
    return likely(instance_) && instance_->vmo_is_vdso_impl(vmo);
  }

  static bool valid_code_mapping(uint64_t vmo_offset, size_t size) {
    return instance_->RoDso::valid_code_mapping(vmo_offset, size);
  }

  // Given VmAspace::vdso_code_mapping_, return the vDSO base address or 0.
  static uintptr_t base_address(const fbl::RefPtr<VmMapping>& code_mapping);

  // Forward declaration of generated class.
  // This class is defined in the file vdso-valid-sysret.h,
  // which is generated by scripts/gen-vdso-valid-sysret.sh.
  // It has a static method named after each syscall:
  //     static bool <syscall-name>(uintptr_t offset);
  // This tests whether <start of vDSO code>+offset is a valid PC
  // for entering the kernel with <syscall-name>'s syscall number.
  struct ValidSyscallPC;

 private:
  using Variant = userboot::VdsoVariant;

  VDso(KernelHandle<VmObjectDispatcher>* vmo_kernel_handles);
  void CreateVariant(Variant, KernelHandle<VmObjectDispatcher>* vmo_kernel_handle);

  bool vmo_is_vdso_impl(const fbl::RefPtr<VmObject>& vmo_ref) const {
    if (vmo_ref == vmo()->vmo())
      return true;
    for (const auto& v : variant_vmo_) {
      if (vmo_ref == v->vmo())
        return true;
    }
    return false;
  }

  static constexpr size_t variant_index(Variant v) {
    DEBUG_ASSERT(v >= Variant::STABLE && v < Variant::COUNT);
    return static_cast<size_t>(v);
  }

  fbl::RefPtr<VmObjectDispatcher> variant_vmo_[static_cast<size_t>(Variant::COUNT)];

  static const VDso* instance_;
};

#endif  // ZIRCON_KERNEL_LIB_USERABI_INCLUDE_LIB_USERABI_VDSO_H_
