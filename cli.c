#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "cli.h"
#include "comp.h"
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
    "max <metric> <value>   Limit the maximum value of the specified metric.\n"
    "min <metric> <value>   Limit the minimum value of the specified metric.\n"
    "save <file> [<format>] Save the data in the specified format and file.\n"
    "                       The output format can be: csv, gpx, shiz, tcx.\n"
    "show [<a> <b>]         Show trackpoints in plain text form.\n"
    "sma <metric> <window>  Compute the SMA of the specified metric with the\n"
    "                       specified window size.\n"
    "summary [detail]       Print a summary of the data.\n"
    "trim <a> <b>           Remove the trackpoints between points <a> and <b>\n"
    "                       (inclusive) and close the distance and time gaps \n"
    "                       between them.\n"
    "undo                   Revert the last operation.\n"
    "\n"
    "The metric can be: elevation, grade, speed.\n\n";

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

static CmdStat cliCmdMax(GpsTrk *pTrk, CmdArgs *pArgs)
{
    char *metric = pArgs->argv[1];
    char *value = pArgs->argv[2];
    double maxVal = 0.0;

    if (pArgs->argc != 3) {
        printf("Syntax: max <metric> <value>\n");
        return ERROR;
    }

    if (strcmp(metric, "elevation") == 0) {
        pArgs->smaMetric = elevation;
    } else if (strcmp(metric, "grade") == 0) {
        pArgs->smaMetric = grade;
    } else if (strcmp(metric, "speed") == 0) {
        pArgs->smaMetric = speed;
    } else {
        return invArgMsg(metric, NULL);
    }

    if (sscanf(value, "%le", &maxVal) != 1) {
        return invArgMsg(value, NULL);
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    {
        TrkPt *p;

        TAILQ_FOREACH(p, &pTrk->trkPtList, tqEntry) {
            if ((pArgs->smaMetric == elevation) && (p->elevation > maxVal)) {
                p->elevation = maxVal;
            } else if ((pArgs->smaMetric == grade) && (p->grade > maxVal)) {
                p->grade = maxVal;
            } else if ((pArgs->smaMetric == speed) && (p->speed > maxVal)) {
                p->speed = maxVal;
            }
        }
    }

    // Recompute min/avg/max values
    computeMinMaxValues(pTrk);

    return OK;
}

static CmdStat cliCmdMin(GpsTrk *pTrk, CmdArgs *pArgs)
{
    char *metric = pArgs->argv[1];
    char *value = pArgs->argv[2];
    double minVal = 0.0;

    if (pArgs->argc != 3) {
        printf("Syntax: min <metric> <value>\n");
        return ERROR;
    }

    if (strcmp(metric, "elevation") == 0) {
        pArgs->smaMetric = elevation;
    } else if (strcmp(metric, "grade") == 0) {
        pArgs->smaMetric = grade;
    } else if (strcmp(metric, "speed") == 0) {
        pArgs->smaMetric = speed;
    } else {
        return invArgMsg(metric, NULL);
    }

    if (sscanf(value, "%le", &minVal) != 1) {
        return invArgMsg(value, NULL);
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    {
        TrkPt *p;

        TAILQ_FOREACH(p, &pTrk->trkPtList, tqEntry) {
            if ((pArgs->smaMetric == elevation) && (p->elevation < minVal)) {
                p->elevation = minVal;
            } else if ((pArgs->smaMetric == grade) && (p->grade < minVal)) {
                p->grade = minVal;
            } else if ((pArgs->smaMetric == speed) && (p->speed < minVal)) {
                p->speed = minVal;
            }
        }
    }

    // Recompute min/avg/max values
    computeMinMaxValues(pTrk);

    return OK;
}

static CmdStat cliCmdSave(GpsTrk *pTrk, CmdArgs *pArgs)
{
    if (pArgs->argc < 2) {
        printf("Syntax: save <file> [<format>]\n");
        return ERROR;
    }

    // Open the output file for writing
    if ((pArgs->outFile = fopen(pArgs->argv[1], "w")) == NULL) {
        return errMsg("Can't open output file");
    }

    if (pArgs->argc == 3) {
        char *outFmt = pArgs->argv[2];
        if (strcmp(outFmt, "csv") == 0) {
            pArgs->outFmt = csv;
        } else if (strcmp(outFmt, "gpx") == 0) {
            pArgs->outFmt = gpx;
        } else if (strcmp(outFmt, "shiz") == 0) {
            pArgs->outFmt = shiz;
        } else if (strcmp(outFmt, "tcx") == 0) {
            pArgs->outFmt = tcx;
        } else {
            return invArgMsg(outFmt, NULL);
        }
    } else {
        pArgs->outFmt = csv;
    }

    if (pArgs->outFmt == csv) {
        // Use HH:MM:SS time format so that it is easy
        // to correlate the TrkPt's with the MP4 video.
        pArgs->tsFmt = hms;
    }

    printOutput(pTrk, pArgs);

    fclose(pArgs->outFile);
    pArgs->outFile = NULL;

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
    char *smaMetric = pArgs->argv[1];
    char *smaWindow = pArgs->argv[2];

    if (pArgs->argc != 3) {
        printf("Syntax: sma <metric> <window>\n");
        return ERROR;
    }

    if (strcmp(smaMetric, "elevation") == 0) {
        pArgs->smaMetric = elevation;
    } else if (strcmp(smaMetric, "grade") == 0) {
        pArgs->smaMetric = grade;
    } else if (strcmp(smaMetric, "speed") == 0) {
        pArgs->smaMetric = speed;
    } else {
        return invArgMsg(smaMetric, NULL);
    }

    if ((sscanf(smaWindow, "%d", &pArgs->smaWindow) != 1) ||
        (pArgs->smaWindow < 3)) {
        return invArgMsg(smaWindow, NULL);
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    // Compute the SMA of the specified metric over
    // the specified window.
    compSMA(pTrk, pArgs);

    if (pArgs->smaMetric == elevation) {
        // Recompute the metrics
        compMetrics(pTrk, pArgs);
    } else {
        // Recompute min/avg/max values
        computeMinMaxValues(pTrk);
    }

    return OK;
}

static CmdStat cliCmdSummary(GpsTrk *pTrk, CmdArgs *pArgs)
{
    if ((pArgs->argc == 2) && (strcmp(pArgs->argv[1], "detail") == 0)) {
        pArgs->detail = true;
    }
    pArgs->outFile = stdout;
    pArgs->outFmt = nil;
    printOutput(pTrk, pArgs);
    pArgs->detail = false;
    return OK;
}

static CmdStat cliCmdTrim(GpsTrk *pTrk, CmdArgs *pArgs)
{
    int from, to;

    if ((pArgs->argc != 3) ||
        (sscanf(pArgs->argv[1], "%d", &from) != 1) ||
        (sscanf(pArgs->argv[2], "%d", &to) != 1)) {
        printf("Syntax: trim <from> <to>\n");
        return ERROR;
    }

    if (from > to) {
        printf("The <from> value must be lower than the <to> value\n");
        return ERROR;
    }

    if (to > pTrk->numTrkPts) {
        printf("The <to> value exceeds the total number of TrkPt's (%d)\n", pTrk->numTrkPts);
        return ERROR;
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    {
        TrkPt *p = TAILQ_FIRST(&pTrk->trkPtList);
        Bool discTrkPt = false;
        Bool trimTrkPts = false;
        double trimmedTime = 0.0;
        double trimmedDistance = 0.0;
        TrkPt *p0 = NULL;
        int index = 0;

        // Remove all TrkPt's in the <from>-<to> range and
        // close the distance and time gaps.
        while (p != NULL) {
            discTrkPt = false;

            // Do we need to trim out this TrkPt?
            if (p->index == from) {
                // Start trimming
                trimTrkPts = true;
                pTrk->numTrimTrkPts++;
                discTrkPt = true;
                p0 = p; // set baseline
            } else if (p->index == to) {
                // Stop trimming
                trimTrkPts = false;
                trimmedTime = p->timestamp - p0->timestamp;     // total time trimmed out
                trimmedDistance = p->distance - p0->distance;   // total distance trimmed out
                pTrk->numTrimTrkPts++;
                discTrkPt = true;
            } else if (trimTrkPts) {
                // Trim this point
                pTrk->numTrimTrkPts++;
                discTrkPt = true;
            }

            // Discard?
            if (discTrkPt) {
                // Remove this TrkPt from the list
                p = remTrkPt(pTrk, p);
            } else {
                // If we trimmed out some previous TrkPt's, then we
                // need to adjust the timestamp and distance values
                // of this TrkPt so as to "close the gap".
                if (p0 != NULL) {
                    p->timestamp -= trimmedTime;
                    p->distance -= trimmedDistance;
                }
                p = nxtTrkPt(NULL, p);
            }
        }

        // Recompute the index of all the TrkPt's
        TAILQ_FOREACH(p, &pTrk->trkPtList, tqEntry) {
            p->index = index++;
        }

        // Update the total number of TrkPt's
        pTrk->numTrkPts = index;
    }

    // Recompute the metrics
    compMetrics(pTrk, pArgs);

    return OK;
}

static CmdStat cliCmdUndo(GpsTrk *pTrk, CmdArgs *pArgs)
{
    if (TAILQ_FIRST(&pTrk->savedTrkPtList) != NULL) {
        restoreTrkPts(pTrk);
    } else {
        printf("Nothing to undo!\n");
    }

    return OK;
}

// CLI command table
static CliCmd cliCmdTbl [] = {
        { "exit",       cliCmdExit },
        { "help",       cliCmdHelp },
        { "max",        cliCmdMax },
        { "min",        cliCmdMin },
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
            // Parse the command line into tokens
            if ((pArgs->argc = cliParseCmdLine(line, pArgs->argv)) != 0) {
                // Add command to the history
                add_history(line);

                // Go process the command!
                if ((s = cliProcCmd(pTrk, pArgs)) == ERROR) {
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
