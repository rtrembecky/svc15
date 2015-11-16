//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include <assert.h>
#include <cstring>
#include <vector>

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

class CheckUnsupported : public FunctionPass
{
  public:
    static char ID;

    CheckUnsupported() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F);
};

static RegisterPass<CheckUnsupported> CHCK("check-unsupported",
                                           "check calls to unsupported functions for symbiotic");
char CheckUnsupported::ID;

static bool array_match(StringRef &name, const char **array)
{
  for (const char **curr = array; *curr; curr++)
    if (name.equals(*curr))
      return true;
  return false;
}

bool CheckUnsupported::runOnFunction(Function &F) {
  static const char *unsupported_calls[] = {
    "pthread_create",
    NULL
  };

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E;) {
    Instruction *ins = &*I;
    ++I;
    if (CallInst *CI = dyn_cast<CallInst>(ins)) {
      if (CI->isInlineAsm())
        continue;

      const Value *val = CI->getCalledValue()->stripPointerCasts();
      const Function *callee = dyn_cast<Function>(val);
      if (!callee || callee->isIntrinsic())
	continue;

      assert(callee->hasName());
      StringRef name = callee->getName();

      if (array_match(name, unsupported_calls)) {
	errs() << "CheckUnsupported: call to '" << name << "' is unsupported\n";
        errs().flush();
      }
    }
  }

  return false;
}

namespace {
  class DeleteUndefined : public FunctionPass {
    public:
      static char ID;

      DeleteUndefined() : FunctionPass(ID) {}

      virtual bool runOnFunction(Function &F);
  };
}

static RegisterPass<DeleteUndefined> DLTU("delete-undefined",
                                          "delete calls to undefined functions");
char DeleteUndefined::ID;

static const char *leave_calls[] = {
  "__assert_fail",
  "abort",
  "klee_make_symbolic",
  "klee_assume",
  "exit",
  "_exit",
  "sprintf",
  "snprintf",
  "swprintf",
  "malloc",
  "calloc",
  "realloc",
  "free",
  "memset",
  "memcmp",
  "memcpy",
  "memmove",
  "kzalloc",
  "__errno_location",
  NULL
};

// FIXME: don't duplicate the code with -instrument-alloca
// replace CallInst with alloca with nondeterministic value
// TODO: what about pointers it takes as parameters?
static void replaceCall(CallInst *CI, Module *M)
{
  LLVMContext& Ctx = M->getContext();
  DataLayout *DL = new DataLayout(M->getDataLayout());
  Constant *name_init = ConstantDataArray::getString(Ctx, "nondet_from_undef");
  GlobalVariable *name = new GlobalVariable(*M, name_init->getType(), true, GlobalValue::PrivateLinkage, name_init);
  Type *size_t_Ty;

  if (DL->getPointerSizeInBits() > 32)
    size_t_Ty = Type::getInt64Ty(Ctx);
  else
    size_t_Ty = Type::getInt32Ty(Ctx);

  //void klee_make_symbolic(void *addr, size_t nbytes, const char *name);
  Constant *C = M->getOrInsertFunction("klee_make_symbolic",
                                       Type::getVoidTy(Ctx),
                                       Type::getInt8PtrTy(Ctx), // addr
                                       size_t_Ty,   // nbytes
                                       Type::getInt8PtrTy(Ctx), // name
                                       NULL);


  Type *Ty = CI->getType();
  // we checked for this before
  assert(!Ty->isVoidTy());
  // what to do in this case?
  assert(Ty->isSized());

  AllocaInst *AI = new AllocaInst(Ty, "alloca_from_undef");
  LoadInst *LI = new LoadInst(AI);
  CallInst *newCI = NULL;
  CastInst *CastI = NULL;

  std::vector<Value *> args;
  CastI = CastInst::CreatePointerCast(AI, Type::getInt8PtrTy(Ctx));

  args.push_back(CastI);
  args.push_back(ConstantInt::get(size_t_Ty, DL->getTypeAllocSize(Ty)));
  args.push_back(ConstantExpr::getPointerCast(name, Type::getInt8PtrTy(Ctx)));
  newCI = CallInst::Create(C, args);


  AI->insertAfter(CI);
  CastI->insertAfter(AI);
  newCI->insertAfter(CastI);
  LI->insertAfter(newCI);

  CI->replaceAllUsesWith(LI);
}

