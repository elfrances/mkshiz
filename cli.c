#include <ctype.h>
#include <math.h>
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
    "Supported CLI commands:\n"
    "\n"
    "cma <metric> <window> [<range>]    Smooth the specified metric using a CMA filter.\n"
    "exit                               Exit the tool\n"
    "help                               Print this help\n"
    "history                            Print the command history.\n"
    "max <metric> <value> [<range>]     Limit the maximum value of the specified metric.\n"
    "min <metric> <value> [<range>]     Limit the minimum value of the specified metric.\n"
    "save <file> [<format>]             Save the data in the specified format and file.\n"
    "                                   The output format can be: csv, gpx, shiz, tcx.\n"
    "scale <metric> <factor> [<range>]  Scale the specified metric by the specified factor.\n"
    "sgf <metric> <window> [<range>]    Smooth the specified metric using the Savitzky-Golay\n"
    "                                   filter.\n"
    "show [<range>]                     Show trackpoints in plain text form.\n"
    "sma <metric> <window> [<range>]    Smooth the specified metric using an SMA filter.\n"
    "summary [detail]                   Print a summary of the data.\n"
    "trim <range>                       Remove the trackpoints within the specified range\n"
    "                                   and close the distance and time gaps between them.\n"
    "undo                               Revert the last operation.\n"
    "\n"
    "The first and last trackpoints within a range can be specified by either their index or by\n"
    "their timestamp. Additionally, the keywords \"start\" and \"end\" are used to indicate the\n"
    "first and last trackpoints in the entire activity, respectively.\n"
    "\n"
    "The metric can be: elevation, grade, speed.\n"
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

static ActMetric getActMetric(const char *metric)
{
    ActMetric actMetric = invalid;

    if (strcmp(metric, "elevation") == 0) {
        actMetric = elevation;
    } else if (strcmp(metric, "grade") == 0) {
        actMetric = grade;
    } else if (strcmp(metric, "speed") == 0) {
        actMetric = speed;
    } else if (strcmp(metric, "gradeChange") == 0) {
        actMetric = gradeChange;
    }

    return actMetric;
}

static int findTrkPtByTime(GpsTrk *pTrk, time_t time)
{
    TrkPt *p;

    TAILQ_FOREACH(p, &pTrk->trkPtList, tqEntry) {
        time_t ts = (time_t) (p->timestamp - pTrk->startTime);
        if (ts == time)
            return p->index;
    }

    return -1;
}

static int getTrkPt(GpsTrk *pTrk, const char *arg)
{
    int hr, min, sec;
    int index = -1;

    if (strcmp(arg, "start") == 0) {
        return 0;
    } else if (strcmp(arg, "end") == 0) {
        return (pTrk->numTrkPts - 1);
    } else if (sscanf(arg, "%d:%d:%d", &hr, &min, &sec) == 3) {
        if ((hr >= 0) &&
            (min >= 0) && (min <= 59) &&
            (sec >= 0) && (sec <= 59)) {
            index = findTrkPtByTime(pTrk, (hr * 3600 + min * 60 + sec));
        }
    } else {
        sscanf(arg, "%d", &index);
    }

    if ((index < 0) || (index > (pTrk->numTrkPts - 1)))
        return -1;

    return index;
}

static int getTrkPtRange(GpsTrk *pTrk, const char *from, const char *to, TrkPtRange *pRange)
{
    if ((pRange->from = getTrkPt(pTrk, from)) < 0) {
        return invArgMsg(from, NULL);
    }

    if ((pRange->to = getTrkPt(pTrk, to)) < 0) {
        return invArgMsg(to, NULL);
    }

    return 0;
}

