// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_MEDIA_AUDIO_AUDIO_CORE_STREAM_H_
#define SRC_MEDIA_AUDIO_AUDIO_CORE_STREAM_H_

#include <fuchsia/media/cpp/fidl.h>
#include <lib/fpromise/result.h>
#include <lib/zx/time.h>

#include <optional>

#include <fbl/static_vector.h>

#include "src/media/audio/audio_core/packet.h"
#include "src/media/audio/audio_core/stage_metrics.h"
#include "src/media/audio/audio_core/stream_usage.h"
#include "src/media/audio/lib/clock/audio_clock.h"
#include "src/media/audio/lib/format/format.h"
#include "src/media/audio/lib/timeline/timeline_function.h"

namespace media::audio {

class BaseStream {
 public:
  static constexpr bool kLogPresentationDelay = false;

  BaseStream(Format format) : format_(format) {}
  virtual ~BaseStream() = default;

  // Format of data generated by this stream.
  // TODO(fxbug.dev/58740): make sure this is accurate in all implementations.
  const Format& format() const { return format_; }

  // A snapshot of a |TimelineFunction| with an associated |generation|. If |generation| is equal
  // between two subsequent calls to |ref_time_to_fract_presentation_frame|, then the
  // |timeline_function| is guaranteed to be unchanged.
  struct TimelineFunctionSnapshot {
    TimelineFunction timeline_function;
    uint32_t generation;
  };

  // This function translates from a timestamp to the corresponding fixed-point frame number that
  // will be presented at that time. The timestamp is relative to the stream's reference clock.
  virtual TimelineFunctionSnapshot ref_time_to_frac_presentation_frame() const = 0;
  virtual AudioClock& reference_clock() = 0;

  // Common shorthands to convert between PTS and frame numbers.
  Fixed FracPresentationFrameAtRefTime(zx::time ref_time) const {
    return Fixed::FromRaw(
        ref_time_to_frac_presentation_frame().timeline_function.Apply(ref_time.get()));
  }
  zx::time RefTimeAtFracPresentationFrame(Fixed frame) const {
    return zx::time(
        ref_time_to_frac_presentation_frame().timeline_function.ApplyInverse(frame.raw_value()));
  }

  // The presentation delay is defined to be the absolute difference between a frame's
  // presentation timestamp and the frame's safe read/write timestamp. This is always a
  // positive number. Ideally this should be the exact delay, if known, and otherwise a
  // true upper-bound of the delay, however in practice it is sometimes a best-effort
  // estimate that can be either low or high.
  //
  // For render pipelines, this represents the delay between reading a frame with
  // ReadLock and actually rendering the frame at an output device. This is also known as
  // the "min lead time".
  //
  // For capture pipelines, this represents the delay between capturing a frame at
  // an input device and reading that frame with ReadLock.
  zx::duration GetPresentationDelay() const { return presentation_delay_.load(); }

  // Presentation delays are propagated from destination streams to source streams. The
  // delay passed to the source stream is typically external_delay + intrinsic_delay.
  // The default implementation is sufficient for pipeline stages that do not introduce
  // extra delay.
  virtual void SetPresentationDelay(zx::duration external_delay) {
    presentation_delay_.store(external_delay);
  }

 private:
  Format format_;
  std::atomic<zx::duration> presentation_delay_{zx::duration(0)};
};

// A read-only stream of audio data.
class ReadableStream : public BaseStream {
 public:
  ReadableStream(Format format) : BaseStream(format) {}
  virtual ~ReadableStream() = default;

  class Buffer {
   public:
    using DestructorT = fit::callback<void(bool fully_consumed)>;

    Buffer(Fixed start_frame, int64_t length_in_frames, void* payload, bool is_continuous,
           StreamUsageMask usage_mask, float total_applied_gain_db, DestructorT dtor = nullptr)
        : dtor_(std::move(dtor)),
          payload_(payload),
          start_(start_frame),
          length_(length_in_frames),
          is_continuous_(is_continuous),
          usage_mask_(usage_mask),
          total_applied_gain_db_(total_applied_gain_db) {}

