// ps4lift: lifts a piece of a PS4 module's functions (x86-64) through Remill into LLVM bitcode for aarch64.
//
//   ps4lift --seg eboot.seg0.bin --seg_vaddr 0 --load_base 0x800000000 --ranges eboot.ranges \
//           --first 0 --count 500 --out chunk0000.bc
//
// Functions come from .eh_frame (scripts/inventory/extract_code.py -> *.ranges, vaddr relative to the
// image). Addresses in the IR are runtime addresses (load_base + vaddr), as the guest code sees them.
// A function of this piece is an external `sub_<addr>`; a function of another piece is an external declaration;
// the other trace heads (jump targets, not function starts) are internal, so there are no
// duplicates when the pieces are linked.
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Scalar/ADCE.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CFG.h>
#include <algorithm>
#include <functional>
#include <remill/Arch/Arch.h>
#include <remill/BC/IntrinsicTable.h>
#include <remill/BC/Lifter.h>
#include <remill/BC/Optimizer.h>
#include <remill/BC/TraceLifter.h>
#include <remill/BC/Util.h>

extern "C" {
#include <xed/xed-interface.h>
}

#include <chrono>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

DEFINE_string(seg, "", "executable segment bytes");
DEFINE_uint64(seg_vaddr, 0, "vaddr of the segment in the image");
DEFINE_uint64(load_base, 0x800000000ull, "runtime address of image vaddr 0");
DEFINE_string(ranges, "", "function list: '<start_hex> <size_hex>' per line (image vaddr)");
DEFINE_uint64(first, 0, "first function index in --ranges");
DEFINE_uint64(count, 500, "number of functions in this chunk");
DEFINE_string(out, "", "output bitcode");
DEFINE_string(arch, "amd64_avx", "remill arch");
DEFINE_string(image, "", "flat image (vaddr 0..size) for data reads: switch tables in .rodata");
DEFINE_bool(switch_tables, true, "recover clang PIC jump tables and lift their targets as traces");
DEFINE_string(table_prefix, "ps4", "prefix of the per-chunk trace table symbols");
DEFINE_bool(stub_x87, true, "replace traces using x86_fp80 with __remill_error stubs");
DEFINE_bool(lower_x87, true, "lower x86_fp80 to fp128 (needed for aarch64)");
DEFINE_bool(abi_dead_state, true, "flags/scratch registers are dead at ret and calls (SysV)");
DEFINE_bool(plain_stack, true, "RSP-relative guest accesses without TSO ordering");
DEFINE_string(state_module, "",
              "write a module with State accessors (ps4_reg_<NAME>, ps4_state_size) and exit");
DEFINE_string(target_triple, "aarch64-unknown-linux-gnu", "");
DEFINE_string(target_datalayout,
              "e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128-Fn32",
              "");

namespace {

struct ChunkTraceManager : remill::TraceManager {
  const remill::Arch *arch;
  llvm::Module *module;
  const std::vector<uint8_t> &seg;
  uint64_t seg_lo, seg_hi;
  const std::set<uint64_t> &all_funcs;    // every function start (runtime address)
  const std::set<uint64_t> &chunk_funcs;  // starts lifted in this chunk
  std::unordered_map<uint64_t, llvm::Function *> traces;
  std::map<uint64_t, std::vector<uint64_t>> jump_tables;  // jmp pc -> targets (same function)

  // Recovered jump tables become a `switch` inside the trace (patch 0003 to TraceLifter):
  // no duplicated function tails, no dispatcher lookup per switch.
  void ForEachDevirtualizedTarget(
      const remill::Instruction &inst,
      std::function<void(uint64_t, remill::DevirtualizedTargetKind)> func) override {
    auto it = jump_tables.find(inst.pc);
    if (it == jump_tables.end()) return;
    for (auto t : it->second) func(t, remill::DevirtualizedTargetKind::kTraceLocal);
  }

  ChunkTraceManager(const remill::Arch *a, llvm::Module *m, const std::vector<uint8_t> &s,
                    uint64_t lo, const std::set<uint64_t> &all, const std::set<uint64_t> &chunk)
      : arch(a), module(m), seg(s), seg_lo(lo), seg_hi(lo + s.size()), all_funcs(all),
        chunk_funcs(chunk) {}

  void SetLiftedTraceDefinition(uint64_t addr, llvm::Function *f) override { traces[addr] = f; }

  // TraceLifter lifts every work-list entry (e.g. direct call targets) for which this returns
  // null. Functions owned by other chunks must not be lifted here: hand back their declaration.
  llvm::Function *GetLiftedTraceDefinition(uint64_t addr) override {
    auto it = traces.find(addr);
    if (it != traces.end()) return it->second;
    if (all_funcs.count(addr) && !chunk_funcs.count(addr)) return GetLiftedTraceDeclaration(addr);
    return nullptr;  // ours and not lifted yet
  }

  // Every function start gets a declaration: a direct jump into another function (tail call)
  // then becomes a tail call to its trace instead of a copy of its body in every jumping trace
  // (which blew 83-byte functions up to 33 KiB of ARM code).
  llvm::Function *GetLiftedTraceDeclaration(uint64_t addr) override {
    if (all_funcs.count(addr)) {
      auto name = TraceName(addr);
      if (auto f = module->getFunction(name)) return f;
      return arch->DeclareLiftedFunction(name, module);
    }
    return nullptr;
  }

