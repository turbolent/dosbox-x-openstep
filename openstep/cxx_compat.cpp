/* GCC 4.2's Mach-O backend omits this libstdc++ template data symbol when
 * basic_string<unsigned short> is instantiated implicitly. UTF-16 conversion
 * uses this specialization because the native library has no std::wstring. */
#include <string>
template const unsigned short std::basic_string<unsigned short>::_Rep::_S_terminal;