    ~Buffer() {
      if (dtor_) {
        dtor_(is_fully_consumed_);
      }
    }

    Buffer(Buffer&& rhs) = default;
    Buffer& operator=(Buffer&& rhs) = default;

    Buffer(const Buffer& rhs) = delete;
    Buffer& operator=(const Buffer& rhs) = delete;

    Fixed start() const { return start_; }
    Fixed end() const { return start_ + Fixed(length_); }
    int64_t length() const { return length_; }
    void* payload() const { return payload_; }

    // Indicates this packet is continuous with a packet previously returned from an immediately
    // preceding |ReadLock| call.
    //
    // Buffers may become discontinuous if, for example, and AudioRenderer is flushed and new
    // packets are provided; these new packets will not be assumed to be continuous with the
    // preceeding ones. Each |ReadableStream| implementation is reponsible for reporting any
    // discontinuity so that stream processors (ex: the mixer) may clear any intermediate state
    // based on the continuity of the stream.
    bool is_continuous() const { return is_continuous_; }

    // Call this to indicate whether the buffer was fully consumed.
    // By default, we assume this is true.
    void set_is_fully_consumed(bool fully_consumed) { is_fully_consumed_ = fully_consumed; }

    // Returns the set of usages that have contributed to this buffer.
    StreamUsageMask usage_mask() const { return usage_mask_; }

    // Returns the total gain that has been applied to the source stream. For example, if
    // total_applied_gain_db = -5.0 and the source stream started as a sine wave with unity
    // amplitude, then payload() should contain a sine wave with amplitude -5.0dB.
    float total_applied_gain_db() const { return total_applied_gain_db_; }

   private:
    DestructorT dtor_;
    void* payload_;
    Fixed start_;
    int64_t length_;
    bool is_continuous_;
    bool is_fully_consumed_ = true;
    StreamUsageMask usage_mask_;
    float total_applied_gain_db_;
  };

  // ReadLockContext provides a container for state that can be carried through a
  // sequence of ReadLock calls.
  class ReadLockContext {
   private:
    static constexpr size_t kMaxStages = 16;

   public:
    // Add the given metrics. Internally we maintain one StageMetrics object per stage.
    // If this method is called multiple times with the same stage name, the metrics are
    // accumulated.
    void AddStageMetrics(const StageMetrics& new_stage) {
      for (auto& old_stage : per_stage_metrics_) {
        if (std::string_view(old_stage.name) == std::string_view(new_stage.name)) {
          old_stage += new_stage;
          return;
        }
      }
      // Add a new stage, or silently drop if we've exceeded the maximum.
      if (per_stage_metrics_.size() < kMaxStages) {
        per_stage_metrics_.push_back(new_stage);
      }
    }

    // Return all metrics accumulated via AddMetrics.
    using StageMetricsVector = fbl::static_vector<StageMetrics, kMaxStages>;
    const StageMetricsVector& per_stage_metrics() { return per_stage_metrics_; }

   private:
    StageMetricsVector per_stage_metrics_;
  };