  bool TryReadExecutableByte(uint64_t addr, uint8_t *byte) override {
    if (addr < seg_lo || addr >= seg_hi) return false;
    if (byte) *byte = seg[addr - seg_lo];
    return true;
  }
};

size_t CountCalls(llvm::Module &m, const char *name) {
  auto f = m.getFunction(name);
  if (!f) return 0;
  size_t n = 0;
  for (auto *u : f->users())
    if (llvm::isa<llvm::CallBase>(u)) ++n;
  return n;
}

std::vector<uint8_t> g_image;

std::vector<std::string> &pending_renames() {
  static std::vector<std::string> v;
  return v;
}

bool ReadImage32(uint64_t rt_addr, int32_t *out) {
  uint64_t off = rt_addr - FLAGS_load_base;
  if (rt_addr < FLAGS_load_base || off + 4 > g_image.size()) return false;
  std::memcpy(out, &g_image[off], 4);
  return true;
}

// clang PIC jump table: lea rT, [rip+T]; movsxd rX, dword [rT+idx*4]; add rX, rT; jmp rX.
// Entries are int32 offsets from T; accept them while the target stays inside the function.
// Fills `tables` with {pc of the indirect jmp -> targets}.
size_t RecoverSwitchTargets(const std::vector<uint8_t> &seg, uint64_t seg_lo, uint64_t fstart,
                            uint64_t fsize, std::map<uint64_t, std::vector<uint64_t>> &tables) {
  size_t found = 0;
  const uint64_t fend = fstart + fsize;
  uint64_t lea_target = 0;
  int since_lea = 1000;
  for (uint64_t pc = fstart; pc < fend;) {
    if (pc < seg_lo || pc >= seg_lo + seg.size()) break;
    xed_decoded_inst_t x;
    xed_decoded_inst_zero(&x);
    xed_decoded_inst_set_mode(&x, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
    unsigned avail = static_cast<unsigned>(std::min<uint64_t>(15, fend - pc));
    if (xed_decode(&x, &seg[pc - seg_lo], avail) != XED_ERROR_NONE) {
      ++pc;
      continue;
    }
    unsigned len = xed_decoded_inst_get_length(&x);
    auto iclass = xed_decoded_inst_get_iclass(&x);
    ++since_lea;
    if (iclass == XED_ICLASS_LEA && xed_decoded_inst_get_base_reg(&x, 0) == XED_REG_RIP) {
      lea_target = pc + len + static_cast<int64_t>(xed_decoded_inst_get_memory_displacement(&x, 0));
      since_lea = 0;
    } else if (iclass == XED_ICLASS_JMP && xed_decoded_inst_number_of_memory_operands(&x) == 0 &&
               xed_decoded_inst_get_reg(&x, XED_OPERAND_REG0) != XED_REG_INVALID && since_lea <= 6) {
      for (uint64_t i = 0; i < 4096; ++i) {
        int32_t e;
        if (!ReadImage32(lea_target + 4 * i, &e)) break;
        uint64_t t = lea_target + static_cast<int64_t>(e);
        if (t < fstart || t >= fend) break;
        tables[pc].push_back(t);
        ++found;
      }
      since_lea = 1000;
    }
    pc += len;
  }
  return found;
}

// SysV x86-64: flags are undefined after `ret` and on entry to a callee; caller-saved registers
// other than the return registers are undefined after `ret`. Remill does not know the ABI, so
// every trace writes all flags (with PF parity chains) and scratch registers back to State on
// exit. After -O2, walk back from each return/call and delete stores to those State fields that
// are not read again before it; a cleanup then drops the computations feeding them.
// (`store undef` does not work: InstCombine deletes it before DSE can use it.)
// Indirect jumps are left alone (flags may be live across a switch dispatch).
struct Field { int64_t off; uint64_t size; };

std::vector<Field> StateFields(llvm::Module &m, const remill::Arch *arch,
                               std::initializer_list<const char *> names) {
  auto &ctx = m.getContext();
  auto *ptr_ty = llvm::PointerType::get(ctx, 0);
  auto *probe = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {ptr_ty}, false),
                                       llvm::GlobalValue::InternalLinkage, "__ps4_state_probe", &m);
  llvm::IRBuilder<> ir(llvm::BasicBlock::Create(ctx, "", probe));
  const auto &dl = m.getDataLayout();
  std::vector<Field> out;
  for (auto name : names)
    if (auto *reg = arch->RegisterByName(name)) {
      llvm::APInt off(64, 0);
      auto *base = reg->AddressOf(probe->getArg(0), ir)->stripAndAccumulateConstantOffsets(dl, off, true);
      if (base == probe->getArg(0)) out.push_back({off.getSExtValue(), dl.getTypeStoreSize(reg->type)});
    }
  ir.CreateRetVoid();
  probe->eraseFromParent();
  return out;
}

void RunPipeline(llvm::Module &m, bool full) {
  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;
  llvm::PassBuilder pb;
  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cgam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cgam, mam);
  if (full) {
    pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2).run(m, mam);
  } else {
    llvm::FunctionPassManager fpm;
    fpm.addPass(llvm::ADCEPass());
    fpm.addPass(llvm::InstCombinePass());
    fpm.addPass(llvm::SimplifyCFGPass());
    llvm::ModulePassManager mpm;
    mpm.addPass(llvm::createModuleToFunctionPassAdaptor(std::move(fpm)));
    mpm.run(m, mam);
  }
}

