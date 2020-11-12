// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include <errno.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "OrbitBase/Logging.h"
#include "OrbitBase/Result.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/flags/usage_config.h"
#include "absl/strings/str_format.h"


ABSL_FLAG(uint32_t, pid, 0, "PID of the process to instrument.");

// Tracee neds to be stoped before calling this.
void DetachAndContinue(uint32_t pid) {
  ptrace(PTRACE_DETACH, pid, nullptr, 0);
}

[[nodiscard]] ErrorMessageOr<void> AttachAndStop(uint32_t pid) {
  LOG("Starting instrumentation of process with pid: %i", pid);   

  if (ptrace(PTRACE_ATTACH, pid, 0, 0) < 0) {
    return ErrorMessage(absl::StrFormat("Can not attach to pid \"%d\": \"%s\"", pid, strerror(errno)));
  }

  // Wait for the traced process to stop. Timeout after one second.
  int timeout = 1000;
  while (true) {
    const int result_waitpid = waitpid(pid, nullptr, WNOHANG);
    if (result_waitpid != 0) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (--timeout == 0) { 
      DetachAndContinue(pid);
      return ErrorMessage("Waiting for the traced process to stop timed out.");
    }
  }
  return outcome::success();
}

// Prototype for experimenting with user space instrumentation
int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  const uint32_t pid = absl::GetFlag(FLAGS_pid);
  if (pid == 0) {
    FATAL("pid must not be 0.");
  }

  auto result = AttachAndStop(pid);
  if (result.has_error()) {
    FATAL("%s", result.error().message());
  }
  
  std::this_thread::sleep_for(std::chrono::milliseconds(10000));

  DetachAndContinue(pid);

  std::this_thread::sleep_for(std::chrono::milliseconds(10000));

  return 0;
}
