# mkshiz

## Intro

The **FulGaz** virtual cycling app uses a proprietary SHIZ file to control the resistance of the smart trainer and the playback speed of the video during a virtual ride.  The SHIZ file is a plain text file that uses the JSON enconding, and includes an array of "trackpoints", where each trackpoint indicates the time, distance, speed, grade, bearing, etc. at that point in the ride.
 
**mkshiz** is an interactive command-line tool for generating a SHIZ file from a FIT file created by a bike computer during a real outdoor activity, that allow users to create their own personal virtual routes to ride using the **FulGaz** app.

## Building the tool

To build the **mkshiz** binary all you need to do is run 'make' at the top-level directory. The tool is known to build warning and error free under Ubuntu, macOS, and Cygwin. As it is written entirely in C and only uses the standard math library and the GNU readline library, it should be easy to port to other platforms.

```
$ make
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -D__CYGWIN__ -o cli.o -c cli.c
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -D__CYGWIN__ -o comp.o -c comp.c
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -D__CYGWIN__ -o const.o -c const.c
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -D__CYGWIN__ -o input.o -c input.c
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -D__CYGWIN__ -o main.o -c main.c
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -D__CYGWIN__ -o output.o -c output.c
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -D__CYGWIN__ -o sgfilter.o -c sgfilter.c
cc -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0 -D__CYGWIN__ -o trkpt.o -c trkpt.c
cc -ggdb  -o ./mkshiz ./cli.o ./comp.o ./const.o ./input.o ./main.o ./output.o ./sgfilter.o ./trkpt.o -lm -lreadline
```

## Usage

The following examples show how to use the tool.  Running the tool with the option --help will show a "manual page" describing all the options: 

```
$ mkshiz.exe --help
SYNTAX:
    mkshiz [OPTIONS] <file> [<file2> ...]

    When multiple input files are specified, the tool will attempt to
    stitch them together into a single file.

OPTIONS:
    --csv-time-format {hms|sec|utc}
        Specifies the format of the timestamp value in the CSV output.
        'hms' and 'sec' imply relative timestamps, while 'utc' implies
        absolute timestamps.
    --csv-units {imperial|metric}
        Specifies the type of units to use in the CSV output.
    --help
        Show this help and exit.
    --no-cli
        Do not start the interactive CLI.
    --quiet
        Suppress all warning messages.
    --version
        Show version information and exit.
NOTES:
    Running the tool under Windows/Cygwin the drive letters are replaced by
    their equivalent cygdrive: e.g. the path "C:\Users\Marcelo\Documents"
    becomes "/cygdrive/c/Users/Marcelo/Documents".

BUGS:
    Report bugs and enhancement requests to: marcelo_mourier@yahoo.com
```

While the tool can process a FIT file directly, the most useful features are available through its interactive CLI:

```
CLI> help
Supported CLI commands:

cma <metric> <window>  Smooth the specified metric using a CMA filter.
exit                   Exit the tool
help                   Print this help
history                Print the command history.
max <metric> <value>   Limit the maximum value of the specified metric.
min <metric> <value>   Limit the minimum value of the specified metric.
save <file> [<format>] Save the data in the specified format and file.
                       The output format can be: csv, gpx, shiz, tcx.
sgf <metric> <window>  Smooth the specified metric using the Savitzky-Golay
                       filter.
show [<a> <b>]         Show trackpoints in plain text form.
sma <metric> <window>  Smooth the specified metric using an SMA filter.
summary [detail]       Print a summary of the data.
trim <a> <b>           Remove the trackpoints between points <a> and <b>
                       (inclusive) and close the distance and time gaps
                       between them. The trackpoints can be specified by
                       their indices or by their timestamps.
undo                   Revert the last operation.

The metric can be: elevation, grade, speed.
```
 
## A note about running mkshiz under Windows/Cygwin

When running the tool under Windows/Cygwin notice that the drive letters used in path names, such as C: and D:, are replaced by the corresponding "cygdrive".  For example, the path "C:\Users\Marcelo\Documents" becomes "/cygdrive/c/Users/Marcelo/Documents", and the path "D:\FulGaz\Videos" becomes "/cygdrive/d/FulGaz/Videos".

## Credits ##

The code used to process the input FIT file is based on Garmin's [FitSDK](https://developer.garmin.com/fit/download/).

The code for computing the Savitzky-Golay filter is based on Fredrik Jonsson's [sgfilter](http://jonsson.eu/programs/cweb/sgfilter/). 