// Thread-local stack accesses do not need TSO ordering or the unaligned check: guest memory
// accesses whose address is RSP (+/- constants, alignment masks, phis of such) — RSP as loaded
// from State — are redirected to __ps4_{read,write}_stack_N (plain ld/st in the runtime).
// Caveat: a stack slot whose address escapes to another thread loses ordering for these direct
// RSP-relative accesses (Lasagne-style escape analysis would close it). RBP is not trusted:
// it is a general register in frame-pointer-less functions.
size_t PlainStackAccesses(llvm::Module &m, const remill::Arch *arch) {
  auto rsp_field = StateFields(m, arch, {"RSP"});
  if (rsp_field.empty()) return 0;
  const auto &dl = m.getDataLayout();
  size_t n = 0;
  for (auto &f : m) {
    if (f.isDeclaration() || f.arg_size() != 3) continue;
    llvm::Value *state = f.getArg(0);
    std::map<llvm::Value *, bool> memo;
    std::function<bool(llvm::Value *, int)> is_stack = [&](llvm::Value *v, int depth) -> bool {
      if (depth > 24) return false;
      if (auto it = memo.find(v); it != memo.end()) return it->second;
      memo[v] = false;  // cycles through phis: assume no until proven
      bool r = false;
      if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(v)) {
        llvm::APInt off(64, 0);
        auto *base = ld->getPointerOperand()->stripAndAccumulateConstantOffsets(dl, off, true);
        r = base == state && off.getSExtValue() == rsp_field[0].off;
      } else if (auto *bo = llvm::dyn_cast<llvm::BinaryOperator>(v)) {
        auto op = bo->getOpcode();
        if ((op == llvm::Instruction::Add || op == llvm::Instruction::Sub || op == llvm::Instruction::Or ||
             op == llvm::Instruction::And) && llvm::isa<llvm::ConstantInt>(bo->getOperand(1)))
          r = is_stack(bo->getOperand(0), depth + 1);
        else if (op == llvm::Instruction::Add && llvm::isa<llvm::ConstantInt>(bo->getOperand(0)))
          r = is_stack(bo->getOperand(1), depth + 1);
      } else if (auto *phi = llvm::dyn_cast<llvm::PHINode>(v)) {
        memo[v] = true;  // optimistic for loops; any non-stack input refutes
        r = true;
        for (auto &in : phi->incoming_values())
          if (!is_stack(in, depth + 1)) { r = false; break; }
      } else if (auto *sel = llvm::dyn_cast<llvm::SelectInst>(v)) {
        r = is_stack(sel->getTrueValue(), depth + 1) && is_stack(sel->getFalseValue(), depth + 1);
      }
      memo[v] = r;
      return r;
    };
    for (auto &bb : f)
      for (auto &inst : bb) {
        auto *call = llvm::dyn_cast<llvm::CallInst>(&inst);
        if (!call || !call->getCalledFunction() || call->arg_size() < 2) continue;
        auto name = call->getCalledFunction()->getName();
        std::string repl;
        if (name.starts_with("__remill_read_memory_")) repl = "__ps4_read_stack_" + name.substr(21).str();
        else if (name.starts_with("__remill_write_memory_")) repl = "__ps4_write_stack_" + name.substr(22).str();
        else continue;
        if (repl.find("f80") != std::string::npos || repl.find("f128") != std::string::npos) continue;
        if (!is_stack(call->getArgOperand(1), 0)) continue;
        auto callee = m.getOrInsertFunction(repl, call->getFunctionType());
        call->setCalledFunction(callee);
        ++n;
      }
  }
  return n;
}

