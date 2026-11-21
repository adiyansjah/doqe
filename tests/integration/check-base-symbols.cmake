execute_process(COMMAND "${LLVM_NM}" --defined-only --demangle "${ARCHIVE}"
  OUTPUT_VARIABLE symbols COMMAND_ERROR_IS_FATAL ANY)
if(symbols MATCHES "doqe::base::testing::|StatusProbeSlot|FlatPool::InjectLiveBytes")
  message(FATAL_ERROR "Production base archive contains test hooks")
endif()
