# fxc /Fh writes `const BYTE <name>[]` (BYTE is a Windows typedef); make the header self-contained.
#   cmake -DHEADER=<file> -P PatchShaderHeader.cmake
file(READ "${HEADER}" CONTENT)
string(REPLACE "const BYTE " "static const unsigned char " CONTENT "${CONTENT}")
file(WRITE "${HEADER}" "${CONTENT}")
