#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defs.h"

#define PROGRAM_VERSION "1.0"

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
        fprintf(stderr, "Invalid syntax.  Use 'gpxFileTool --help' for more information.\n");
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
        } else if (strcmp(arg, "--quiet") == 0) {
            pArgs->quiet = true;
        } else if (strcmp(arg, "--verbatim") == 0) {
            pArgs->verbatim = true;
        } else if (strcmp(arg, "--version") == 0) {
            fprintf(stdout, "Program version %s built on %s %s\n", PROGRAM_VERSION, __DATE__, __TIME__);
            exit(0);
        } else {
            fprintf(stderr, "Invalid option: %s\nUse --help for the list of supported options.\n", arg);
            return -1;
        }
    }

    return n;
}

int main(int argc, char **argv)
{
    CmdArgs cmdArgs = {0};
    int n;

    // Parse the command arguments
    if ((n = parseArgs(argc, argv, &cmdArgs)) < 0) {
        return -1;
    }

    return 0;
}
