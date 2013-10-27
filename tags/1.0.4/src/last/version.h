#pragma once

#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#define __LOC__ __FILE__ "("__STR1__(__LINE__)") : Info : "

#pragma message(__LOC__"Run the NAnt script to get proper version info")

#define FILEVER         1, 0, 0, 0
#define PRODUCTVER      1, 0, 0, 0
#define STRFILEVER      "1.0.0.0\0"
#define STRPRODUCTVER   "1.0.0.0\0"

#define BP_VERMAJOR     1
#define BP_VERMINOR     0
#define BP_VERMICRO     0
#define BP_VERBUILD     0
#define BP_VERDATE      "no date"