size_t RemoveAbiDeadStores(llvm::Module &m, const remill::Arch *arch) {
  auto flags = StateFields(m, arch, {"CF", "PF", "AF", "ZF", "SF", "OF"});
  auto scratch = StateFields(m, arch, {"RCX", "RSI", "RDI", "R8", "R9", "R10", "R11", "XMM2",
                                       "XMM3", "XMM4", "XMM5", "XMM6", "XMM7", "XMM8", "XMM9",
                                       "XMM10", "XMM11", "XMM12", "XMM13", "XMM14", "XMM15"});
  const auto &dl = m.getDataLayout();
  auto state_off = [&](llvm::Value *p, llvm::Value *state, int64_t &off) {
    llvm::APInt a(64, 0);
    auto *base = p->stripAndAccumulateConstantOffsets(dl, a, true);
    off = a.getSExtValue();
    return base == state;
  };
  size_t removed = 0;
  for (auto &f : m) {
    if (f.isDeclaration() || f.arg_size() != 3) continue;
    llvm::Value *state = f.getArg(0);
    std::vector<std::pair<llvm::CallInst *, bool>> sites;
    for (auto &bb : f)
      for (auto &inst : bb)
        if (auto *call = llvm::dyn_cast<llvm::CallInst>(&inst))
          if (auto *callee = call->getCalledFunction(); callee && call->arg_size() == 3) {
            auto name = callee->getName();
            if (name == "__remill_function_return") sites.emplace_back(call, true);
            else if (name == "__remill_function_call" || name.starts_with("sub_"))
              sites.emplace_back(call, false);
          }
    (void)sites;
    // Backward liveness of the tracked fields over the CFG (bit i = field i).
    std::vector<Field> fields(flags);
    fields.insert(fields.end(), scratch.begin(), scratch.end());
    const uint64_t all = (fields.size() >= 64) ? ~0ull : ((1ull << fields.size()) - 1);
    const uint64_t flag_bits = (1ull << flags.size()) - 1;
    auto field_bits = [&](int64_t off, uint64_t size, bool exact) {
      uint64_t b = 0;
      for (size_t k = 0; k < fields.size(); ++k) {
        auto &d = fields[k];
        if (exact ? (d.off == off && d.size == size)
                  : (off < d.off + (int64_t)d.size && d.off < off + (int64_t)size))
          b |= 1ull << k;
      }
      return b;
    };
    // Transfer one instruction backward; `dead_store` set when a store writes only dead fields.
    auto step = [&](llvm::Instruction &i, uint64_t live, bool *dead_store) -> uint64_t {
      int64_t off;
      if (auto *st = llvm::dyn_cast<llvm::StoreInst>(&i)) {
        if (!st->isVolatile() && state_off(st->getPointerOperand(), state, off)) {
          auto size = dl.getTypeStoreSize(st->getValueOperand()->getType());
          uint64_t b = field_bits(off, size, true);
          if (b && dead_store && !(live & b)) *dead_store = true;
          return live & ~b;
        }
      } else if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(&i)) {
        if (state_off(ld->getPointerOperand(), state, off))
          return live | field_bits(off, dl.getTypeStoreSize(ld->getType()), false);
      } else if (auto *cb = llvm::dyn_cast<llvm::CallBase>(&i)) {
        // State escapes only into calls that receive it; guest-memory intrinsics never see it.
        bool gets_state = false;
        for (auto &arg : cb->args())
          if (arg.get() == state) gets_state = true;
        if (!gets_state && cb->getCalledFunction()) return live;
        auto *callee = cb->getCalledFunction();
        auto name = callee ? callee->getName() : llvm::StringRef();
        if (name == "__remill_function_return") return 0;                     // after ret: nothing
        if (name == "__remill_function_call" || name.starts_with("sub_"))
          return (live & ~flag_bits) | (all & ~flag_bits);                   // args may be read
        return all;                                                          // jump, hypercall...
      } else if (llvm::isa<llvm::ReturnInst>(&i)) {
        return all;
      }
      return live;
    };
    std::map<llvm::BasicBlock *, uint64_t> live_in;
    for (auto &bb : f) live_in[&bb] = 0;
    for (bool changed = true; changed;) {
      changed = false;
      std::vector<llvm::BasicBlock *> order;
      for (auto &b : f) order.push_back(&b);
      for (auto it = order.rbegin(); it != order.rend(); ++it) {
        auto &bb = **it;
        uint64_t live = 0;
        for (auto *succ : llvm::successors(&bb)) live |= live_in[succ];
        for (auto ii = bb.rbegin(); ii != bb.rend(); ++ii) live = step(*ii, live, nullptr);
        if (live != live_in[&bb]) { live_in[&bb] = live; changed = true; }
      }
    }
    std::vector<llvm::Instruction *> erase;
    for (auto &bb : f) {
      uint64_t live = 0;
      for (auto *succ : llvm::successors(&bb)) live |= live_in[succ];
      for (auto ii = bb.rbegin(); ii != bb.rend(); ++ii) {
        bool dead = false;
        live = step(*ii, live, &dead);
        if (dead) erase.push_back(&*ii);
      }
    }
    for (auto *e : erase) e->eraseFromParent();
    removed += erase.size();
  }
  return removed;
}

