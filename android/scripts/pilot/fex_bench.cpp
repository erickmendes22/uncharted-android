// SPDX-License-Identifier: MIT
// fex-bench: the Remill pilot's tests (guest.x86.elf: guest.c for x86-64 -march=btver2) run under
// FEXCore with the port's configuration, timed like the pilot's runner (best of N calls), to set
// FEX's JIT against the native and the lifted (AOT) code of the same tests.
// Build: SMOKE=$PWD/fex_bench.cpp bash ../port-arm64/build_fex_android.sh (the fexcore-smoke target,
// ~/src/fex-android/Bin/fexcore-smoke). The test addresses are those of ~/src/pilot/lifted/functions.txt.
// Run on the G2: LD_LIBRARY_PATH=. ./fexcore-smoke <reps> guest.x86.elf
#include "Common/Config.h"
#include "Common/HostFeatures.h"
#include <FEXCore/Config/Config.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/SignalDelegator.h>
#include <FEXCore/Core/X86Enums.h>
#include <FEXCore/Debug/InternalThreadState.h>
#include <FEXCore/HLE/SyscallHandler.h>

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
using u64 = uint64_t;
using u32 = uint32_t;

struct Saxpy {
  float a;
  u32 n;
  float* x;
  float* y;
};
struct Dbl {
  const double* v;
  u64 n;
};

constexpr int N = 4096;
u64 data[N], cell;
float fx[N], fy[N], fy0[N];
double dv[N];
u64 rng = 0x243F6A8885A308D3ull;
u64 Next() {
  rng ^= rng << 13;
  rng ^= rng >> 7;
  rng ^= rng << 17;
  return rng;
}
double Now() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void MapGuestImage(const char* path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    perror(path);
    exit(1);
  }
  Elf64_Ehdr eh;
  if (pread(fd, &eh, sizeof eh, 0) != sizeof eh) exit(1);
  for (int i = 0; i < eh.e_phnum; i++) {
    Elf64_Phdr ph;
    pread(fd, &ph, sizeof ph, static_cast<off_t>(eh.e_phoff + static_cast<u64>(i) * eh.e_phentsize));
    if (ph.p_type != PT_LOAD) continue;
    const u64 page = static_cast<u64>(sysconf(_SC_PAGESIZE));
    const u64 lo = ph.p_vaddr & ~(page - 1), hi = (ph.p_vaddr + ph.p_memsz + page - 1) & ~(page - 1);
    void* p = mmap(reinterpret_cast<void*>(lo), hi - lo, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p != reinterpret_cast<void*>(lo)) {
      perror("guest mmap");
      exit(1);
    }
    pread(fd, reinterpret_cast<void*>(ph.p_vaddr), ph.p_filesz, static_cast<off_t>(ph.p_offset));
  }
  close(fd);
}

class BenchSyscallHandler final : public FEXCore::HLE::SyscallHandler {
public:
  BenchSyscallHandler() {
    OSABI = FEXCore::HLE::SyscallOSABI::OS_GENERIC;
  }
  uint64_t HandleSyscall(FEXCore::Core::CpuStateFrame*, FEXCore::HLE::SyscallArguments*) override {
    return 0;
  }
  FEXCore::HLE::ExecutableRangeInfo QueryGuestExecutableRange(FEXCore::Core::InternalThreadState*, uint64_t) override {
    return {0, std::numeric_limits<uint64_t>::max(), true};
  }
  std::optional<FEXCore::ExecutableFileSectionInfo> LookupExecutableFileSection(FEXCore::Core::InternalThreadState*, uint64_t) override {
    return std::nullopt;
  }
};

class BenchSignalDelegator final : public FEXCore::SignalDelegator {
public:
  explicit BenchSignalDelegator(uintptr_t callbackReturn)
    : CallbackReturn {callbackReturn} {}
  uintptr_t GetThunkCallbackRET() const override {
    return CallbackReturn;
  }

private:
  uintptr_t CallbackReturn;
};

struct Test {
  const char* name;
  u64 addr; // ~/src/pilot/lifted/functions.txt
};
constexpr Test kTests[] = {
  {"t_carry", 0x800001200},    {"t_flags", 0x8000012d0},  {"t_bits", 0x800001350},  {"t_saxpy", 0x8000013a0},
  {"t_simd_int", 0x8000014f0}, {"t_switch", 0x800001570}, {"t_atomic", 0x800001610}, {"t_double", 0x800001650},
};
} // namespace

