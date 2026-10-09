# Usage: cmake -DSPV=... -DMSL=... -DOUT=... -DIDENT=... -P EmbedShader.cmake
function(to_c_array file var out)
  file(READ ${file} _hex HEX)
  string(LENGTH "${_hex}" _len)
  math(EXPR _n "${_len} / 2")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _arr "${_hex}")
  set(${out} "static const unsigned char ${var}[] = {${_arr} 0x00};\nstatic const unsigned int ${var}_size = ${_n};\n" PARENT_SCOPE)
endfunction()

to_c_array(${SPV} ${IDENT}_spv _a)
to_c_array(${MSL} ${IDENT}_msl _b)
file(WRITE ${OUT} "// Generated from shaders/. Do not edit.\n#pragma once\n${_a}${_b}")
