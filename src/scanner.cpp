// SPDX-License-Identifier: MPL-2.0
#include "scanner.hpp"

#include <string.h>

#include "nbt/app/log.hpp"

namespace diagnostics {

namespace {

UnitResult::State state_of(nbt::app::Diag::Status status) {
  switch (status) {
    case nbt::app::Diag::kTimeout:
      return UnitResult::kNoAnswer;
    case nbt::app::Diag::kRefused:
      return UnitResult::kRefused;
    case nbt::app::Diag::kNoRoute:
      return UnitResult::kNoRoute;
    case nbt::app::Diag::kBusy:
      return UnitResult::kBusy;
    case nbt::app::Diag::kNotParked:
      return UnitResult::kNotParked;
    default:
      return UnitResult::kFailed;
  }
}

}  // namespace

Scanner::Scanner()
    : thread_(),
      running_(false),
      quit_(false),
      working_(false),
      queue_(),
      head_(0),
      count_(0),
      version_(0) {
  pthread_mutex_init(&mutex_, NULL);
  pthread_cond_init(&wake_, NULL);
  memset(results_, 0, sizeof(results_));
}

Scanner::~Scanner() {
  stop();
  pthread_cond_destroy(&wake_);
  pthread_mutex_destroy(&mutex_);
}

bool Scanner::start() {
  if (running_ || !diag_.init()) return false;
  quit_ = false;
  if (pthread_create(&thread_, NULL, &Scanner::thread_main, this)) return false;
  running_ = true;
  return true;
}

void Scanner::stop() {
  if (!running_) return;
  pthread_mutex_lock(&mutex_);
  quit_ = true;
  pthread_cond_signal(&wake_);
  pthread_mutex_unlock(&mutex_);
  // A request in flight ends within its timeout and margin.
  pthread_join(thread_, NULL);
  running_ = false;
}

void* Scanner::thread_main(void* self) {
  static_cast<Scanner*>(self)->loop();
  return NULL;
}

void Scanner::push(Kind kind, unsigned unit, bool after_clear) {
  if (!running_ || unit >= kUnitCount) return;
  pthread_mutex_lock(&mutex_);
  bool queued = false;
  for (unsigned i = 0; i < count_; ++i) {
    const Job& job = queue_[(head_ + i) % kQueue];
    queued = queued || (job.kind == kind && job.unit == unit && job.after_clear == after_clear);
  }
  if (!queued && count_ < kQueue) {
    queue_[(head_ + count_++) % kQueue] = Job{kind, unit, after_clear};
    results_[unit].state = kind == kRead ? UnitResult::kReading : UnitResult::kClearing;
    ++version_;
    pthread_cond_signal(&wake_);
  }
  pthread_mutex_unlock(&mutex_);
}

void Scanner::read_all() {
  for (unsigned i = 0; i < kUnitCount; ++i) push(kRead, i);
}

void Scanner::read(unsigned unit) {
  push(kRead, unit);
}

void Scanner::clear(unsigned unit) {
  push(kClear, unit);
  push(kRead, unit, true);
}

void Scanner::loop() {
  pthread_mutex_lock(&mutex_);
  while (!quit_) {
    if (!count_) {
      pthread_cond_wait(&wake_, &mutex_);
      continue;
    }
    const Job job = queue_[head_];
    head_ = (head_ + 1) % kQueue;
    --count_;
    working_ = true;
    pthread_mutex_unlock(&mutex_);
    run(job);
    pthread_mutex_lock(&mutex_);
    working_ = false;
  }
  pthread_mutex_unlock(&mutex_);
}

void Scanner::run(const Job& job) {
  const Unit& unit = kUnits[job.unit];
  UnitResult result;
  memset(&result, 0, sizeof(result));
  if (job.kind == kClear) {
    const nbt::app::Diag::Status status = diag_.clear(unit.address, &result.nrc);
    result.state = status != nbt::app::Diag::kOk ? state_of(status)
                   : result.nrc                  ? UnitResult::kRefused
                                                 : UnitResult::kReading;
    result.cleared = status == nbt::app::Diag::kOk && !result.nrc;
    nbt::app::log_info("%s: clear: %s (nrc %02X)", unit.name, result.cleared ? "done" : "not done",
                       result.nrc);
    if (!result.cleared) {
      // The read queued after it would hide why: drop it.
      pthread_mutex_lock(&mutex_);
      if (count_ && queue_[head_].kind == kRead && queue_[head_].unit == job.unit) {
        head_ = (head_ + 1) % kQueue;
        --count_;
      }
      pthread_mutex_unlock(&mutex_);
    }
    set(job.unit, result);
    return;
  }
  uint8_t answer[4 * 255 + 1];
  size_t size = 0;
  const nbt::app::Diag::Status status =
      diag_.codes(unit.address, 0xFF, answer, sizeof(answer), &size, &result.nrc);
  if (status != nbt::app::Diag::kOk) {
    result.state = state_of(status);
  } else if (result.nrc) {
    result.state = UnitResult::kRefused;
  } else {
    result.state = UnitResult::kRead;
    result.count = parse_codes(answer, size, result.codes, kCodesMax, &result.total);
  }
  result.cleared = job.after_clear;
  nbt::app::log_info("%s: read: state %d, %u codes", unit.name, static_cast<int>(result.state),
                     static_cast<unsigned>(result.total));
  set(job.unit, result);
}

void Scanner::set(unsigned unit, const UnitResult& result) {
  pthread_mutex_lock(&mutex_);
  // A job still queued for it keeps its state on screen.
  UnitResult::State queued = result.state;
  for (unsigned i = 0; i < count_; ++i) {
    const Job& job = queue_[(head_ + i) % kQueue];
    if (job.unit == unit) queued = job.kind == kRead ? UnitResult::kReading : UnitResult::kClearing;
  }
  results_[unit] = result;
  if (queued != result.state && result.state != UnitResult::kNotParked &&
      result.state != UnitResult::kRefused) {
    results_[unit].state = queued;
  }
  ++version_;
  pthread_mutex_unlock(&mutex_);
}

uint64_t Scanner::snapshot(UnitResult out[kUnitCount]) {
  pthread_mutex_lock(&mutex_);
  memcpy(out, results_, sizeof(results_));
  const uint64_t version = version_;
  pthread_mutex_unlock(&mutex_);
  return version;
}

bool Scanner::busy() {
  pthread_mutex_lock(&mutex_);
  const bool busy = working_ || count_;
  pthread_mutex_unlock(&mutex_);
  return busy;
}

}  // namespace diagnostics
