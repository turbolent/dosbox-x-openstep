#include "shell_env.h"
#include <algorithm>
#include <cctype>
#include <cstring>

bool OSExpandDOSEnv(std::string &line, OSEnvLookup lookup, void *context,
                   size_t limit)
{
    if (line.size() > limit) return false;
    size_t begin = 0;
    while (begin < line.size() && std::isspace((unsigned char)line[begin])) ++begin;
    std::string prefix;
    if (line.size() - begin >= 4 &&
        std::toupper((unsigned char)line[begin]) == 'F' &&
        std::toupper((unsigned char)line[begin+1]) == 'O' &&
        std::toupper((unsigned char)line[begin+2]) == 'R' && line[begin+3] == ' ') {
        const size_t first = line.find('%');
        if (first != std::string::npos) prefix = line.substr(0, first+1);
    }
    std::string text = line.substr(prefix.size());
    for (unsigned substitutions = 0; ; ++substitutions) {
        size_t start = text.find('%'), end = std::string::npos;
        while (start != std::string::npos && start+1 < text.size()) {
            const unsigned char next = text[start+1];
            if (next < '0' || next > '9') {
                end = text.find('%', start+1);
                if (end != std::string::npos) break;
            }
            start = text.find('%', start+1);
        }
        if (end == std::string::npos) break;
        if (substitutions >= 8192) return false;
        std::string name = text.substr(start+1, end-start-1), value;
        if (name.empty()) value.assign(1, '\b');
        else {
            name.erase(0, name.find_first_not_of(' '));
            if (!name.empty() && !lookup(context, name, value)) value.clear();
        }
        const size_t length = end-start+1;
        if (value == text.substr(start, length)) return false;
        const size_t retained = prefix.size() + text.size() - length;
        if (retained > limit || value.size() > limit - retained) return false;
        text.replace(start, length, value);
    }
    std::replace(text.begin(), text.end(), '\b', '%');
    line = prefix + text;
    return true;
}