  // ReadableStream is implemented by audio pipeline stages that consume zero or more
  // source streams and produce a destination stream. ReadLock acquires a readlock on
  // the destination stream. The parameters |dest_frame| and |frame_count| represent a
  // range of frames on the destination stream's frame timeline.
  //
  // If no data is available for that frame range, ReadLock returns std::nullopt.
  // Otherwise, ReadLock returns a buffer representing all or part of the requested range.
  // If |start()| on the returned buffer is greater than |dest_frame|, then the stream
  // has no data for those frames and it may be treated as silence. Conversely, if |end()|
  // on the returned buffer is less than |dest_frame + frame_count|, this does not indicate
  // silence for those frames. Instead it indicates the full frame range is not available
  // on a single contiguous buffer. Clients should call |ReadLock| again and provide the
  // |end()| of the previous buffer as |dest_frame| to query if the stream has more frames.
  //
  // The returned buffer must not refer to frames outside of the range [floor(dest_frame),
  // ceiling(dest_frame) + frame_count).
  //
  // The buffer will remain locked until it is destructed. It is illegal to call ReadLock
  // again until the lock has been released.
  //
  // TODO(fxbug.dev/50669): Implementations must return std::nullopt if they have no frames for the
  // requested range. This requirement is not enforced by all implementations (e.g., PacketQueue).
  virtual std::optional<Buffer> ReadLock(ReadLockContext& ctx, Fixed dest_frame,
                                         int64_t frame_count) = 0;

  // Trims the stream by releasing any frames before the given frame. When invoked,
  // the caller is making a promise that they will not try to ReadLock any frame before
  // dest_frame. If the stream has allocated buffers for the trimmed range, it can free
  // those buffers now.
  virtual void Trim(Fixed dest_frame) = 0;

  // Hooks to log [Partial] Underflow events.
  // TODO(fxbug.dev/58614): convert this to use PTS instead of frame numbers
  virtual void ReportUnderflow(Fixed frac_source_start, Fixed frac_source_mix_point,
                               zx::duration underflow_duration) {}
  virtual void ReportPartialUnderflow(Fixed frac_source_offset, int64_t dest_mix_offset) {}
};

// A write-only stream of audio data.
class WritableStream : public BaseStream {
 public:
  WritableStream(Format format) : BaseStream(format) {}
  virtual ~WritableStream() = default;

  // PTS is relative to to parent stream's reference clock.
  class Buffer {
   public:
    using DestructorT = fit::callback<void()>;

    Buffer(int64_t start_frame, int64_t length_in_frames, void* payload, DestructorT dtor = nullptr)
        : dtor_(std::move(dtor)),
          payload_(payload),
          start_(start_frame),
          end_(start_frame + length_in_frames),
          length_(length_in_frames) {}

    ~Buffer() {
      if (dtor_) {
        dtor_();
      }
    }

    Buffer(Buffer&& rhs) = default;
    Buffer& operator=(Buffer&& rhs) = default;

    Buffer(const Buffer& rhs) = delete;
    Buffer& operator=(const Buffer& rhs) = delete;

    int64_t start() const { return start_; }
    int64_t end() const { return end_; }
    int64_t length() const { return length_; }
    void* payload() const { return payload_; }

   private:
    DestructorT dtor_;
    void* payload_;
    int64_t start_;
    int64_t end_;
    int64_t length_;
  };

  // WritableStream is implemented by audio sinks. WriteLock acquires a write lock on the
  // stream. The parameters |frame| and |frame_count| represent a range of frames on the
  // stream's frame timeline.
  //
  // If data cannot be written to that frame range, WriteLock returns std::nullopt.
  // Otherwise, WriteLock returns a buffer representing all or part of the requested range.
  // If |start()| on the returned buffer is greater than |dest_frame|, then no frames before
  // |start()| must be written. Conversely, if |end()| on the returned buffer is less than
  // |dest_frame + frame_count|, this does not indicate those frames can be omitted. Instead
  // it indicates the full frame range is not available on a single contiguous buffer. Clients
  // should call |WriteLock| again and provide the |end()| of the previous buffer as |dest_frame|
  // to query if the stream has more frames.
  //
  // The returned buffer must not refer to frames outside of the range [frame, frame + frame_count).
  //
  // Callers can write directly to the payload. The buffer will remain locked until it is
  // destructed. It is illegal to call WriteLock again until the lock has been released.
  virtual std::optional<Buffer> WriteLock(int64_t frame, int64_t frame_count) = 0;
};

}  // namespace media::audio

#endif  // SRC_MEDIA_AUDIO_AUDIO_CORE_STREAM_H_
