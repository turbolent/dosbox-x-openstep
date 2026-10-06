#ifndef DOSBOX_OPENSTEP_SHELL_ENV_H
#define DOSBOX_OPENSTEP_SHELL_ENV_H
#include <string>

typedef bool (*OSEnvLookup)(void *, const std::string &, std::string &);
/* DOS percent expansion, including %% and FOR's first percent. On overflow or
 * cyclic expansion, return false and leave the input unchanged. */
bool OSExpandDOSEnv(std::string &line, OSEnvLookup lookup, void *context,
                   size_t limit);
#endif
