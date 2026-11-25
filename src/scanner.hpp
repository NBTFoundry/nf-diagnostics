// SPDX-License-Identifier: MPL-2.0
#ifndef NF_DIAGNOSTICS_SCANNER_HPP
#define NF_DIAGNOSTICS_SCANNER_HPP

#include <pthread.h>
#include <stdint.h>

#include "nbt/app/diag.hpp"
#include "units.hpp"

namespace diagnostics {

/* Reads and clears fault codes through NBT Manager's diagnostics (capability
 * `diag`) on a thread of its own: each request waits for the car. One job
 * at a time, in the order asked; asking for what is already queued adds
 * nothing. */
class Scanner {
 public:
  Scanner();
  ~Scanner();
  /* False without the capability (and then every job is ignored). */
  bool start();
  void stop();
  bool available() const { return running_; }

  void read_all();
  void read(unsigned unit);
  /* Clears every code of `unit`, then reads it again. */
  void clear(unsigned unit);

  /* The results; the returned number grows with every change. */
  uint64_t snapshot(UnitResult out[kUnitCount]);
  bool busy();

 private:
  enum Kind { kRead, kClear };
  struct Job {
    Kind kind;
    unsigned unit;
    bool after_clear;  // a read that shows what a clear left
  };
  enum { kQueue = 2 * kUnitCount };

  Scanner(const Scanner&);
  Scanner& operator=(const Scanner&);
  static void* thread_main(void* self);
  void loop();
  void push(Kind kind, unsigned unit, bool after_clear = false);
  void run(const Job& job);
  void set(unsigned unit, const UnitResult& result);

  nbt::app::Diag diag_;
  pthread_t thread_;
  pthread_mutex_t mutex_;
  pthread_cond_t wake_;
  bool running_, quit_, working_;
  Job queue_[kQueue];
  unsigned head_, count_;
  UnitResult results_[kUnitCount];
  uint64_t version_;
};

}  // namespace diagnostics

#endif
