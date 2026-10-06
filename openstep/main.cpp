/* Native OPENSTEP entry point. DOSBox-X is GPL-2.0-or-later; see COPYING. */
#include "emulator.h"
#include "cross.h"
#include "logging.h"
#include "host.h"
#include <stdexcept>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>

void DOSBOX_SetupConfigSections(void);
static bool Arguments(void)
{
    CommandLine &cmd = *control->cmdline;
    std::string option, value;
    cmd.ChangeOptStyle(CommandLine::gnu);
    cmd.BeginOpt(true);
    while (cmd.GetOpt(option)) {
        if (option == "help" || option == "h" || option == "version" || option == "v") {
            printf("DOSBox-X %s for OPENSTEP\n"
                "dosbox-x [program] [-conf file] [-c command] [-set 'section property=value']\n"
                "  -fullscreen -fastlaunch -exit -noautoexec -noconfig -defaultconf\n"
                "  -machine type -lang file -securemode -time-limit seconds\n", VERSION);
            return false;
        }
        /* Workspace adds these arguments; AppKit reads the original process
         * argv to complete its WindowServer launch handshake. MachLaunch's
         * two numbers arrive together in ONE argument, e.g. "54 332". */
        if (option == "MachLaunch" || option == "NXAutoLaunch") {
            if (!cmd.NextOptArgv(value)) throw std::runtime_error("Incomplete Workspace launch arguments");
        }
        else if (option == "fullscreen" || option == "fs") control->opt_fullscreen = true;
        else if (option == "fastlaunch") control->opt_fastlaunch = true;
        else if (option == "exit") control->opt_exit = true;
        else if (option == "defaultconf") control->opt_defaultconf = true;
        else if (option == "noautoexec") control->opt_noautoexec = true;
        else if (option == "noconfig") control->opt_noconfig = true;
        else if (option == "securemode") control->opt_securemode = true;
        else if (option == "log-con") control->opt_log_con = true;
        else if (option == "nolog") control->opt_nolog = true;
        else if (option == "eraseconf") control->opt_eraseconf = true;
        else if (option == "conf" || option == "c" || option == "o" || option == "set" ||
                 option == "machine" || option == "lang" || option == "time-limit") {
            if (!cmd.NextOptArgv(value)) throw std::runtime_error("Missing argument to -" + option);
            if (option == "conf") control->config_file_list.push_back(value);
            else if (option == "c") control->opt_c.push_back(value);
            else if (option == "o") control->opt_o.push_back(value);
            else if (option == "set") control->opt_set.push_back(value);
            else if (option == "machine") control->opt_machine = value;
            else if (option == "lang") control->opt_lang = value;
            else {
                char *end = NULL;
                double seconds = strtod(value.c_str(), &end);
                if (!end || *end || !(seconds > 0 && seconds <= 4294967))
                    throw std::runtime_error("Invalid -time-limit");
                control->opt_time_limit = seconds;
            }
        } else throw std::runtime_error("Unsupported OPENSTEP option: -" + option);
    }
    cmd.EndOpt();
    if (control->opt_c.size() > 11)
        throw std::runtime_error("At most eleven -c commands are supported; use a DOS batch file for longer scripts");
    cmd.BeginOpt(false);
    while (!cmd.CurrentArgvEnd()) {
        cmd.GetCurrentArgv(value);
        if (value.compare(0, 2, "~/") == 0) {
            const char *home = getenv("HOME");
            if (!home) throw std::runtime_error("HOME is not set");
            value = std::string(home) + value.substr(1);
        }
        char resolved[CROSS_LEN];
        struct stat st;
        if (!realpath(value.c_str(), resolved) || stat(resolved, &st))
            throw std::runtime_error("Cannot open startup path: " + value);
        value = resolved;
        /* These paths become DOS batch commands. Reject characters that its
         * parser would expand or interpret as command syntax. */
        if (value.find_first_of("\"\r\n%&|<>") != std::string::npos)
            throw std::runtime_error("Unsupported character in startup path: " + value);
        const char *ext = strrchr(value.c_str(), '.');
        if (S_ISDIR(st.st_mode) || (S_ISREG(st.st_mode) && ext && !strcasecmp(ext, ".zip")))
            control->auto_bat_additional.push_back("@mount c: \"" + value + "\" -nl");
        else if (S_ISREG(st.st_mode) && ext && (!strcasecmp(ext, ".bat") ||
                 !strcasecmp(ext, ".exe") || !strcasecmp(ext, ".com")))
            control->auto_bat_additional.push_back(value);
        else throw std::runtime_error("Expected a directory, ZIP, BAT, COM or EXE: " + value);
        cmd.NextArgv();
    }
    cmd.EndOpt();
    return true;
}

static std::string ExecutablePath(const char *argument)
{
    char resolved[CROSS_LEN];
    const char *loaded = OSHostExecutablePath();
    if (loaded && realpath(loaded, resolved)) return resolved;
    /* Workspace can use the login-style spelling "-DOSBox-X". */
    if (argument[0] == '-' && argument[1]) ++argument;
    struct stat current;
    if (!stat(argument, &current) && S_ISREG(current.st_mode) &&
        !access(argument, X_OK) && realpath(argument, resolved)) return resolved;
    if (strchr(argument, '/')) {
        if (realpath(argument, resolved)) return resolved;
    } else {
        const char *environment = getenv("PATH");
        const std::string path = environment ? environment : "/usr/local/bin:/usr/bin:/bin";
        size_t start = 0;
        do {
            const size_t end = path.find(':', start);
            std::string directory = path.substr(start, end == std::string::npos ? end : end - start);
            if (directory.empty()) directory = ".";
            const std::string candidate = directory + "/" + argument;
            struct stat st;
            if (!stat(candidate.c_str(), &st) && S_ISREG(st.st_mode) &&
                !access(candidate.c_str(), X_OK) && realpath(candidate.c_str(), resolved)) return resolved;
            if (end == std::string::npos) break;
            start = end + 1;
        } while (true);
    }
    throw std::runtime_error("Cannot resolve the executable path for restart");
}