// x87: x86_fp80 does not compile for aarch64 (llc: unsupported libcall). Lower every x86_fp80
// value to fp128 (same 15-bit exponent and bias; 112- vs 63-bit fraction). The 80-bit memory
// format (Remill keeps ST registers as 10 raw bytes) is crossed with runtime helpers:
//   fp128 __ps4_f80_to_f128(i64 mantissa, i16 sign_exp)          — exact
//   i64 __ps4_f128_to_f80_lo(fp128), i16 __ps4_f128_to_f80_hi(fp128) — round fraction to 63 bits
// so a value is rounded to x87 extended precision whenever it lands in a register or memory.
// Returns the number of functions it changed; anything it cannot lower is left for the stubber.
size_t LowerX87ToF128(llvm::Module &m) {
  auto &ctx = m.getContext();
  auto *f80 = llvm::Type::getX86_FP80Ty(ctx);
  auto *f128 = llvm::Type::getFP128Ty(ctx);
  auto *i80 = llvm::Type::getIntNTy(ctx, 80);
  auto *i64 = llvm::Type::getInt64Ty(ctx);
  auto *i16 = llvm::Type::getInt16Ty(ctx);
  auto to128 = m.getOrInsertFunction("__ps4_f80_to_f128", llvm::FunctionType::get(f128, {i64, i16}, false));
  auto lo = m.getOrInsertFunction("__ps4_f128_to_f80_lo", llvm::FunctionType::get(i64, {f128}, false));
  auto hi = m.getOrInsertFunction("__ps4_f128_to_f80_hi", llvm::FunctionType::get(i16, {f128}, false));

  auto bits_to_f128 = [&](llvm::IRBuilder<> &ir, llvm::Value *v80) -> llvm::Value * {
    auto *mant = ir.CreateTrunc(v80, i64);
    auto *se = ir.CreateTrunc(ir.CreateLShr(v80, 64), i16);
    return ir.CreateCall(to128, {mant, se});
  };
  auto f128_to_bits = [&](llvm::IRBuilder<> &ir, llvm::Value *q) -> llvm::Value * {
    auto *l = ir.CreateZExt(ir.CreateCall(lo, {q}), i80);
    auto *h = ir.CreateZExt(ir.CreateCall(hi, {q}), i80);
    return ir.CreateOr(l, ir.CreateShl(h, 64));
  };
  auto touches = [f80](llvm::Instruction &inst) {
    if (inst.getType() == f80) return true;
    for (auto &op : inst.operands())
      if (op->getType() == f80) return true;
    return false;
  };

  size_t changed = 0;
  for (auto &fn : m) {
    if (fn.isDeclaration()) continue;
    std::vector<llvm::Instruction *> work;
    for (auto &bb : fn)
      for (auto &inst : bb)
        if (touches(inst)) work.push_back(&inst);
    if (work.empty()) continue;

    llvm::DenseMap<llvm::Value *, llvm::Value *> map;
    std::vector<std::pair<llvm::PHINode *, llvm::PHINode *>> phis;
    bool ok = true;
    // Placeholders for f80 phis first (loops).
    for (auto *inst : work)
      if (auto *phi = llvm::dyn_cast<llvm::PHINode>(inst); phi && phi->getType() == f80) {
        auto *np = llvm::PHINode::Create(f128, phi->getNumIncomingValues(), "", phi->getIterator());
        map[phi] = np;
        phis.emplace_back(phi, np);
      }
    auto get = [&](llvm::Value *v) -> llvm::Value * {
      if (v->getType() != f80) return v;
      if (auto it = map.find(v); it != map.end()) return it->second;
      if (auto *c = llvm::dyn_cast<llvm::ConstantFP>(v)) {
        llvm::APFloat a = c->getValueAPF();
        bool lost;
        a.convert(llvm::APFloat::IEEEquad(), llvm::APFloat::rmNearestTiesToEven, &lost);
        return llvm::ConstantFP::get(ctx, a);
      }
      if (llvm::isa<llvm::PoisonValue>(v)) return llvm::PoisonValue::get(f128);
      if (llvm::isa<llvm::UndefValue>(v)) return llvm::UndefValue::get(f128);
      return nullptr;
    };
    std::vector<llvm::Instruction *> dead;
    for (auto *inst : work) {
      if (llvm::isa<llvm::PHINode>(inst) && inst->getType() == f80) continue;
      llvm::IRBuilder<> ir(inst);
      llvm::Value *nv = nullptr;
      if (auto *bc = llvm::dyn_cast<llvm::BitCastInst>(inst)) {
        if (bc->getType() == f80 && bc->getOperand(0)->getType() == i80) nv = bits_to_f128(ir, bc->getOperand(0));
        else if (bc->getType() == i80 && bc->getOperand(0)->getType() == f80) {
          if (auto *q = get(bc->getOperand(0))) nv = f128_to_bits(ir, q);
        }
      } else if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(inst)) {
        auto *raw = ir.CreateAlignedLoad(i80, ld->getPointerOperand(), llvm::Align(1));
        nv = bits_to_f128(ir, raw);
      } else if (auto *st = llvm::dyn_cast<llvm::StoreInst>(inst)) {
        if (auto *q = get(st->getValueOperand())) {
          ir.CreateAlignedStore(f128_to_bits(ir, q), st->getPointerOperand(), llvm::Align(1));
          dead.push_back(inst);
          continue;
        }
      } else if (auto *bo = llvm::dyn_cast<llvm::BinaryOperator>(inst)) {
        auto *a = get(bo->getOperand(0)), *b = get(bo->getOperand(1));
        if (a && b) nv = ir.CreateBinOp(bo->getOpcode(), a, b);
      } else if (auto *un = llvm::dyn_cast<llvm::UnaryOperator>(inst)) {
        if (auto *a = get(un->getOperand(0))) nv = ir.CreateUnOp(un->getOpcode(), a);
      } else if (auto *cmp = llvm::dyn_cast<llvm::FCmpInst>(inst)) {
        auto *a = get(cmp->getOperand(0)), *b = get(cmp->getOperand(1));
        if (a && b) nv = ir.CreateFCmp(cmp->getPredicate(), a, b);
      } else if (auto *cast = llvm::dyn_cast<llvm::CastInst>(inst)) {
        auto op = cast->getOpcode();
        auto *src = get(cast->getOperand(0));
        auto *dty = cast->getType() == f80 ? f128 : cast->getType();
        if (!src) {
        } else if ((op == llvm::Instruction::FPExt || op == llvm::Instruction::FPTrunc)) {
          // f32/f64 <-> f80 becomes f32/f64 <-> f128 (direction is decided by the widths).
          auto sw = src->getType()->getPrimitiveSizeInBits(), dw = dty->getPrimitiveSizeInBits();
          nv = sw == dw ? src : sw < dw ? ir.CreateFPExt(src, dty) : ir.CreateFPTrunc(src, dty);
        } else {
          nv = ir.CreateCast(op, src, dty);
        }
      } else if (auto *sel = llvm::dyn_cast<llvm::SelectInst>(inst)) {
        auto *a = get(sel->getTrueValue()), *b = get(sel->getFalseValue());
        if (a && b) nv = ir.CreateSelect(sel->getCondition(), a, b);
      } else if (auto *call = llvm::dyn_cast<llvm::CallInst>(inst)) {
        auto *callee = call->getCalledFunction();
        std::vector<llvm::Value *> args;
        std::vector<llvm::Type *> tys;
        bool args_ok = callee != nullptr;
        for (auto &arg : call->args()) {
          auto *a = get(arg);
          if (!a) { args_ok = false; break; }
          args.push_back(a);
          tys.push_back(a->getType());
        }
        if (args_ok) {
          auto *rty = call->getType() == f80 ? f128 : call->getType();
          if (auto iid = callee->getIntrinsicID()) {
            // Overloaded FP intrinsics (fabs, trunc, floor, sqrt, is.fpclass...): re-mangle on fp128.
            llvm::SmallVector<llvm::Type *, 2> over;
            if (llvm::Intrinsic::isOverloaded(iid)) over.push_back(f128);
            auto *decl = llvm::Intrinsic::getOrInsertDeclaration(&m, iid, over);
            nv = ir.CreateCall(decl, args);
          } else {
            // libm long double functions (sqrtl, sinl...): on aarch64 long double is fp128.
            auto name = callee->getName().str();
            auto *fty = llvm::FunctionType::get(rty, tys, false);
            auto newf = m.getOrInsertFunction(name + ".__f128", fty);
            nv = ir.CreateCall(newf, args);
            pending_renames().emplace_back(name);
          }
        }
      }
      if (!nv) { ok = false; break; }
      if (inst->getType() == f80) map[inst] = nv;
      else inst->replaceAllUsesWith(nv);
      dead.push_back(inst);
    }
    if (ok) {
      for (auto &[old_phi, new_phi] : phis)
        for (unsigned i = 0; i < old_phi->getNumIncomingValues(); ++i) {
          auto *v = get(old_phi->getIncomingValue(i));
          if (!v) { ok = false; break; }
          new_phi->addIncoming(v, old_phi->getIncomingBlock(i));
        }
    }
    if (!ok) continue;  // the stubber takes it; leftover new instructions are dead and harmless
    for (auto &[old_phi, new_phi] : phis) dead.push_back(old_phi);
    for (auto *d : dead) d->dropAllReferences();
    for (auto *d : dead) d->eraseFromParent();
    ++changed;
  }
  // `sqrtl.__f128` → `sqrtl` with the fp128 signature (old x86_fp80 declarations go away).
  for (auto &name : pending_renames()) {
    auto *old = m.getFunction(name);
    auto *nf = m.getFunction(name + ".__f128");
    if (!nf) continue;
    if (old && old->use_empty()) old->eraseFromParent();
    if (!m.getFunction(name)) nf->setName(name);
  }
  pending_renames().clear();
  return changed;
}