bool DeleteUndefined::runOnFunction(Function &F)
{
  bool modified = false;
  Module *M = F.getParent();

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E;) {
    Instruction *ins = &*I;
    ++I;
    if (CallInst *CI = dyn_cast<CallInst>(ins)) {
      if (CI->isInlineAsm())
        continue;

      const Value *val = CI->getCalledValue()->stripPointerCasts();
      const Function *callee = dyn_cast<Function>(val);
      if (!callee || callee->isIntrinsic())
        continue;

      assert(callee->hasName());
      StringRef name = callee->getName();

      if (name.equals("nondet_int") ||
          name.equals("klee_int") || array_match(name, leave_calls)) {
        continue;
      }

      // if this is __VERIFIER_something call different that to nondet,
      // keep it
      if (name.startswith("__VERIFIER") && !name.startswith("__VERIFIER_nondet"))
        continue;

      if (callee->isDeclaration()) {
       errs() << "Prepare: removing call to '" << name << "' (unsound)\n";
       if (!CI->getType()->isVoidTy()) {
         replaceCall(CI, M);
       }
       CI->eraseFromParent();
       modified = true;
      }
    }
  }
  return modified;
}

namespace {
  class Prepare : public ModulePass {
    public:
      static char ID;

      Prepare() : ModulePass(ID) {}

      virtual bool runOnModule(Module &M);

    private:
      void findInitFuns(Module &M);
  };
}

static RegisterPass<Prepare> PRP("prepare",
                                 "Prepare the code for svcomp");
char Prepare::ID;

void Prepare::findInitFuns(Module &M) {
  SmallVector<Constant *, 1> initFns;
  Type *ETy = TypeBuilder<void *, false>::get(M.getContext());
  Function *_main = M.getFunction("main");
  assert(_main);

  initFns.push_back(ConstantExpr::getBitCast(_main, ETy));
  ArrayType *ATy = ArrayType::get(ETy, initFns.size());
  new GlobalVariable(M, ATy, true, GlobalVariable::InternalLinkage,
                     ConstantArray::get(ATy, initFns),
                     "__ai_init_functions");
}

bool Prepare::runOnModule(Module &M) {
  static const char *del_body[] = {
    "kzalloc",
    "nondet_int",
    "__VERIFIER_assume",
    "__VERIFIER_nondet_pointer",
    "__VERIFIER_nondet_pchar",
    "__VERIFIER_nondet_char",
    "__VERIFIER_nondet_short",
    "__VERIFIER_nondet_int",
    "__VERIFIER_nondet_long",
    "__VERIFIER_nondet_uchar",
    "__VERIFIER_nondet_ushort",
    "__VERIFIER_nondet_uint",
    "__VERIFIER_nondet_ulong",
    "__VERIFIER_nondet_unsigned",
    "__VERIFIER_nondet_u32",
    "__VERIFIER_nondet_float",
    "__VERIFIER_nondet_double",
    "__VERIFIER_nondet_bool",
    "__VERIFIER_nondet__Bool",
    NULL
  };
  LLVMContext &C = M.getContext();

  for (const char **curr = del_body; *curr; curr++) {
    Function *toDel = M.getFunction(*curr);
    if (toDel && !toDel->empty()) {
      errs() << "deleting " << toDel->getName() << '\n';
      toDel->deleteBody();
    }
  }

  for (Module::global_iterator I = M.global_begin(), E = M.global_end();
      I != E; ++I) {
    GlobalVariable *GV = &*I;
    if (GV->isConstant() || GV->hasInitializer())
      continue;
    GV->setInitializer(Constant::getNullValue(GV->getType()->getElementType()));
    errs() << "making " << GV->getName() << " non-extern\n";
  }

  findInitFuns(M);

  return true;
}

class InstrumentAlloc : public FunctionPass {
  public:
    static char ID;

    InstrumentAlloc() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F);
};

static RegisterPass<InstrumentAlloc> INSTALLOC("instrument-alloc",
                                               "replace calls to malloc and calloc with our funs");
char InstrumentAlloc::ID;

class InstrumentAllocNeverFails : public FunctionPass {
  public:
    static char ID;

    InstrumentAllocNeverFails() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F);
};

static RegisterPass<InstrumentAllocNeverFails> INSTALLOCNF("instrument-alloc-nf",
                                                           "replace calls to malloc and calloc with our funs and assume that the"
                                                           "allocation never fail");
char InstrumentAllocNeverFails::ID;

