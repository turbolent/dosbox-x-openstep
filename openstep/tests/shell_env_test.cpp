#include "shell_env.h"
#include <cassert>
#include <cstdio>
#include <map>
#if OS_ENV_REFERENCE
#include <regex>
#include <algorithm>
#endif

typedef std::map<std::string, std::string> Environment;
static bool lookup(void *context, const std::string &name, std::string &value) {
    const Environment &env = *static_cast<Environment*>(context);
    Environment::const_iterator it = env.find(name);
    if (it == env.end()) return false;
    value = it->second;
    return true;
}
static void check(Environment &env, const char *input, const char *expected) {
    std::string text(input);
    assert(OSExpandDOSEnv(text, lookup, &env, 4096));
    assert(text == expected);
}
int main() {
    Environment env;
    env["A"] = "one"; env["B"] = "%A%"; env["PATH"] = "C:\\DOS;C:\\WINDOWS";
    check(env, "nothing", "nothing");
    check(env, "%", "%");
    check(env, "%%", "%");
    check(env, "% %", "");
    check(env, "%  %", "");
    check(env, "% % %", " %");
    check(env, "%A% %MISSING%", "one ");
    check(env, "%%%A%%%", "%one%");
    check(env, "%  A%", "one");
    check(env, "%A %", "");
    check(env, "%1%", "%1%");
    check(env, "%B%", "one");
    check(env, "FOR %i IN (%A%) DO ECHO %i", "FOR %i IN (one) DO ECHO %i");
    check(env, "echo %PATH%", "echo C:\\DOS;C:\\WINDOWS");
    env["LOOP"] = "%LOOP%";
    std::string text("%LOOP%");
    assert(!OSExpandDOSEnv(text, lookup, &env, 4096) && text == "%LOOP%");
    env["X"] = "%Y%"; env["Y"] = "%X%";
    text = "%X%";
    assert(!OSExpandDOSEnv(text, lookup, &env, 4096) && text == "%X%");
    env["BIG"] = std::string(4097, 'x'); text = "%BIG%";
    assert(!OSExpandDOSEnv(text, lookup, &env, 4096) && text == "%BIG%");
    env["BIG"] = std::string(4096, 'x');
    assert(OSExpandDOSEnv(text, lookup, &env, 4096) && text.size() == 4096);
#if OS_ENV_REFERENCE
    /* Compare the scanner with the upstream regex on generated input, using
     * nonrecursive values so the reference cannot loop indefinitely. */
    const std::regex pattern("\\%([^%0-9][^%]*)?%");
    unsigned state = 1;
    const char chars[] = "%AB012 _~ ";
    for (unsigned n = 0; n < 5000; ++n) {
        std::string input;
        for (unsigned j = 0; j < n%64; ++j) {
            state = state * 1664525u + 1013904223u;
            input += chars[(state >> 16) % (sizeof(chars)-1)];
        }
        text = input;
        assert(OSExpandDOSEnv(text, lookup, &env, 4096));
        std::smatch match;
        while (std::regex_search(input, match, pattern)) {
            std::string name = match[1].str(), value;
            if (name.empty()) value.assign(1, '\b');
            else {
                name.erase(0, name.find_first_not_of(' '));
                if (!name.empty()) lookup(&env, name, value);
            }
            input.replace(match.position(0), match.length(0), value);
        }
        std::replace(input.begin(), input.end(), '\b', '%');
        assert(text == input);
    }
#endif
    puts("OPENSTEP DOS environment expansion tests passed");
}