static void CreatePreferences(void)
{
    /* Publish a complete default file without replacing existing preferences. */
    std::string name = configfile + ".XXXXXX";
    std::vector<char> temporary(name.begin(), name.end()); temporary.push_back(0);
    int fd = mkstemp(&temporary[0]);
    if (fd < 0) { perror("DOSBox-X preferences"); return; }
    close(fd);
    bool complete = control->PrintConfig(&temporary[0], false, true);
    if (complete) {
        FILE *file = fopen(&temporary[0], "a");
        if (file) {
            fprintf(file, "[openstep]\n");
            control->GetSection("openstep")->PrintData(file, 1, true);
            complete = !ferror(file);
            if (fclose(file)) complete = false;
        } else complete = false;
    }
    if (complete) {
        if (link(&temporary[0], configfile.c_str()) && errno != EEXIST)
            perror("DOSBox-X preferences");
    } else fprintf(stderr, "DOSBox-X: Cannot write default preferences\n");
    unlink(&temporary[0]);
}

static void SetOption(const std::string &setting)
{
    const size_t equal = setting.find('=');
    if (equal == std::string::npos) throw std::runtime_error("Expected property=value: " + setting);
    const size_t space = setting.find(' ');
    Section *section = NULL;
    std::string property = setting.substr(0, equal);
    if (space != std::string::npos && space < equal) {
        section = control->GetSection(setting.substr(0, space));
        if (section) property = setting.substr(space + 1, equal - space - 1);
    }
    if (!section) section = control->GetSectionFromProperty(property.c_str());
    if (!section || !section->HandleInputline((property + setting.substr(equal)).c_str()))
        throw std::runtime_error("Cannot set configuration: " + setting);
}

static void RunGuest(void *status)
{
    int &result = *static_cast<int *>(status);
    try {
        OSEmulatorInit(); OSEmulatorRun();
    } catch (const char *message) {
        fprintf(stderr, "DOSBox-X: %s\n", message); result = 1;
    } catch (const std::exception &error) {
        fprintf(stderr, "DOSBox-X: %s\n", error.what()); result = 1;
    } catch (int signal) {
        if (signal != 0 && signal != 1) {
            fprintf(stderr, "DOSBox-X: Unexpected startup transition %d\n", signal); result = 1;
        }
    } catch (...) {
        fprintf(stderr, "DOSBox-X: Unexpected emulator exception\n"); result = 1;
    }
    /* Finalize captures while AppKit can still run a completion dialog. */
    try { OSEmulatorShutdown(); }
    catch (...) { fprintf(stderr, "DOSBox-X: Emulator shutdown failed\n"); result = 1; }
}

int main(int argc, char **argv)
{
    int result = 0;
    CommandLine command(argc, argv);
    Config configuration(&command);
    control = &configuration;
    try {
        if (!Arguments()) { control = NULL; return 0; }
        working_dir = Cross::GetCurDir();
        dosboxpath = ExecutablePath(argc ? argv[0] : "dosbox-x");
        /* Foundation derives its defaults domain and main bundle from argv[0]. */
        if (argc) argv[0] = const_cast<char *>(dosboxpath.c_str());
        LOG::EarlyInit(); LOG::SetupConfigSection();
        DOSBOX_SetupConfigSections();
        OSFrontendConfigure();
        configfile = Cross::CreatePlatformConfigDir() + Cross::GetPlatformConfigName();
        if (!control->opt_defaultconf && control->config_file_list.empty()) {
            if (!control->ParseConfigFile("dosbox-x.conf") &&
                !control->ParseConfigFile(configfile.c_str())) CreatePreferences();
        }
        for (size_t i = 0; i < control->config_file_list.size(); ++i)
            if (!control->ParseConfigFile(control->config_file_list[i].c_str()))
                throw std::runtime_error("Cannot read config: " + control->config_file_list[i]);
        if (control->opt_eraseconf)
            for (size_t i = 0; i < control->config_file_list.size(); ++i)
                remove(control->config_file_list[i].c_str());
        for (size_t i = 0; i < control->opt_set.size(); ++i) SetOption(control->opt_set[i]);
        /* AUTOEXEC applies -securemode after the authorized startup mounts. */
        LOG::Init();
        OSMenuInit(); OSFrontendStart();
        OSApplicationRun(RunGuest, &result);
    } catch (const char *message) {
        fprintf(stderr, "DOSBox-X: %s\n", message); result = 1;
    } catch (const std::exception &error) {
        fprintf(stderr, "DOSBox-X: %s\n", error.what()); result = 1;
    }
    OSFrontendShutdown(); LOG::Exit();
    mainMenu.clear_all_menu_items();
    control = NULL;
    return result ? result : OSFrontendRestart();
}