static CmdStat cliCmdCma(GpsTrk *pTrk, CmdArgs *pArgs)
{
    char *smaMetric = pArgs->argv[1];
    char *smaWindow = pArgs->argv[2];

    if (pArgs->argc < 3) {
        printf("Syntax: cma <metric> <window> [<from> <to>]\n");
        return ERROR;
    }

    if ((pArgs->actMetric = getActMetric(smaMetric)) == invalid) {
        return invArgMsg(smaMetric, NULL);
    }

    if ((sscanf(smaWindow, "%d", &pArgs->smaWindow) != 1) ||
        ((pArgs->smaWindow % 2) == 0)) {
        return invArgMsg(smaWindow, NULL);
    }

    if (pArgs->argc == 5) {
        if (getTrkPtRange(pTrk, pArgs->argv[3], pArgs->argv[4], &pArgs->range) < 0) {
            return -1;
        }
    } else {
        pArgs->range.from = 0;
        pArgs->range.to = pTrk->numTrkPts - 1;
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    // Compute the CMA of the specified metric over
    // the specified window.
    compCMA(pTrk, pArgs);

    if (pArgs->actMetric == elevation) {
        // Recompute the metrics
        compMetrics(pTrk, pArgs);
    }

    // Recompute min/avg/max values
    computeMinMaxValues(pTrk);

    return OK;
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

static CmdStat cliCmdHistory(GpsTrk *pTrk, CmdArgs *pArgs)
{
    HIST_ENTRY **histList = history_list();

    if (histList != NULL) {
        for (int n = 0; histList[n] != NULL; n++) {
            HIST_ENTRY *histEntry = histList[n];
            if (histEntry != NULL) {
                printf("%s\n", histEntry->line);
            }
        }
    }

    return OK;
}

static CmdStat cliCmdMax(GpsTrk *pTrk, CmdArgs *pArgs)
{
    char *metric = pArgs->argv[1];
    char *value = pArgs->argv[2];
    double maxVal = 0.0;

    if (pArgs->argc < 3) {
        printf("Syntax: max <metric> <value> [<from> <to>]\n");
        return ERROR;
    }

    if ((pArgs->actMetric = getActMetric(metric)) == invalid) {
        return invArgMsg(metric, NULL);
    }

    if (sscanf(value, "%le", &maxVal) != 1) {
        return invArgMsg(value, NULL);
    }

    if (pArgs->argc == 5) {
        if (getTrkPtRange(pTrk, pArgs->argv[3], pArgs->argv[4], &pArgs->range) < 0) {
            return -1;
        }
    } else {
        pArgs->range.from = 0;
        pArgs->range.to = pTrk->numTrkPts - 1;
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    {
        TrkPt *tp;

        TAILQ_FOREACH(tp, &pTrk->trkPtList, tqEntry) {
            int index = tp->index;

            if ((index >= pArgs->range.from) && (index <= pArgs->range.to)) {
                if ((pArgs->actMetric == elevation) && (tp->elevation > maxVal)) {
                    tp->elevation = maxVal;
                } else if ((pArgs->actMetric == grade) && (tp->grade > maxVal)) {
                    tp->grade = maxVal;
                } else if ((pArgs->actMetric == speed) && (tp->speed > maxVal)) {
                    tp->speed = maxVal;
                } else if (pArgs->actMetric == gradeChange) {
                    TrkPt *p0 = TAILQ_PREV(tp, TrkPtList, tqEntry);
                    if (p0 != NULL) {
                        if ((tp->deltaG = fabs(tp->grade - p0->grade)) > maxVal) {
                            printf("TrkPt #%d: grade=%.3lf->%.3lf change=%.3lf exceeds the maxVal=%.3lf\n",
                                    tp->index, p0->grade, tp->grade, tp->deltaG, maxVal);
                            // TBD
                        }
                    }
                }
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

    if (pArgs->argc < 3) {
        printf("Syntax: min <metric> <value> [<from> <to>]\n");
        return ERROR;
    }

    if ((pArgs->actMetric = getActMetric(metric)) == invalid) {
        return invArgMsg(metric, NULL);
    }

    if (sscanf(value, "%le", &minVal) != 1) {
        return invArgMsg(value, NULL);
    }

    if (pArgs->argc == 5) {
        if (getTrkPtRange(pTrk, pArgs->argv[3], pArgs->argv[4], &pArgs->range) < 0) {
            return -1;
        }
    } else {
        pArgs->range.from = 0;
        pArgs->range.to = pTrk->numTrkPts - 1;
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    {
        TrkPt *tp;

        TAILQ_FOREACH(tp, &pTrk->trkPtList, tqEntry) {
            int index = tp->index;

            if ((index >= pArgs->range.from) && (index <= pArgs->range.to)) {
                if ((pArgs->actMetric == elevation) && (tp->elevation < minVal)) {
                    tp->elevation = minVal;
                } else if ((pArgs->actMetric == grade) && (tp->grade < minVal)) {
                    tp->grade = minVal;
                } else if ((pArgs->actMetric == speed) && (tp->speed < minVal)) {
                    tp->speed = minVal;
                }
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

static CmdStat cliCmdScale(GpsTrk *pTrk, CmdArgs *pArgs)
{
    char *smaMetric = pArgs->argv[1];
    char *scaleFactor = pArgs->argv[2];

    if (pArgs->argc < 3) {
        printf("Syntax: scale <metric> <factor> [<from> <to>]\n");
        return ERROR;
    }

    if ((pArgs->actMetric = getActMetric(smaMetric)) == invalid) {
        return invArgMsg(smaMetric, NULL);
    }

    if (sscanf(scaleFactor, "%le", &pArgs->scaleFactor) != 1) {
        return invArgMsg(scaleFactor, NULL);
    }

    if (pArgs->argc == 5) {
        if (getTrkPtRange(pTrk, pArgs->argv[3], pArgs->argv[4], &pArgs->range) < 0) {
            return -1;
        }
    } else {
        pArgs->range.from = 0;
        pArgs->range.to = pTrk->numTrkPts - 1;
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    // Scale the specified metric by the specified factor
    scaleMetric(pTrk, pArgs);

    if (pArgs->actMetric == elevation) {
        // Recompute the metrics
        compMetrics(pTrk, pArgs);
    }

    // Recompute min/avg/max values
    computeMinMaxValues(pTrk);

    return OK;
}

static CmdStat cliCmdSgf(GpsTrk *pTrk, CmdArgs *pArgs)
{
    char *smaMetric = pArgs->argv[1];
    char *smaWindow = pArgs->argv[2];

    if (pArgs->argc < 3) {
        printf("Syntax: sgf <metric> <window> [<from> <to>]\n");
        return ERROR;
    }

    if ((pArgs->actMetric = getActMetric(smaMetric)) == invalid) {
        return invArgMsg(smaMetric, NULL);
    }

    if ((sscanf(smaWindow, "%d", &pArgs->smaWindow) != 1) ||
        ((pArgs->smaWindow % 2) == 0)) {
        return invArgMsg(smaWindow, NULL);
    }

    if (pArgs->argc == 5) {
        if (getTrkPtRange(pTrk, pArgs->argv[3], pArgs->argv[4], &pArgs->range) < 0) {
            return -1;
        }
    } else {
        pArgs->range.from = 0;
        pArgs->range.to = pTrk->numTrkPts - 1;
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    // Compute the SGF of the specified metric over
    // the specified window.
    compSGF(pTrk, pArgs);

    if (pArgs->actMetric == elevation) {
        // Recompute the metrics
        compMetrics(pTrk, pArgs);
    }

    // Recompute min/avg/max values
    computeMinMaxValues(pTrk);

    return OK;
}

static CmdStat cliCmdShow(GpsTrk *pTrk, CmdArgs *pArgs)
{
    TrkPt *tp;

    if (pArgs->argc == 3) {
        if (getTrkPtRange(pTrk, pArgs->argv[1], pArgs->argv[2], &pArgs->range) < 0) {
            return -1;
        }
    } else {
        pArgs->range.from = 0;
        pArgs->range.to = pTrk->numTrkPts - 1;
    }

    TAILQ_FOREACH(tp, &pTrk->trkPtList, tqEntry) {
        int index = tp->index;

        if ((index >= pArgs->range.from) && (index <= pArgs->range.to)) {
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

    if (pArgs->argc < 3) {
        printf("Syntax: sma <metric> <window> [<from> <to>]\n");
        return ERROR;
    }

    if ((pArgs->actMetric = getActMetric(smaMetric)) == invalid) {
        return invArgMsg(smaMetric, NULL);
    }

    if ((sscanf(smaWindow, "%d", &pArgs->smaWindow) != 1) ||
        (pArgs->smaWindow < 3)) {
        return invArgMsg(smaWindow, NULL);
    }

    if (pArgs->argc == 5) {
        if (getTrkPtRange(pTrk, pArgs->argv[3], pArgs->argv[4], &pArgs->range) < 0) {
            return -1;
        }
    } else {
        pArgs->range.from = 0;
        pArgs->range.to = pTrk->numTrkPts - 1;
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    // Compute the SMA of the specified metric over
    // the specified window.
    compSMA(pTrk, pArgs);

    if (pArgs->actMetric == elevation) {
        // Recompute the metrics
        compMetrics(pTrk, pArgs);
    }

    // Recompute min/avg/max values
    computeMinMaxValues(pTrk);

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
    if ((pArgs->argc != 3) ||
        (getTrkPtRange(pTrk, pArgs->argv[1], pArgs->argv[2], &pArgs->range) < 0)) {
        printf("Syntax: trim <range>\n");
        return ERROR;
    }

    // Save current TrkPt's so that this operation
    // can be 'undo'
    saveTrkPts(pTrk);

    {
        TrkPt *p = TAILQ_FIRST(&pTrk->trkPtList);
        Bool discTrkPt = false;
        Bool trimTrkPts = false;
        double baselineTime = 0.0;
        double baselineDist = 0.0;
        double trimmedTime = 0.0;
        double trimmedDist = 0.0;
        int index = 0;

        // Remove all TrkPt's in the <from>-<to> range and
        // close the distance and time gaps.
        while (p != NULL) {
            discTrkPt = false;

            // Do we need to trim out this TrkPt?
            if (p->index == pArgs->range.from) {
                // Start trimming
                trimTrkPts = true;
                pTrk->numTrimTrkPts++;
                discTrkPt = true;
                baselineTime = p->timestamp;    // set baseline timestamp
                baselineDist = p->distance;     // set baseline distance
            } else if (p->index == pArgs->range.to) {
                // Stop trimming
                trimTrkPts = false;
                trimmedTime = p->timestamp - baselineTime + 1;  // total time trimmed out
                trimmedDist = p->distance - baselineDist + 1;   // total distance trimmed out
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
                // of this TrkPt so as to "close the gaps".  Protect
                // against silly negative values!
                if ((p->timestamp -= trimmedTime) < 0.0)
                    p->timestamp = 0.0;
                if ((p->distance -= trimmedDist) < 0.0)
                    p->distance = 0.0;
                p = nxtTrkPt(NULL, p);
            }
        }

        // Recompute the index of all the TrkPt's
        TAILQ_FOREACH(p, &pTrk->trkPtList, tqEntry) {
            p->index = index++;
        }

        // Update the total number of TrkPt's
        pTrk->numTrkPts = index;

        if ((p = TAILQ_FIRST(&pTrk->trkPtList)) != NULL) {
            // Adjust the start values
            p->distance = 0.0;
            p->grade = 0.0;
            pTrk->startTime = p->timestamp;
        }

        if ((p = TAILQ_LAST(&pTrk->trkPtList, TrkPtList)) != NULL) {
            // Adjust the end values
            pTrk->distance = p->distance;
            pTrk->endTime = p->timestamp;
        }
    }

    // Recompute min/avg/max values
    computeMinMaxValues(pTrk);

    return OK;
}

static CmdStat cliCmdUndo(GpsTrk *pTrk, CmdArgs *pArgs)
{
    if (TAILQ_FIRST(&pTrk->savedTrkPtList) != NULL) {
        restoreTrkPts(pTrk);
    } else {
        printf("Nothing to undo!\n");
    }

    // Update the start/end times
    pTrk->startTime = TAILQ_FIRST(&pTrk->trkPtList)->timestamp;
    pTrk->endTime = TAILQ_LAST(&pTrk->trkPtList, TrkPtList)->timestamp;

    // Recompute min/avg/max values
    computeMinMaxValues(pTrk);

    return OK;
}

// CLI command table
static CliCmd cliCmdTbl [] = {
        { "cma",        cliCmdCma },
        { "exit",       cliCmdExit },
        { "help",       cliCmdHelp },
        { "history",    cliCmdHistory },
        { "max",        cliCmdMax },
        { "min",        cliCmdMin },
        { "save",       cliCmdSave },
        { "scale",      cliCmdScale },
        { "sgf",        cliCmdSgf },
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
