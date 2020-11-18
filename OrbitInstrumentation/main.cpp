// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "OrbitBase/Logging.h"
#include "OrbitBase/Result.h"
#include "OrbitLinuxTracing/LinuxTracingUtils.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/flags/usage_config.h"
#include "absl/strings/str_format.h"

ABSL_FLAG(uint32_t, pid, 0, "PID of the process to instrument.");

using LinuxTracing::GetTidsOfProcess;

void DetachAndContinueProcess(pid_t pid) {
  auto process_tids = GetTidsOfProcess(pid);
  for (auto tid : process_tids) {
    ptrace(PTRACE_DETACH, tid, nullptr, 0);
  }
}

[[nodiscard]] ErrorMessageOr<void> AttachAndStopThread(pid_t tid) {
  LOG("Attach to thread : %i", tid);

  if (ptrace(PTRACE_ATTACH, tid, 0, 0) < 0) {
    return ErrorMessage(
        absl::StrFormat("Can not attach to tid \"%d\": \"%s\"", tid, strerror(errno)));
  }

  // Wait for the traced thread to stop. Timeout after one second.
  int timeout = 1000;
  while (true) {
    const int result_waitpid = waitpid(tid, nullptr, WNOHANG);
    if (result_waitpid != 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (--timeout == 0) {
      ptrace(PTRACE_DETACH, tid, nullptr, 0);
      return ErrorMessage("Waiting for the traced thread to stop timed out.");
    }
  }
  return outcome::success();
}

[[nodiscard]] ErrorMessageOr<void> AttachAndStopProcess(pid_t pid) {
  bool all_threads_halted = false;
  auto process_tids = GetTidsOfProcess(pid);
  std::set<pid_t> halted_tids;
  while (!all_threads_halted) {
    // Note that the process is still running - it can spawn and end threads at this point.
    for (auto tid : process_tids) {
      if (halted_tids.count(tid) == 1) {
        continue;
      }
      auto result = AttachAndStopThread(tid);
      if (result.has_error()) {
        // If the tid does not exist anymore this is fine; the thread was just ended before we could
        // stop it. Otherwise we return the error.
        const auto tids_now = GetTidsOfProcess(pid);
        for (const auto& t : tids_now) {
          if (t == tid) {
            return result.error();
          }
        }
      } else {
        halted_tids.insert(tid);
      }
    }
    // Check if there were new tids; if so run another round and stop them.
    process_tids = GetTidsOfProcess(pid);
    if (process_tids.size() == halted_tids.size()) {
      all_threads_halted = true;
    }
  }
  return outcome::success();
}

// Prototype for experimenting with user space instrumentation
int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  const pid_t pid = absl::GetFlag(FLAGS_pid);
  if (pid == 0) {
    FATAL("pid must not be 0.");
  }

  auto result = AttachAndStopProcess(pid);
  if (result.has_error()) {
    FATAL("%s", result.error().message());
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10000));

  DetachAndContinueProcess(pid);

  std::this_thread::sleep_for(std::chrono::milliseconds(10000));

  return 0;
}
