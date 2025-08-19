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

#include <iostream>
#include <mach-o/dyld.h>
#include <string>
#if LLVM_ENABLE_ABI_BREAKING_CHECKS == 1
#error "Configure LLVM with -DLLVM_ABI_BREAKING_CHECKS=FORCE_OFF"
#endif

using namespace llvm;

#pragma mark - SWIFT-FRONTEND
ModulePassManager (*orig_perModuleDefaultPipeline)(OptimizationLevel Level,
                                                   bool LTOPreLink) = nullptr;
static ModulePassManager
hook_perModuleDefaultPipeline(OptimizationLevel Level,
                              bool LTOPreLink = false) {
  std::cout << "Hook [perModuleDefaultPipeline]\n" << std::endl;
  ModulePassManager MPM = orig_perModuleDefaultPipeline(Level, LTOPreLink);
  // 先进行字符串加密
  // 出现字符串加密基本块以后再进行基本块分割和其他混淆 加大解密难度
  MPM.addPass(StringEncryptionPass(true));
  // MPM.addPass(
  //     createModuleToFunctionPassAdaptor(IndirectCallPass())); // 间接调用
  // MPM.addPass(createModuleToFunctionPassAdaptor(
  //     SplitBasicBlockPass())); // 优先进行基本块分割
  MPM.addPass(createModuleToFunctionPassAdaptor(FlatteningPass())); //
  //     对于控制流平坦化
  // MPM.addPass(
  //     createModuleToFunctionPassAdaptor(SubstitutionPass())); // 指令替换
  MPM.addPass(
      createModuleToFunctionPassAdaptor(BogusControlFlowPass())); // 虚假控制流
  // MPM.addPass(IndirectBranchPass(true)); // 间接指令
  // 理论上间接指令应该放在最后
  //  MPM.addPass(IndirectGlobalVariablePass(true)); // 间接全局变量
  // MPM.addPass(RewriteSymbolPass()); //
  // 根据yaml信息 重命名特定symbols

  return MPM;
}

#pragma mark - HOOK FUNCTION

void hookFunction(char *image_name) {

  void *orig_perModuleDefaultPipeline_ptr = nullptr;
  void *symbol1 = DobbySymbolResolver(
      image_name, "__ZN4llvm11PassBuilder29buildPerModuleDefaultPipelineENS_"
                  "17OptimizationLevelEb");

  int result = DobbyHook(symbol1, (void *)hook_perModuleDefaultPipeline,
                         &orig_perModuleDefaultPipeline_ptr);

  if (result == 0) {
    orig_perModuleDefaultPipeline =
        reinterpret_cast<decltype(orig_perModuleDefaultPipeline)>(
            orig_perModuleDefaultPipeline_ptr);
  }
  std::cout << "Hook end! result=" << (result == 0 ? "success" : "failed")
            << std::endl;
}

static __attribute__((__constructor__)) void Inj3c73d(int argc, char *argv[]) {
  char *executablePath = argv[0];
  if (strstr(executablePath, "swift-frontend")) {
    std::cout << "Hooking Swift Frontend...\n";
  } else {
    std::cout << "Hooking Clang Frontend...\n";
  }
  hookFunction(executablePath);
}
