#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cli.h"
#include "const.h"
#include "defs.h"
#include "input.h"
#include "output.h"
#include "trkpt.h"

static const char *help =
        "SYNTAX:\n"
        "    mkshiz [OPTIONS] <file> [<file2> ...]\n"
        "\n"
        "    When multiple input files are specified, the tool will attempt to\n"
        "    stitch them together into a single file.\n"
        "\n"
        "OPTIONS:\n"
        "    --csv-time-format {hms|sec|utc}\n"
        "        Specifies the format of the timestamp value in the CSV output.\n"
        "        'hms' and 'sec' imply relative timestamps, while 'utc' implies\n"
        "        absolute timestamps.\n"
        "    --csv-units {imperial|metric}\n"
        "        Specifies the type of units to use in the CSV output.\n"
        "    --help\n"
        "        Show this help and exit.\n"
        "    --quiet\n"
        "        Suppress all warning messages.\n"
        "    --version\n"
        "        Show version information and exit.\n";

static void invalidArgument(const char *arg, const char *val)
{
    fprintf(stderr, "Invalid argument: %s %s\n", arg, (val != NULL) ? val : "");
}

static int parseArgs(int argc, char **argv, CmdArgs *pArgs)
{
    int numArgs, n;

    if (argc < 2) {
        fprintf(stderr, "Invalid syntax.  Use 'mkshiz --help' for more information.\n");
        return -1;
    }

    // By default display metric units
    pArgs->units = metric;

    for (n = 1, numArgs = argc -1; n <= numArgs; n++) {
        const char *arg;
        const char *val;

        arg = argv[n];

        if (strcmp(arg, "--help") == 0) {
            fprintf(stdout, "%s\n", help);
            exit(0);
        } else if (strcmp(arg, "--csv-time-format") == 0) {
            val = argv[++n];
            if (strcmp(val, "hms") == 0) {
                pArgs->tsFmt = hms;
            } else if (strcmp(val, "sec") == 0) {
                pArgs->tsFmt = sec;
            } else if (strcmp(val, "utc") == 0) {
                pArgs->tsFmt = utc;
            } else {
                invalidArgument(arg, val);
                return -1;
            }
        } else if (strcmp(arg, "--csv-units") == 0) {
            val = argv[++n];
            if (strcmp(val, "imperial") == 0) {
                pArgs->units = imperial;
            } else if (strcmp(val, "metric") == 0) {
                pArgs->units = metric;
            } else {
                invalidArgument(arg, val);
                return -1;
            }
        } else if (strcmp(arg, "--output-format") == 0) {
            val = argv[++n];
            if (strcmp(val, "csv") == 0) {
                pArgs->outFmt = csv;
            } else if (strcmp(val, "gpx") == 0) {
                pArgs->outFmt = gpx;
            } else if (strcmp(val, "shiz") == 0) {
                pArgs->outFmt = shiz;
            } else if (strcmp(val, "tcx") == 0) {
                pArgs->outFmt = tcx;
            } else {
                invalidArgument(arg, val);
                return -1;
            }
        } else if (strcmp(arg, "--quiet") == 0) {
            pArgs->quiet = true;
        } else if (strcmp(arg, "--verbatim") == 0) {
            pArgs->verbatim = true;
        } else if (strcmp(arg, "--version") == 0) {
            fprintf(stdout, "Program version %d.%d built on %s %s\n", PROG_VER_MAJOR, PROG_VER_MINOR, __DATE__, __TIME__);
            exit(0);
        } else if (strncmp(arg, "--", 2) == 0) {
            fprintf(stderr, "Invalid option: %s\nUse --help for the list of supported options.\n", arg);
            return -1;
        } else {
            // Assume it's the input file(s)
            break;
        }
    }

    pArgs->outFile = stdout;

    return n;
}

int main(int argc, char **argv)
{
    CmdArgs cmdArgs = {0};
    GpsTrk gpsTrk = {0};
    TrkPt *pTrkPt;
    int n;

    // Parse the command arguments
    if ((n = parseArgs(argc, argv, &cmdArgs)) < 0) {
        return -1;
    }

    TAILQ_INIT(&gpsTrk.trkPtList);

    // Process each FIT input file
    while (n < argc) {
        const char *fileSuffix;
        int s;
        cmdArgs.inFile = argv[n++];
        if ((fileSuffix = strrchr(cmdArgs.inFile, '.')) == NULL) {
            fprintf(stderr, "Unsupported input file %s\n", cmdArgs.inFile);
            return -1;
        }
        if (strcmp(fileSuffix, ".fit") == 0) {
            s = parseFitFile(&cmdArgs, &gpsTrk, cmdArgs.inFile);
        } else {
            fprintf(stderr, "Unsupported input file %s\n", cmdArgs.inFile);
            return -1;
        }
        if (s != 0) {
            fprintf(stderr, "Failed to parse input file %s\n", cmdArgs.inFile);
            return -1;
        }
        cmdArgs.inFile = NULL;
    }

    // Done parsing all the input files. Make sure we have
    // at least one TrkPt!
    if ((pTrkPt = TAILQ_FIRST(&gpsTrk.trkPtList)) == NULL) {
        // Hu?
        fprintf(stderr, "ERROR: No track points found!\n");
        return -1;
    }

    // Make sure the FIT includes timing data
    if (pTrkPt->timestamp == 0.0) {
        fprintf(stderr, "ERROR: FIT is missing timing data!\n");
        return -1;
    }

    // Make sure the FIT includes speed data
    if (pTrkPt->speed == nilSpeed) {
        fprintf(stderr, "WARNING: FIT is missing speed data...\n");
    }

    // Process the CLI commands
    cliCmdHandler(&gpsTrk, &cmdArgs);

    return 0;
}
