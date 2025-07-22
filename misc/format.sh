#!/bin/sh
PATHS=$(find main -maxdepth 1 \! -name thirdparty -type d)
find ${PATHS} -maxdepth 1 -name '*.c' -exec clang-format -i {} \;
find ${PATHS} -maxdepth 1 -name '*.h' -exec clang-format -i {} \;
