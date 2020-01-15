//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------

//-----------------------------------
// Author: Florian Kuebler
//-----------------------------------

#include "LinuxUtils.h"
#include "PrintVar.h"
#include "LinuxPerfUtils.h"
#include "ScopeTimer.h"

#include "PerfEvent.h"
#include <linux/version.h>
#include <sys/errno.h>

//-----------------------------------------------------------------------------
perf_event_attr LinuxPerfUtils::generic_perf_event_attr()
{
    perf_event_attr pe{};
    pe.size = sizeof(struct perf_event_attr);
    pe.sample_period = 1;
    pe.use_clockid = 1;
    pe.clockid = CLOCK_MONOTONIC;
    pe.sample_id_all = 1; // also include timestamps for lost events
    pe.disabled = 1;

    // we can set these even if we do not do sampling, as without the flag being
    // set in sample_type, they won't be used anyways.
    pe.sample_stack_user = STACK_SIZE;
    pe.sample_regs_user = SAMPLE_REGS_USER;

    pe.sample_type = SAMPLE_TYPE_FLAGS;

    return pe;
}

//-----------------------------------------------------------------------------
int32_t LinuxPerfUtils::task_event_open(int32_t a_CPU) {
    perf_event_attr pe = generic_perf_event_attr();
    pe.type = PERF_TYPE_SOFTWARE;
    pe.config = PERF_COUNT_SW_DUMMY;
    pe.task = 1;

    int32_t fd = perf_event_open(&pe, -1, a_CPU, -1 /*group_fd*/, 0 /*flags*/);

    if (fd == -1)
    {
        PRINT("perf_event_open error: %d\n", errno);
    }

    return fd;
}

//-----------------------------------------------------------------------------
int32_t LinuxPerfUtils::context_switch_open(pid_t a_PID, int32_t a_CPU)
{
    perf_event_attr pe = generic_perf_event_attr();
    pe.type = PERF_TYPE_SOFTWARE;
    pe.config = PERF_COUNT_SW_DUMMY;
    pe.context_switch = 1;

    int32_t fd = perf_event_open(&pe, a_PID, a_CPU, -1 /*group_fd*/, 0 /*flags*/);

    if (fd == -1)
    {
        PRINT("perf_event_open error: %d\n", errno);
    }

    return fd;
}

//-----------------------------------------------------------------------------
int32_t LinuxPerfUtils::tracepoint_event_open(
    uint64_t a_TracepointID, 
    pid_t a_PID, 
    int32_t a_CPU, 
    uint64_t additonal_sample_type
)
{
    perf_event_attr pe = generic_perf_event_attr();
    pe.type = PERF_TYPE_TRACEPOINT;
    pe.config = a_TracepointID;
    pe.sample_type |= PERF_SAMPLE_RAW;
    pe.sample_type |= additonal_sample_type;

    int32_t fd = perf_event_open(&pe, a_PID, a_CPU, -1 /*grpup_fd*/, 0 /*flags*/);

    if (fd == -1)
    {
        PRINT("perf_event_open error: %d\n", errno);
    }

    return fd;
}

//-----------------------------------------------------------------------------
bool LinuxPerfUtils::supports_perf_event_uprobes()
{
    return LinuxUtils::GetKernelVersion() >= KERNEL_VERSION(4,17,0);
}

//-----------------------------------------------------------------------------
int32_t LinuxPerfUtils::uprobe_event_open(
    const char* a_Module,
    uint64_t a_FunctionOffset,
    pid_t a_PID,
    int32_t a_CPU,
    int32_t a_GroupFd,
    uint64_t additonal_sample_type
)
{
    SCOPE_TIMER_FUNC;
    PRINT_VAR(a_Module);
    PRINT_VAR(a_FunctionOffset);
    PRINT_VAR(a_PID);
    PRINT_VAR(a_CPU);
    PRINT_VAR(a_GroupFd);
    PRINT_VAR(additonal_sample_type);
    
    perf_event_attr pe = generic_perf_event_attr();

    pe.type = 7; // TODO: should be read from "/sys/bus/event_source/devices/uprobe/type"
    pe.config = 0;
    pe.config1 = reinterpret_cast<uint64_t>(a_Module); // pe.config1 == pe.uprobe_path
    pe.config2 = a_FunctionOffset;                     // pe.config2 == pe.probe_offset
    pe.sample_type |= additonal_sample_type;

    int32_t fd = perf_event_open(&pe, a_PID, a_CPU, a_GroupFd, 0 /*flags*/);

    if (fd == -1)
    {
        PRINT("perf_event_open error: %d\n", errno);
    }

    PRINT_VAR(fd);
    return fd;
}

//-----------------------------------------------------------------------------
int32_t LinuxPerfUtils::uretprobe_event_open(
    const char* a_Module,
    uint64_t a_FunctionOffset, 
    pid_t a_PID,
    int32_t a_CPU,
    int32_t a_GroupFd,
    uint64_t additonal_sample_type
)
{
    SCOPE_TIMER_FUNC;
    PRINT_VAR(a_Module);
    PRINT_VAR(a_FunctionOffset);
    PRINT_VAR(a_PID);
    PRINT_VAR(a_CPU);
    PRINT_VAR(a_GroupFd);
    PRINT_VAR(additonal_sample_type);

    perf_event_attr pe = generic_perf_event_attr();

    pe.type = 7; // TODO: should be read from "/sys/bus/event_source/devices/uprobe/type"
    pe.config = 1;
    pe.config1 = reinterpret_cast<uint64_t>(a_Module); // pe.config1 == pe.uprobe_path
    pe.config2 = a_FunctionOffset;                     // pe.config2 == pe.probe_offset
    pe.sample_type |= additonal_sample_type;

    int32_t fd = perf_event_open(&pe, a_PID, a_CPU, a_GroupFd, 0 /*flags*/);

    if (fd == -1)
    {
        PRINT("perf_event_open error: %d\n", errno);
    }

    PRINT_VAR(fd);
    return fd;
}