static void replace_malloc(Module *M, CallInst *CI, bool never_fails)
{
  Constant *C = NULL;

  if (never_fails)
    C = M->getOrInsertFunction("__VERIFIER_malloc0", CI->getType(), CI->getOperand(0)->getType(), NULL);
  else
    C = M->getOrInsertFunction("__VERIFIER_malloc", CI->getType(), CI->getOperand(0)->getType(), NULL);

  assert(C);
  Function *Malloc = cast<Function>(C);

  CI->setCalledFunction(Malloc);
}

static void replace_calloc(Module *M, CallInst *CI, bool never_fails)
{
  Constant *C = NULL;
  if (never_fails)
    C = M->getOrInsertFunction("__VERIFIER_calloc0", CI->getType(), CI->getOperand(0)->getType(), CI->getOperand(1)->getType(), NULL);
  else
    C = M->getOrInsertFunction("__VERIFIER_calloc", CI->getType(), CI->getOperand(0)->getType(), CI->getOperand(1)->getType(), NULL);

  assert(C);
  Function *Calloc = cast<Function>(C);
  CI->setCalledFunction(Calloc);
}

static bool instrument_alloc(Function &F, bool never_fails)
{
  bool modified = false;
  Module *M = F.getParent();

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E;) {
    Instruction *ins = &*I;
    ++I;
    if (CallInst *CI = dyn_cast<CallInst>(ins)) {
      if (CI->isInlineAsm())
        continue;

      const Value *val = CI->getCalledValue()->stripPointerCasts();
      const Function *callee = dyn_cast<Function>(val);
      if (!callee || callee->isIntrinsic())
        continue;

      assert(callee->hasName());
      StringRef name = callee->getName();

      if (name.equals("malloc")) {
        replace_malloc(M, CI, never_fails);
        modified = true;
      } else if (name.equals("calloc")) {
        replace_calloc(M, CI, never_fails);
        modified = true;
      }
    }
  }
  return modified;
}

bool InstrumentAlloc::runOnFunction(Function &F)
{
    return instrument_alloc(F, false /* never fails */);
}

bool InstrumentAllocNeverFails::runOnFunction(Function &F)
{
    return instrument_alloc(F, true /* never fails */);
}

class InitializeUninitialized : public FunctionPass {
  public:
    static char ID;

    InitializeUninitialized() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F);
};


static RegisterPass<InitializeUninitialized> INIUNINI("initialize-uninitialized",
                                                      "initialize all uninitialized variables to non-deterministic value");
char InitializeUninitialized::ID;

bool InitializeUninitialized::runOnFunction(Function &F)
{
  bool modified = false;
  Module *M = F.getParent();
  LLVMContext& Ctx = M->getContext();
  DataLayout *DL = new DataLayout(M->getDataLayout());
  Constant *name_init = ConstantDataArray::getString(Ctx, "nondet");
  GlobalVariable *name = new GlobalVariable(*M, name_init->getType(), true, GlobalValue::PrivateLinkage, name_init);
  Type *size_t_Ty;

  if (DL->getPointerSizeInBits() > 32)
    size_t_Ty = Type::getInt64Ty(Ctx);
  else
    size_t_Ty = Type::getInt32Ty(Ctx);

  //void klee_make_symbolic(void *addr, size_t nbytes, const char *name);
  Constant *C = M->getOrInsertFunction("klee_make_symbolic",
                                       Type::getVoidTy(Ctx),
                                       Type::getInt8PtrTy(Ctx), // addr
                                       size_t_Ty,   // nbytes
                                       Type::getInt8PtrTy(Ctx), // name
                                       NULL);


  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E;) {
    Instruction *ins = &*I;
    ++I;

    if (AllocaInst *AI = dyn_cast<AllocaInst>(ins)) {
      Type *Ty = AI->getAllocatedType();
      CallInst *CI = NULL;
      CastInst *CastI = NULL;
      GetElementPtrInst *GEP = NULL;

      if (Ty->isSized()) {
        std::vector<Value *> args;
        CastI = CastInst::CreatePointerCast(AI, Type::getInt8PtrTy(Ctx));

        args.push_back(CastI);
        args.push_back(ConstantInt::get(size_t_Ty, DL->getTypeAllocSize(Ty)));
        args.push_back(ConstantExpr::getPointerCast(name, Type::getInt8PtrTy(Ctx)));
        CI = CallInst::Create(C, args);
      }

      if (CI) {
        assert(CastI);
        if (GEP) {
            GEP->insertAfter(AI);
            CastI->insertAfter(GEP);
        } else
            CastI->insertAfter(AI);

        CI->insertAfter(CastI);
        modified = true;
      }
    }
  }

  delete DL;
  return modified;
}

