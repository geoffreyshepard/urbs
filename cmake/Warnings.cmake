function(urbs_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /wd4100 /wd4201)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                                             -Wno-sign-conversion -Wno-unused-parameter
                                             -Wno-gnu-anonymous-struct -Wno-nested-anon-types)
  endif()
endfunction()
