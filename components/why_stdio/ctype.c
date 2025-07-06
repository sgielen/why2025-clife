#include <ctype.h>

#define _U __CTYPE_UPPER
#define _L __CTYPE_LOWER
#define _N __CTYPE_DIGIT
#define _S __CTYPE_SPACE
#define _P __CTYPE_PUNCT
#define _C __CTYPE_CNTRL
#define _X __CTYPE_HEX
#define _B __CTYPE_BLANK
#define _T __CTYPE_TAB

#define _CTYPE_DATA_0_127 \
        _C,     _C,     _C,     _C,     _C,     _C,     _C,     _C, \
        _C,     _C|_S, _C|_S, _C|_S,    _C|_S,  _C|_S,  _C,     _C, \
        _C,     _C,     _C,     _C,     _C,     _C,     _C,     _C, \
        _C,     _C,     _C,     _C,     _C,     _C,     _C,     _C, \
        _S|_B,  _P,     _P,     _P,     _P,     _P,     _P,     _P, \
        _P,     _P,     _P,     _P,     _P,     _P,     _P,     _P, \
        _N,     _N,     _N,     _N,     _N,     _N,     _N,     _N, \
        _N,     _N,     _P,     _P,     _P,     _P,     _P,     _P, \
        _P,     _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U, \
        _U,     _U,     _U,     _U,     _U,     _U,     _U,     _U, \
        _U,     _U,     _U,     _U,     _U,     _U,     _U,     _U, \
        _U,     _U,     _U,     _P,     _P,     _P,     _P,     _P, \
        _P,     _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L, \
        _L,     _L,     _L,     _L,     _L,     _L,     _L,     _L, \
        _L,     _L,     _L,     _L,     _L,     _L,     _L,     _L, \
        _L,     _L,     _L,     _P,     _P,     _P,     _P,     _C

#define _CTYPE_DATA_0_127_WIDE \
        _C,     _C,     _C,     _C,     _C,     _C,     _C,     _C, \
        _C,     _C|_S|_T, _C|_S, _C|_S, _C|_S,  _C|_S,  _C,     _C, \
        _C,     _C,     _C,     _C,     _C,     _C,     _C,     _C, \
        _C,     _C,     _C,     _C,     _C,     _C,     _C,     _C, \
        _S|_B,  _P,     _P,     _P,     _P,     _P,     _P,     _P, \
        _P,     _P,     _P,     _P,     _P,     _P,     _P,     _P, \
        _N,     _N,     _N,     _N,     _N,     _N,     _N,     _N, \
        _N,     _N,     _P,     _P,     _P,     _P,     _P,     _P, \
        _P,     _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U|_X,  _U, \
        _U,     _U,     _U,     _U,     _U,     _U,     _U,     _U, \
        _U,     _U,     _U,     _U,     _U,     _U,     _U,     _U, \
        _U,     _U,     _U,     _P,     _P,     _P,     _P,     _P, \
        _P,     _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L|_X,  _L, \
        _L,     _L,     _L,     _L,     _L,     _L,     _L,     _L, \
        _L,     _L,     _L,     _L,     _L,     _L,     _L,     _L, \
        _L,     _L,     _L,     _P,     _P,     _P,     _P,     _C

#define _CTYPE_DATA_128_255 \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0, \
        0,      0,      0,      0,      0,      0,      0,      0

const char _ctype_b[128 + 256] = {
        _CTYPE_DATA_128_255,
        _CTYPE_DATA_0_127,
        _CTYPE_DATA_128_255
};