int main(int argc, char** argv) {
  const int reps = argc > 1 ? atoi(argv[1]) : 200;
  MapGuestImage(argc > 2 ? argv[2] : "guest.x86.elf");
  for (int i = 0; i < N; i++) {
    data[i] = Next();
    fx[i] = static_cast<float>(Next() % 1000) / 37.0f;
    fy0[i] = static_cast<float>(Next() % 1000) / 91.0f;
    dv[i] = static_cast<double>(static_cast<int64_t>(Next())) / 9.2e15;
  }

  // Guest stack, the return stub (hlt) and an empty function (ret) for the call overhead.
  const size_t stackSize = 1 << 20;
  auto* stack = static_cast<uint8_t*>(mmap(nullptr, stackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  auto* stub = static_cast<uint8_t*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  stub[0] = 0xf4; // hlt
  stub[16] = 0xc3; // ret
  const u64 hltAddr = reinterpret_cast<u64>(stub), retAddr = hltAddr + 16;
  const u64 stackTop = reinterpret_cast<u64>(stack) + stackSize - 64;

  FEX::Config::InitializeConfigs(FEX::Config::PortableInformation {});
  FEXCore::Config::Initialize();
  FEXCore::Config::AddLayer(FEX::Config::CreateEnvironmentLayer(environ));
  FEXCore::Config::Load();
  FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, "1");
  FEXCore::Config::Set(FEXCore::Config::CONFIG_DISABLETELEMETRY, "1");
  auto hostFeatures = FEX::FetchHostFeatures();
  auto context = FEXCore::Context::Context::CreateNewContext(hostFeatures);
  BenchSignalDelegator signalDelegator {hltAddr};
  auto syscallHandler = fextl::make_unique<BenchSyscallHandler>();
  context->SetSignalDelegator(&signalDelegator);
  context->SetSyscallHandler(syscallHandler.get());
  context->EnableExitOnHLT();
  if (!context->InitCore()) {
    std::fprintf(stderr, "InitCore failed\n");
    return 1;
  }
  const size_t callRetAlloc = FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE + 2 * 4096;
  auto* callRet = static_cast<uint8_t*>(mmap(nullptr, callRetAlloc, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  mprotect(callRet + 4096, FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE, PROT_READ | PROT_WRITE);
  auto* thread = context->CreateThread(retAddr, stackTop);
  thread->CallRetStackBase = callRet + 4096;
  thread->CurrentFrame->State.callret_sp =
    reinterpret_cast<u64>(callRet + 4096) + FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE / 4;
  std::array<FEXCore::Core::CPUState::gdt_segment, 32> gdt {};
  auto& state = thread->CurrentFrame->State;
  state.segment_arrays[FEXCore::Core::CPUState::SEGMENT_ARRAY_INDEX_GDT] = gdt.data();
  state.segment_arrays[FEXCore::Core::CPUState::SEGMENT_ARRAY_INDEX_LDT] = gdt.data();
  state.cs_idx = FEXCore::Core::CPUState::DEFAULT_USER_CS << 3;
  auto* codeSegment = FEXCore::Core::CPUState::GetSegmentFromIndex(state, state.cs_idx);
  FEXCore::Core::CPUState::SetGDTBase(codeSegment, 0);
  FEXCore::Core::CPUState::SetGDTLimit(codeSegment, 0xF'FFFFU);
  state.cs_cached = FEXCore::Core::CPUState::CalculateGDTBase(*codeSegment);
  codeSegment->L = 1;
  codeSegment->D = 0;

  auto call = [&](u64 rip, u64 a, u64 b) {
    state.rip = rip;
    state.gregs[FEXCore::X86State::REG_RDI] = a;
    state.gregs[FEXCore::X86State::REG_RSI] = b;
    state.gregs[FEXCore::X86State::REG_RSP] = stackTop - 8;
    std::memcpy(reinterpret_cast<void*>(stackTop - 8), &hltAddr, 8);
    context->ExecuteThread(thread);
    return state.gregs[FEXCore::X86State::REG_RAX];
  };

  double best = 1e9;
  for (int k = 0; k < reps; k++) {
    const double t0 = Now();
    call(retAddr, 0, 0);
    best = std::min(best, Now() - t0);
  }
  std::printf("%-11s %16s %9.2f us (the cost of one entry)\n", "empty", "", best * 1e6);
  for (const Test& t : kTests) {
    Saxpy s {1.25f, N, fx, fy};
    Dbl d {dv, N};
    u64 a = reinterpret_cast<u64>(data), b = N;
    if (!std::strcmp(t.name, "t_saxpy")) a = reinterpret_cast<u64>(&s);
    if (!std::strcmp(t.name, "t_double")) a = reinterpret_cast<u64>(&d);
    if (!std::strcmp(t.name, "t_atomic")) {
      a = reinterpret_cast<u64>(&cell);
      b = 1000;
    }
    u64 r = 0;
    best = 1e9;
    for (int k = 0; k < reps; k++) {
      std::memcpy(fy, fy0, sizeof fy);
      cell = 7;
      const double t0 = Now();
      r = call(t.addr, a, b);
      best = std::min(best, Now() - t0);
    }
    std::printf("%-11s %016llx %9.1f us\n", t.name, static_cast<unsigned long long>(r), best * 1e6);
  }
  context->DestroyThread(thread);
  return 0;
}
