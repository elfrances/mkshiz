#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "cli.h"
#include "output.h"

// Command status code
typedef enum CmdStat {
    OK = 0,
    ERROR = 1,
    EXIT = 2
} CmdStat;

static const char *cliHelp = \
    "Supported CLI commands:\n\n"
    "exit                   Exit the tool\n"
    "help                   Print this help\n"
    "history                Print the command history.\n"
    "save <fmt> <file>      Save the data in the specified format and file.\n"
    "                       The format can be: csv, gpx, shiz, tcx.\n"
    "sma <metric> <wind>    Compute the SMA of the specified metric with the\n"
    "                       specified window size. The metric can be: elevation,\n"
    "                       grade, speed. The window size must be an odd number.\n"
    "summary [detail]       Print a summary of the data.\n"
    "trim <a,b>             Remove the trackpoints between points 'a' and 'b'\n"
    "                       and close the distance and time gaps between them.\n"
    "undo                   Revert the last operation.\n"
    "\n";

typedef struct CliCmd {
    const char *name;
    CmdStat (*handler)(GpsTrk *, CmdArgs *);
} CliCmd;

static CmdStat cliCmdExit(GpsTrk *pTrk, CmdArgs *pArgs)
{
    return EXIT;
}

static CmdStat cliCmdHelp(GpsTrk *pTrk, CmdArgs *pArgs)
{
    printf("%s", cliHelp);
    return OK;
}

static CmdStat cliCmdSave(GpsTrk *pTrk, CmdArgs *pArgs)
{
    return OK;
}

static CmdStat cliCmdSma(GpsTrk *pTrk, CmdArgs *pArgs)
{
    if (pArgs->argc != 3) {
        printf("Syntax: sma <metric> <window>\n");
        return ERROR;
    }
    return OK;
}

static CmdStat cliCmdSummary(GpsTrk *pTrk, CmdArgs *pArgs)
{
    pArgs->outFile = stdout;
    pArgs->outFmt = nil;
    printOutput(pTrk, pArgs);
    return OK;
}

static CmdStat cliCmdTrim(GpsTrk *pTrk, CmdArgs *pArgs)
{
    return OK;
}

static CmdStat cliCmdUndo(GpsTrk *pTrk, CmdArgs *pArgs)
{
    return OK;
}

// CLI command table
static CliCmd cliCmdTbl [] = {
        { "exit",       cliCmdExit },
        { "help",       cliCmdHelp },
        { "save",       cliCmdSave },
        { "sma",        cliCmdSma },
        { "summary",    cliCmdSummary },
        { "trim",       cliCmdTrim },
        { "undo",       cliCmdUndo },
        { NULL,         NULL },
};

static CmdStat cliProcCmd(GpsTrk *pTrk, CmdArgs *pArgs)
{
    CmdStat s = ERROR;

    for (int i = 0; cliCmdTbl[i].name != NULL; i++) {
        const char *name = cliCmdTbl[i].name;
        if (strncmp(pArgs->argv[0], name, strlen(name)) == 0) {
            s = (*cliCmdTbl[i].handler)(pTrk, pArgs);
            break;
        }
    }

    return s;
}

static int cliParseCmdLine(const char *line, char *argv[])
{
    char *cmdLine;
    int argc = 0;

    // Clone the command line string because we'll
    // modify it while we parse it...
    if ((cmdLine = strdup(line)) != NULL) {
        char *p = cmdLine;
        char *pArg = NULL;
        int c;
        int skipWhite = true;

        // Break down the command line into single-word
        // arguments...
        while ((c = *p) != '\0') {
            if (isspace(c)) {
                if (skipWhite) {
                    p++;
                } else {
                    *p++ = '\0';
                    argv[argc] = strdup(pArg);
                    pArg = NULL;
                    if (++argc == MAX_ARGS)
                        break;
                    skipWhite = true;
                }
            } else {
                if (pArg == NULL) {
                    pArg = p;
                    skipWhite = false;
                }
                p++;
            }
        }

        if ((pArg != NULL) && (argc < MAX_ARGS)) {
            argv[argc] = strdup(pArg);
            argc++;
        }

        for (int n = argc; n <= MAX_ARGS; n++)
            argv[n] = NULL;

        free(cmdLine);
    }

    return argc;
}

int cliCmdHandler(GpsTrk *pTrk, CmdArgs *pArgs)
{
    char *line = NULL;
    CmdStat s = OK;

    // Init readline's history
    using_history();

    while (s != EXIT) {
        line = readline("cmd? ");
        if ((line != NULL) && (line[0] != '\0')) {
            // Parse the command line into token
            if ((pArgs->argc = cliParseCmdLine(line, pArgs->argv)) != 0) {
                // Go process the command!
                if ((s = cliProcCmd(pTrk, pArgs)) == OK) {
                    // Add command to the history
                    add_history(line);
                } else {
                    printf("Invalid command: %s\n", line);
                }

                // Free the argv[] strings
                for (int n = 0; n < pArgs->argc; n++) {
                    free(pArgs->argv[n]);
                    pArgs->argv[n] = NULL;
                }

            }
        }
        free(line);
    }

    return 0;
}
