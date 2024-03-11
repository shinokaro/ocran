#include <windows.h>
#include <string.h>
#include "error.h"
#include "inst_dir.h"

static char* ConcatStr(const char *first, ...) {
    va_list args;
    va_start(args, first);
    size_t len = 0;
    for (const char* s = first; s; s = va_arg(args, const char*)) {
        len += strlen(s);
    }
    va_end(args);

    char *str = LocalAlloc(LPTR, len + 1);

    if (str == NULL) {
        LAST_ERROR("Failed to allocate memory");
        return NULL;
    }

    va_start(args, first);
    char *p = str;
    for (const char *s = first; s; s = va_arg(args, const char*)) {
        size_t l = strlen(s);
        memcpy(p, s, l);
        p += l;
    }
    str[len] = '\0';
    va_end(args);

    return str;
}

#define CONCAT_STR3(s1, s2, s3) ConcatStr(s1, s2, s3, NULL)

static char *EscapeAndQuoteCmdArg(const char* arg)
{
    size_t arg_len = strlen(arg);
    size_t count = 0;
    for (size_t i = 0; i < arg_len; i++) { if (arg[i] == '\"') count++; }
    char *sanitized = (char *)LocalAlloc(LPTR, arg_len + count * 2 + 3);
    if (sanitized == NULL) {
        LAST_ERROR("Failed to allocate memory");
        return NULL;
    }

    char *p = sanitized;
    *p++ = '\"';
    for (size_t i = 0; i < arg_len; i++) {
        if (arg[i] == '\"') { *p++ = '\\'; }
        *p++ = arg[i];
    }
    *p++ = '\"';
    *p = '\0';

    return sanitized;
}

static BOOL ParseArguments(const char *args, size_t args_size, size_t *out_argc, const char ***out_argv)
{
    size_t local_argc = 0;
    for (const char *s = args; s < (args + args_size); s += strlen(s) + 1) {
        local_argc++;
    }

    const char **local_argv = (const char **)LocalAlloc(LPTR, (local_argc + 1) * sizeof(char *));
    if (local_argv == NULL) {
        FATAL("Failed to memory allocate for argv");
        return FALSE;
    }

    const char *s = args;
    for (size_t i = 0; i < local_argc; i++) {
        local_argv[i] = s;
        s += strlen(s) + 1;
    }
    local_argv[local_argc] = NULL;

    *out_argc = local_argc;
    *out_argv = local_argv;
    return TRUE;
}

static char *BuildCommandLine(size_t argc, const char *argv[])
{
    char *command_line = EscapeAndQuoteCmdArg(argv[0]);
    if (command_line == NULL) {
        FATAL("Failed to initialize command line with first arg");
        return NULL;
    }

    for (size_t i = 1; i < argc; i++) {
        char *str;
        if (i == 1) {
            str = ExpandInstDirPath(argv[1]);
            if (str == NULL) {
                FATAL("Failed to expand script name to installation directory at arg index 1");
                goto cleanup;
            }
        } else {
            str = ReplaceInstDirPlaceholder(argv[i]);
            if (str == NULL) {
                FATAL("Failed to replace arg placeholder at arg index %d", i);
                goto cleanup;
            }
        }

        char *sanitized = EscapeAndQuoteCmdArg(str);
        LocalFree(str);
        if (sanitized == NULL) {
            FATAL("Failed to sanitize arg at arg index %d", i);
            goto cleanup;
        }
        char *base = command_line;
        command_line = CONCAT_STR3(base, " ", sanitized);
        LocalFree(base);
        LocalFree(sanitized);
        if (command_line == NULL) {
            FATAL("Failed to build command line after adding arg at arg index %d", i);
            goto cleanup;
        }
    }

    return command_line;

cleanup:
    LocalFree(command_line);

    return NULL;
}

static const char *Script_ApplicationName = NULL;
static char *Script_CommandLine = NULL;

#define HAS_SCRIPT_INFO (Script_ApplicationName && Script_CommandLine)

BOOL GetScriptInfo(const char **app_name, char **cmd_line)
{
    if (HAS_SCRIPT_INFO) {
        *app_name = Script_ApplicationName;
        *cmd_line = Script_CommandLine;
        return TRUE;
    } else {
        return FALSE;
    }
}

BOOL InitializeScriptInfo(const char *args, size_t args_size, char *extra_args)
{
    if (HAS_SCRIPT_INFO) {
        FATAL("Script info is already set");
        return FALSE;
    }

    size_t argc;
    const char **argv = NULL;
    if (!ParseArguments(args, args_size, &argc, &argv)) {
        FATAL("Failed to parse arguments");
        return FALSE;
    }

    // Set Script_ApplicationName
    char *application_name = ExpandInstDirPath(argv[0]);
    if (application_name == NULL) {
        FATAL("Failed to expand application name to installation directory");
        LocalFree(argv);
        return FALSE;
    }

    // Set Script_CommandLine
    char *cmd_line = BuildCommandLine(argc, argv);
    if (cmd_line == NULL) {
        FATAL("Failed to build command line");
        LocalFree(argv);
        return FALSE;
    }

    char *command_line = CONCAT_STR3(cmd_line, " ", extra_args);
    LocalFree(argv);
    LocalFree(cmd_line);
    if (command_line == NULL) {
        FATAL("Failed to build command line");
        return FALSE;
    }

    Script_ApplicationName = application_name;
    Script_CommandLine = command_line;

    return TRUE;
}

void FreeScriptInfo(void)
{
    LocalFree((char *)Script_ApplicationName);
    Script_ApplicationName = NULL;

    LocalFree(Script_CommandLine);
    Script_CommandLine = NULL;
}