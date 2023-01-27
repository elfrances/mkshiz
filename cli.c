#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "cli.h"
#include "output.h"
#include "trkpt.h"

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
    "save <file> [<fmt>]    Save the data in the specified format and file.\n"
    "                       The output format can be: csv, gpx, shiz, tcx.\n"
    "show [<a> <b>]         Show trackpoints in plain text form.\n"
    "sma <metric> <wind>    Compute the SMA of the specified metric with the\n"
    "                       specified window size. The metric can be: elevation,\n"
    "                       grade, speed. The window size must be an odd number.\n"
    "summary [detail]       Print a summary of the data.\n"
    "trim <a> <b>           Remove the trackpoints between points <a> and <b>\n"
    "                       (inclusive) and close the distance and time gaps \n"
    "                       between them.\n"
    "undo                   Revert the last operation.\n"
    "\n";

typedef struct CliCmd {
    const char *name;
    CmdStat (*handler)(GpsTrk *, CmdArgs *);
} CliCmd;

static CmdStat invArgMsg(const char *arg, const char *msg)
{
    printf("Invalid argument '%s'\n", arg);
    if (msg)
        printf("%s\n", msg);
    return ERROR;
}

static CmdStat errMsg(const char *msg)
{
    printf("%s\n", msg);
    return ERROR;
}

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

static CmdStat cliCmdShow(GpsTrk *pTrk, CmdArgs *pArgs)
{
    int a = -1;
    int b = -1;
    TrkPt *tp;

    if (pArgs->argc == 3) {
        if (sscanf(pArgs->argv[1], "%d", &a) != 1) {
            return invArgMsg(pArgs->argv[1], NULL);
        }
        if (sscanf(pArgs->argv[2], "%d", &b) != 1) {
            return invArgMsg(pArgs->argv[2], NULL);
        }
        if (b < a) {
            return errMsg("End point <b> must be higher than start point <a>");
        }
    }

    TAILQ_FOREACH(tp, &pTrk->trkPtList, tqEntry) {
        if (((a == -1) || (tp->index >= a)) && ((b == -1) || (tp->index <= b))) {
            printf("TrkPt #%u at %s {\n", tp->index, fmtTrkPtIdx(tp));
            printf("  latitude=%.10lf longitude=%.10lf elevation=%.10lf time=%.3lf distance=%.10lf speed=%.10lf dist=%.10lf run=%.10lf rise=%.10lf grade=%.2lf\n",
                    tp->latitude, tp->longitude, tp->elevation, tp->timestamp, tp->distance, tp->speed, tp->dist, tp->run, tp->rise, tp->grade);
            printf("}\n");
        }
    }

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
        { "show",       cliCmdShow },
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
        line = readline("CLI> ");
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