// x87 (x86_fp80) does not compile for aarch64 yet (llc: unsupported libcall). Until the f80 →
// fp128 semantics land, a trace that touches x86_fp80 is replaced by a stub that escapes with
// __remill_error at its entry pc. Returns the number of stubbed traces.
size_t StubX87Traces(llvm::Module &m) {
  auto *f80 = llvm::Type::getX86_FP80Ty(m.getContext());
  auto uses_f80 = [f80](llvm::Function &f) {
    for (auto &bb : f)
      for (auto &inst : bb) {
        if (inst.getType() == f80) return true;
        for (auto &op : inst.operands())
          if (op->getType() == f80) return true;
      }
    return false;
  };
  auto err = m.getFunction("__remill_error");
  size_t n = 0;
  for (auto &f : m) {
    if (f.isDeclaration() || !uses_f80(f)) continue;
    auto *fty = f.getFunctionType();
    if (!err) {
      err = llvm::Function::Create(fty, llvm::GlobalValue::ExternalLinkage, "__remill_error", &m);
    }
    f.deleteBody();
    auto *bb = llvm::BasicBlock::Create(m.getContext(), "", &f);
    llvm::IRBuilder<> ir(bb);
    auto *call = ir.CreateCall(err, {f.getArg(0), f.getArg(1), f.getArg(2)});
    ir.CreateRet(call);
    ++n;
  }
  return n;
}

