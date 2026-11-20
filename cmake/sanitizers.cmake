option(DOQE_ENABLE_SANITIZERS "Instrument the build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(DOQE_ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all
    -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
