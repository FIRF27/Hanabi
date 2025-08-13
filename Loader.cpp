// For open-source license, please refer to
// [License](https://github.com/HikariObfuscator/Hikari/wiki/License).
#include "dobby.h"
#include <dlfcn.h>
#include <llvm/Config/abi-breaking.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Obfuscation/BogusControlFlow.h>
#include <llvm/Transforms/Obfuscation/CryptoUtils.h>
#include <llvm/Transforms/Obfuscation/Flattening.h>
#include <llvm/Transforms/Obfuscation/IPObfuscationContext.h>
#include <llvm/Transforms/Obfuscation/IndirectBranch.h>
#include <llvm/Transforms/Obfuscation/IndirectCall.h>
#include <llvm/Transforms/Obfuscation/IndirectGlobalVariable.h>
#include <llvm/Transforms/Obfuscation/ObfuscationOptions.h>
#include <llvm/Transforms/Obfuscation/SplitBasicBlock.h>
#include <llvm/Transforms/Obfuscation/StringEncryption.h>
#include <llvm/Transforms/Obfuscation/Substitution.h>
#include <llvm/Transforms/Obfuscation/Utils.h>

#include <mach-o/dyld.h>
#include <string>

#if LLVM_ENABLE_ABI_BREAKING_CHECKS == 1
#error "Configure LLVM with -DLLVM_ABI_BREAKING_CHECKS=FORCE_OFF"
#endif

using namespace llvm;

typedef void (*dobby_dummy_func_t)(void);

/**
 /// Register a callback for a default optimizer pipeline extension point
 ///
 /// This extension point allows adding optimizations at the very end of the
 /// function optimization pipeline.
 void registerOptimizerLastEPCallback(
     const std::function<void(ModulePassManager &, OptimizationLevel)> &C) {
   OptimizerLastEPCallbacks.push_back(C);
 }
  */

// xcode16.2 llvmorg-17.0.5
// PassBuilder::buildPerModuleDefaultPipeline
// 替换目标函数 registerOptimizerLastEPCallback
ModulePassManager (*targetFunction)(void *Level, bool LTOPreLink) = nullptr;

static ModulePassManager hookFunction(void *Level, bool LTOPreLink)
{
  ModulePassManager MPM = targetFunction(Level, LTOPreLink);
  MPM.addPass(createModuleToFunctionPassAdaptor(BogusControlFlowPass()));
  return MPM;
}

static __attribute__((__constructor__)) void Inj3c73d(int argc, char *argv[])
{
  char *executablePath = argv[0];
  // Initialize our own LLVM Library
  if (strstr(executablePath, "swift-frontend"))
    errs() << "Applying Apple SwiftC Hooks...\n";
  else
    errs() << "Applying Apple Clang Hooks...\n";

  // 用于保存 DobbyHook 返回的原函数地址（void*）
  void *targetFunctionPtr = nullptr;
  // 使用 DobbyHook 进行 Hook，注意参数类型都用 void* 强转
  void *symbolResolver = DobbySymbolResolver(
      executablePath,
      "__ZN4llvm11PassBuilder29buildPerModuleDefaultPipelineENS_"
      "17OptimizationLevelEb");

  int result = DobbyHook(symbolResolver, (void *)hookFunction, &targetFunctionPtr);

  if (result != 0)
  {
    errs() << "DobbyHook failed!\n";
    return;
  }

  // 将 void* 转换成正确的函数指针类型
  targetFunction = reinterpret_cast<decltype(targetFunction)>(targetFunctionPtr);
  errs() << "Hook buildPerModuleDefaultPipeline success\n";
}