// Retarget the x86-semantics module to aarch64 (see scripts/pilot/build.sh for the same steps).
void Retarget(llvm::Module &m) {
  m.setTargetTriple(llvm::Triple(FLAGS_target_triple));
  m.setDataLayout(FLAGS_target_datalayout);
  for (auto &f : m) {
    f.removeFnAttr("target-cpu");
    f.removeFnAttr("target-features");
    f.removeFnAttr("tune-cpu");
    if (f.isDeclaration()) {
      f.removeFnAttr(llvm::Attribute::NoInline);
      f.removeFnAttr(llvm::Attribute::OptimizeNone);
      f.removeFnAttr(llvm::Attribute::NoDuplicate);
    }
    for (auto &arg : f.args()) arg.removeAttr(llvm::Attribute::Dereferenceable);
    for (auto &bb : f)
      for (auto &inst : bb)
        if (auto *cb = llvm::dyn_cast<llvm::CallBase>(&inst)) {
          cb->removeFnAttr(llvm::Attribute::NoInline);
          cb->removeFnAttr(llvm::Attribute::NoDuplicate);
          for (unsigned i = 0; i < cb->arg_size(); ++i)
            cb->removeParamAttr(i, llvm::Attribute::Dereferenceable);
        }
  }
}

}  // namespace

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  auto t0 = std::chrono::steady_clock::now();

  std::ifstream sf(FLAGS_seg, std::ios::binary);
  std::vector<uint8_t> seg((std::istreambuf_iterator<char>(sf)), {});
  CHECK(!seg.empty() || !FLAGS_state_module.empty()) << "cannot read " << FLAGS_seg;
  const uint64_t seg_lo = FLAGS_load_base + FLAGS_seg_vaddr;

  std::vector<std::pair<uint64_t, uint64_t>> ranges;
  {
    std::ifstream rf(FLAGS_ranges);
    std::string a, b;
    while (rf >> a >> b) ranges.emplace_back(std::stoull(a, nullptr, 16), std::stoull(b, nullptr, 16));
  }
  std::set<uint64_t> all, chunk;
  for (auto &r : ranges) all.insert(FLAGS_load_base + r.first);
  for (uint64_t i = FLAGS_first; i < std::min<uint64_t>(ranges.size(), FLAGS_first + FLAGS_count); ++i)
    chunk.insert(FLAGS_load_base + ranges[i].first);
  if (chunk.empty() && FLAGS_state_module.empty()) {
    std::cerr << "empty chunk\n";
    return 1;
  }

  llvm::LLVMContext context;
  auto arch = remill::Arch::Get(context, "linux", FLAGS_arch);
  std::unique_ptr<llvm::Module> module(remill::LoadArchSemantics(arch.get()));
  remill::IntrinsicTable intrinsics(module.get());

  if (!FLAGS_state_module.empty()) {
    // Host-side access to the lifted State: accessors built from Remill's own register GEPs,
    // so offsets follow the aarch64 data layout of the lifted code by construction.
    llvm::Module sm("ps4_state", context);
    arch->PrepareModuleDataLayout(&sm);
    auto *ptr_ty = llvm::PointerType::get(context, 0);
    const char *regs[] = {"RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RBP", "RSP", "R8", "R9",
                          "R10", "R11", "R12", "R13", "R14", "R15", "RIP", "FSBASE", "GSBASE",
                          "CF", "PF", "AF", "ZF", "SF", "DF", "OF", "XMM0", "XMM1", "XMM2",
                          "XMM3", "XMM4", "XMM5", "XMM6", "XMM7"};
    for (auto name : regs) {
      auto *reg = arch->RegisterByName(name);
      CHECK(reg) << name;
      auto *fn = llvm::Function::Create(llvm::FunctionType::get(ptr_ty, {ptr_ty}, false),
                                        llvm::GlobalValue::ExternalLinkage,
                                        std::string("ps4_reg_") + name, &sm);
      auto *bb = llvm::BasicBlock::Create(context, "", fn);
      auto *addr = reg->AddressOf(fn->getArg(0), bb);
      llvm::IRBuilder<>(bb).CreateRet(addr);
    }
    Retarget(sm);
    llvm::DataLayout dl(FLAGS_target_datalayout);
    auto size = dl.getTypeAllocSize(arch->StateStructType()).getFixedValue();
    auto *i64 = llvm::Type::getInt64Ty(context);
    new llvm::GlobalVariable(sm, i64, true, llvm::GlobalValue::ExternalLinkage,
                             llvm::ConstantInt::get(i64, size), "ps4_state_size");
    std::error_code sec;
    llvm::raw_fd_ostream sos(FLAGS_state_module, sec);
    CHECK(!sec) << sec.message();
    llvm::WriteBitcodeToFile(sm, sos);
    printf("state module: %zu registers, sizeof(State) = %llu (aarch64 layout)\n",
           sizeof(regs) / sizeof(regs[0]), (unsigned long long)size);
    return 0;
  }
  ChunkTraceManager manager(arch.get(), module.get(), seg, seg_lo, all, chunk);
  remill::TraceLifter lifter(arch.get(), manager);
  size_t failed = 0;
  size_t switch_targets = 0;
  if (FLAGS_switch_tables && !FLAGS_image.empty()) {
    std::ifstream imf(FLAGS_image, std::ios::binary);
    g_image.assign((std::istreambuf_iterator<char>(imf)), {});
    for (uint64_t i = FLAGS_first; i < std::min<uint64_t>(ranges.size(), FLAGS_first + FLAGS_count); ++i)
      switch_targets += RecoverSwitchTargets(seg, seg_lo, FLAGS_load_base + ranges[i].first,
                                             ranges[i].second, manager.jump_tables);
  }
  for (auto addr : chunk)
    if (!lifter.Lift(addr)) ++failed;
  auto t_lift = std::chrono::steady_clock::now();

  // Same clean-up as remill-lift (bin/lift/Lift.cpp).
  if (auto g = module->getGlobalVariable("llvm.compiler.used", true)) g->eraseFromParent();
  std::vector<llvm::GlobalVariable *> isels;
  for (auto &g : module->globals())
    if (g.getName().starts_with("ISEL_")) isels.push_back(&g);
  for (auto *g : isels) g->eraseFromParent();
  if (auto f = module->getFunction("__remill_intrinsics")) f->eraseFromParent();
  // The x86 semantics carry a reference __remill_sync_hyper_call (inline x86 asm: cpuid, rdtsc,
  // lgdt...). Replace it with a declaration — our runtime implements hypercalls.
  if (auto hc = module->getFunction("__remill_sync_hyper_call")) {
    auto name = hc->getName().str();
    auto decl = module->getOrInsertFunction(name + "_", hc->getFunctionType());
    hc->replaceAllUsesWith(decl.getCallee());
    hc->eraseFromParent();
    decl.getCallee()->setName(name);
  }
  for (auto &f : module->functions()) {
    if (!f.getName().starts_with("__remill_")) continue;
    f.removeFnAttr(llvm::Attribute::ReadNone);
    for (auto &arg : f.args()) arg.removeAttr(llvm::Attribute::ReadNone);
  }

  remill::OptimizationGuide guide = {};
  remill::OptimizeModule(arch.get(), module.get(), manager.traces, guide);
  auto t_opt = std::chrono::steady_clock::now();

  llvm::Module dest("ps4lift_chunk", context);
  arch->PrepareModuleDataLayout(&dest);
  std::vector<std::pair<uint64_t, std::string>> names;
  for (auto &[addr, f] : manager.traces) names.emplace_back(addr, f->getName().str());
  for (auto &[addr, f] : manager.traces) remill::MoveFunctionIntoModule(f, &dest);
  for (auto &[addr, name] : names)
    if (auto *moved = dest.getFunction(name); moved && !chunk.count(addr) && !all.count(addr))
      moved->setLinkage(llvm::GlobalValue::InternalLinkage);
  size_t x87_lowered = FLAGS_lower_x87 ? LowerX87ToF128(dest) : 0;
  size_t x87_stubs = FLAGS_stub_x87 ? StubX87Traces(dest) : 0;
  Retarget(dest);

  // Per-chunk trace table {guest address, lifted function}, registered at startup with the
  // runtime dispatcher (indirect calls/jumps, switch targets).
  {
    auto *i64 = llvm::Type::getInt64Ty(context);
    auto *ptr_ty = llvm::PointerType::get(context, 0);
    auto *void_ty = llvm::Type::getVoidTy(context);
    auto *ety = llvm::StructType::get(i64, ptr_ty);
    std::vector<llvm::Constant *> elems;
    for (auto &[addr, name] : names)
      if (auto *fn = dest.getFunction(name); fn && !fn->isDeclaration())
        elems.push_back(llvm::ConstantStruct::get(ety, {llvm::ConstantInt::get(i64, addr), fn}));
    auto *aty = llvm::ArrayType::get(ety, elems.size());
    std::string base = FLAGS_table_prefix + "_" + std::to_string(FLAGS_first);
    auto *table = new llvm::GlobalVariable(dest, aty, true, llvm::GlobalValue::InternalLinkage,
                                           llvm::ConstantArray::get(aty, elems), base + "_traces");
    auto reg = dest.getOrInsertFunction("ps4rt_register_traces",
                                        llvm::FunctionType::get(void_ty, {ptr_ty, i64}, false));
    auto *ctor = llvm::Function::Create(llvm::FunctionType::get(void_ty, false),
                                        llvm::GlobalValue::InternalLinkage, base + "_ctor", &dest);
    llvm::IRBuilder<> ir(llvm::BasicBlock::Create(context, "", ctor));
    ir.CreateCall(reg, {table, llvm::ConstantInt::get(i64, elems.size())});
    ir.CreateRetVoid();
    llvm::appendToGlobalCtors(dest, ctor, 65535);
  }

  // Optimize here (tables already reference every trace, so nothing is dropped), then remove
  // ABI-dead flag/scratch stores and clean up what fed them.
  size_t abi_removed = 0, stack_plain = 0;
  {
    RunPipeline(dest, true);
    if (FLAGS_abi_dead_state) {
      abi_removed = RemoveAbiDeadStores(dest, arch.get());
      if (FLAGS_plain_stack) stack_plain = PlainStackAccesses(dest, arch.get());
      RunPipeline(dest, false);
    }
  }

  size_t jumps = CountCalls(dest, "__remill_jump"), icalls = CountCalls(dest, "__remill_function_call"),
         errors = CountCalls(dest, "__remill_error"), missing = CountCalls(dest, "__remill_missing_block"),
         hyper = CountCalls(dest, "__remill_sync_hyper_call"),
         async_hyper = CountCalls(dest, "__remill_async_hyper_call");

  std::error_code ec;
  llvm::raw_fd_ostream os(FLAGS_out, ec);
  CHECK(!ec) << ec.message();
  llvm::WriteBitcodeToFile(dest, os);
  auto t_end = std::chrono::steady_clock::now();
  auto ms = [](auto a, auto b) { return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count(); };
  printf("chunk first=%llu funcs=%zu traces=%zu switch_targets=%zu abi_dead_stores=%zu stack_plain=%zu x87_lowered=%zu x87_stubs=%zu failed=%zu jump=%zu icall=%zu "
         "error=%zu missing=%zu hyper=%zu async=%zu lift_ms=%lld opt_ms=%lld total_ms=%lld\n",
         (unsigned long long)FLAGS_first, chunk.size(), manager.traces.size(), switch_targets, abi_removed, stack_plain, x87_lowered, x87_stubs, failed,
         jumps, icalls,
         errors, missing, hyper, async_hyper, (long long)ms(t0, t_lift), (long long)ms(t_lift, t_opt),
         (long long)ms(t0, t_end));
  return 0;
}
