set(LLVM_LINK_COMPONENTS core engine)

add_llvm_loadable_module(LLVMsvc15
	Prepare.cpp
)
