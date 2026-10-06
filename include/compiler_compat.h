/* Language annotations shared by current compilers and OPENSTEP GCC 4.2.
 * Do not redefine C++ keywords: constant-expression users must remain explicit. */
#ifndef DOSBOX_COMPILER_COMPAT_H
#define DOSBOX_COMPILER_COMPAT_H
#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900)
#define DBX_CONSTEXPR constexpr
#define DBX_OVERRIDE override
#define DBX_NOEXCEPT noexcept
#define DBX_ALIGNAS(n) alignas(n)
#define DBX_STATIC_ASSERT(test, message) static_assert(test, message)
#define DBX_DEFAULT_UINT = unsigned int
#else
#define DBX_CONSTEXPR
#define DBX_OVERRIDE
/* Old libstdc++ containers do not declare nonthrowing destructors. */
#define DBX_NOEXCEPT
#define DBX_ALIGNAS(n) __attribute__((aligned(n)))
#define DBX_JOIN_IMPL(a,b) a##b
#define DBX_JOIN(a,b) DBX_JOIN_IMPL(a,b)
#define DBX_STATIC_ASSERT(test, message) typedef char DBX_JOIN(dbx_assert_,__LINE__)[(test) ? 1 : -1]
#define DBX_DEFAULT_UINT
#endif
#if defined(__GNUC__)
#define DBX_PACKED __attribute__((packed))
#else
#define DBX_PACKED
#endif
#endif
