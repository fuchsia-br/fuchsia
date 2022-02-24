// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVELOPER_DEBUG_ZXDB_CLIENT_FRAME_H_
#define SRC_DEVELOPER_DEBUG_ZXDB_CLIENT_FRAME_H_

#include <stdint.h>

#include <optional>

#include "lib/fit/function.h"
#include "src/developer/debug/shared/register_id.h"
#include "src/developer/debug/shared/register_info.h"
#include "src/developer/debug/shared/register_value.h"
#include "src/developer/debug/zxdb/client/client_object.h"
#include "src/developer/debug/zxdb/symbols/symbol_data_provider.h"
#include "src/lib/fxl/macros.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace zxdb {

class EvalContext;
class Location;
class Thread;

// Represents one stack frame.
//
// See also FrameFingerprint (the getter for a fingerprint is on Thread).
class Frame : public ClientObject {
 public:
  explicit Frame(Session* session);
  virtual ~Frame();

  fxl::WeakPtr<Frame> GetWeakPtr();

  // Guaranteed non-null.
  virtual Thread* GetThread() const = 0;

  // Returns true if this is a synthetic stack frame for an inlined function. Inlined functions
  // don't have separate functions or stack pointers and are generated by the debugger based on the
  // symbols for a given location.
  virtual bool IsInline() const = 0;

  // Returns the physical stack frame associated with the current frame. This is used to get the
  // non-inlined frame an inlined frame was expanded from. Non-inlined frames should return |this|.
  virtual const Frame* GetPhysicalFrame() const = 0;

  // Returns the location of the stack frame code. This will be symbolized.
  virtual const Location& GetLocation() const = 0;

  // Returns the program counter of this frame. This may be faster than
  // GetLocation().address() since it doesn't need to be symbolized.
  virtual uint64_t GetAddress() const = 0;

  // Retrieves the registers of the given category that were saved with this stack frame. Only the
  // general registers are always available synchronously and on every stack frame.
  //
  // Non-general registers can be retrieved for the top stack frame by querying asynchronously. Once
  // queried, they will be available synchronously from this function. If unfetched or the top stack
  // frame is non-topmost, this will return nullptr.
  //
  // The general registers for non-topmost stack frames will be reconstructed by the unwinder.
  // Normally only a subset of them are avilable in that case (IP and SP, and some
  // architecture-dependant ones). The top stack frame will have all of them.
  //
  // Inline frames will report the registers from the physical frame they're associated with.
  virtual const std::vector<debug::RegisterValue>* GetRegisterCategorySync(
      debug::RegisterCategory category) const = 0;

  // Asynchronous version of GetRegisterCategorySync(). For topmost stack frames, things like vector
  // and floating-point registers can be queried from the agent with this function. The results will
  // be cached so will be available synchronously in the future via GetRegisterCategorySync().
  //
  // The callback will always be issued. If the frame is destroyed before the registers are
  // retrieved, the error will be set and it will be called with an empty vector.
  //
  // If |always_request| is set, the registers will always be requested even if there is an entry
  // in the cache. This is normally used for console commands such as "registers" that will always
  // want the most up to date data.
  virtual void GetRegisterCategoryAsync(
      debug::RegisterCategory category, bool always_request,
      fit::function<void(const Err&, const std::vector<debug::RegisterValue>&)> cb) = 0;

  // Writes to the given register. The register must be a canonical hardware register.
  //
  // This will fail if the current frame is not the top physical frame (otherwise it will clobber
  // the register for the top frame).
  virtual void WriteRegister(debug::RegisterID id, std::vector<uint8_t> data,
                             fit::callback<void(const Err&)> cb) = 0;

  // The frame base pointer.
  //
  // This is not necessarily the "BP" register. The symbols can specify an arbitrary frame base for
  // a location and this value will reflect that. If the base pointer is known-unknown, it will be
  // reported as 0 rather than nullopt (nullopt from GetBasePointer() indicates it needs an async
  // call).
  //
  // In most cases the frame base is available synchronously (when it's in a register which is the
  // common case), but symbols can declare any DWARF expression to compute the frame base.
  //
  // The synchronous version will return the base pointer if possible. If it returns no value, code
  // that can handle async calls can call the asynchronous version to be notified when the value is
  // available.
  virtual std::optional<uint64_t> GetBasePointer() const = 0;
  virtual void GetBasePointerAsync(fit::callback<void(uint64_t bp)> cb) = 0;

  // Returns the stack pointer at this location.
  virtual uint64_t GetStackPointer() const = 0;

  // The canonical frame address is the stack pointer immediately before calling into the current
  // frame. This will be 0 if unknown.
  virtual uint64_t GetCanonicalFrameAddress() const = 0;

  // Returns the SymbolDataProvider that can be used to evaluate symbols in the context of this
  // frame.
  virtual fxl::RefPtr<SymbolDataProvider> GetSymbolDataProvider() const = 0;

  // Returns the EvalContext that can be used to evaluate expressions in the context of this frame.
  virtual fxl::RefPtr<EvalContext> GetEvalContext() const = 0;

  // Determines if the code location this frame's address corresponds to is potentially ambiguous.
  // This happens when the instruction is the beginning of an inlined routine, and the address could
  // be considered either the imaginary call to the inlined routine, or its first code instruction.
  // See the Stack class declaration for more details about this case.
  virtual bool IsAmbiguousInlineLocation() const = 0;

 private:
  FXL_DISALLOW_COPY_AND_ASSIGN(Frame);

  fxl::WeakPtrFactory<Frame> weak_factory_;
};

}  // namespace zxdb

#endif  // SRC_DEVELOPER_DEBUG_ZXDB_CLIENT_FRAME_H_
