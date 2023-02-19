#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "comp.h"
#include "const.h"
#include "trkpt.h"

// Check the TrkPt's for duplicates and bogus values
int checkTrkPts(GpsTrk *pTrk, const CmdArgs *pArgs)
{
    TrkPt *p1 = TAILQ_FIRST(&pTrk->trkPtList);  // previous TrkPt
    TrkPt *p2 = TAILQ_NEXT(p1, tqEntry);    // current TrkPt
    Bool discTrkPt = false;

    while (p2 != NULL) {
        // Discard any duplicate points...
        discTrkPt = false;

        // Without elevation data, there isn't much we can do!
        if (p2->elevation == nilElev) {
            fprintf(stderr, "ERROR: TrkPt #%d (%s) is missing its elevation data !\n", p2->index, fmtTrkPtIdx(p2));
            return -1;
        }

        // Some GPX tracks may have duplicate TrkPt's. This
        // can happen when the file has multiple laps, and
        // the last point in lap N is the same as the first
        // point in lap N+1.
        if ((p2->latitude == p1->latitude) &&
            (p2->longitude == p1->longitude) &&
            (p2->elevation == p1->elevation)) {
            if (!pArgs->quiet) {
                fprintf(stderr, "INFO: Discarding duplicate TrkPt #%d (%s) !\n", p2->index, fmtTrkPtIdx(p2));
            }
            pTrk->numDupTrkPts++;
            discTrkPt = true;
        }

        // Timestamps should increase monotonically
        if (p2->timestamp <= p1->timestamp) {
            if (!pArgs->quiet) {
                fprintf(stderr, "INFO: TrkPt #%d (%s) has a non-increasing timestamp value: %.3lf !\n",
                        p2->index, fmtTrkPtIdx(p2), p2->timestamp);
            }

            // Discard as a dummy
            pTrk->numDiscTrkPts++;
            discTrkPt = true;
        }

        // Distance should increase monotonically
        if ((p2->distance != 0) && (p2->distance <= p1->distance)) {
            if (!pArgs->quiet) {
                fprintf(stderr, "INFO: TrkPt #%d (%s) has a non-increasing distance value: %.3lf !\n",
                        p2->index, fmtTrkPtIdx(p2), p2->distance);
            }

            // Discard as a dummy
            pTrk->numDiscTrkPts++;
            discTrkPt = true;
        }

        // Discard?
        if (discTrkPt) {
            // Remove this TrkPt from the list keeping
            // p1 the same.
            p2 = remTrkPt(pTrk, p2);
        } else {
            // Advance to the next TrkPt and update p1
            p2 = nxtTrkPt(&p1, p2);
        }
    }

    return 0;
}

// Compute the great-circle distance (in meters) between two
// track points using the Haversine formula. See below for
// the details:
//
//   https://en.wikipedia.org/wiki/Haversine_formula
//
static double compHaversine(const TrkPt *p1, const TrkPt *p2)
{
    const double two = (double) 2.0;
    double phi1 = p1->latitude * degToRad;  // p1's latitude in radians
    double phi2 = p2->latitude * degToRad;  // p2's latitude in radians
    double deltaPhi = (phi2 - phi1);        // latitude diff in radians
    double deltaLambda = (p2->longitude - p1->longitude) * degToRad;   // longitude diff in radians
    double a = sin(deltaPhi / two);
    double b = sin(deltaLambda / two);
    double h = (a * a) + cos(phi1) * cos(phi2) * (b * b);

    assert(h >= 0.0);

    return (two * earthMeanRadius * asin(sqrt(h)));
}

// Compute the bearing (in decimal degrees) between two track
// points.  See below for the details:
//
//   https://www.movable-type.co.uk/scripts/latlong.html
//
static double compBearing(const TrkPt *p1, const TrkPt *p2)
{
    double phi1 = p1->latitude * degToRad;  // p1's latitude in radians
    double phi2 = p2->latitude * degToRad;  // p2's latitude in radians
    double deltaLambda = (p2->longitude - p1->longitude) * degToRad;   // longitude diff in radians
    double x = sin(deltaLambda) * cos(phi2);
    double y = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(deltaLambda);
    double theta = atan2(x, y);  // in radians

    return fmod((theta / degToRad + 360.0), 360.0); // in degrees decimal (0-359.99)
}

int computeMinMaxValues(GpsTrk *pTrk)
{
    TrkPt *p1 = TAILQ_FIRST(&pTrk->trkPtList);  // previous TrkPt
    TrkPt *p2 = TAILQ_NEXT(p1, tqEntry);    // current TrkPt

    // Initialize the min/max values
    pTrk->minSpeed = +999.9;
    pTrk->maxSpeed = -999.9;
    pTrk->minElev = +99999.9;
    pTrk->maxElev = -99999.9;
    pTrk->minGrade = +99.9;
    pTrk->maxGrade = -99.9;

    // Reset max delta values
    pTrk->maxDeltaD = 0.0;
    pTrk->maxDeltaDTrkPt = NULL;
    pTrk->maxDeltaG = 0.0;
    pTrk->maxDeltaGTrkPt = NULL;
    pTrk->maxDeltaT = 0.0;
    pTrk->maxDeltaTTrkPt = NULL;

    // Reset rolling sum values
    pTrk->elevGain = 0.0;
    pTrk->elevLoss = 0.0;
    pTrk->grade = 0.0;

    while (p2 != NULL) {
        if (p2->speed > pTrk->maxSpeed) {
             pTrk->maxSpeed = p2->speed;
             pTrk->maxSpeedTrkPt = p2;
        } else if ((p2->speed != 0) && (p2->speed < pTrk->minSpeed)) {
            pTrk->minSpeed = p2->speed;
            pTrk->minSpeedTrkPt = p2;
        }

        if (p2->elevation > pTrk->maxElev) {
             pTrk->maxElev = p2->elevation;
             pTrk->maxElevTrkPt = p2;
        } else if (p2->elevation < pTrk->minElev) {
            pTrk->minElev = p2->elevation;
            pTrk->minElevTrkPt = p2;
        }

        if (p2->grade > pTrk->maxGrade) {
             pTrk->maxGrade = p2->grade;
             pTrk->maxGradeTrkPt = p2;
        } else if (p2->grade < pTrk->minGrade) {
            pTrk->minGrade = p2->grade;
            pTrk->minGradeTrkPt = p2;
        }

        // Update the max dist value
        if (p2->dist > pTrk->maxDeltaD) {
            pTrk->maxDeltaD = p2->dist;
            pTrk->maxDeltaDTrkPt = p2;
        }

        // Update the max absolute grade change
        if (p2->deltaG > pTrk->maxDeltaG) {
            pTrk->maxDeltaG = p2->deltaG;
            pTrk->maxDeltaGTrkPt = p2;
        }

        // Update the max time interval
        if (p2->deltaT > pTrk->maxDeltaT) {
            pTrk->maxDeltaT = p2->deltaT;
            pTrk->maxDeltaTTrkPt = p2;
        }

        // Update the rolling values of the elevation gain
        // and grade, to compute the averages for the
        // activity.
        if (p2->rise >= 0.0) {
            pTrk->elevGain += p2->rise;
        } else {
            pTrk->elevLoss += fabs(p2->rise);
        }
        pTrk->grade += p2->grade;

        p2 = nxtTrkPt(&p1, p2);
    }

    return 0;
}

/* Consecutive points P1 and P2 in the track define a pseudo-triangle,
 * where the base is the horizontal distance "run", the height is the
 * vertical distance "rise", and the hypotenuse is the actual distance
 * traveled between the two points.
 *
 * The figure is not an exact triangle, because the "run" is not a straight
 * line, but rather the great-circle distance over the Earth's surface.
 * But when the two points are "close together", as during a slow speed
 * activity like cycling (when the sample points are spaced apart by just
 * 1 second) we can assume the run is a straight line, and hence we are
 * dealing with a rectangular triangle.
 *
 *                                 + P2
 *                                /|
 *                               / |
 *                         dist /  | rise
 *                             /   |
 *                            /    |
 *                        P1 +-----+
 *                             run
 *
 *   Assuming the angle at P1, between "dist" and "run", is "theta", then
 *   the following equations describe the relationship between the various
 *   values:
 *
 *   slope = rise / run = tan(theta)
 *
 *   dist^2 = run^2 + rise^2
 *
 *   dist = speed * (t2 - t1)
 *
 *   The "rise" is simply the elevation gain between the two points. In a
 *   consumer-level GPS device, the error in the elevation value can be 3X
 *   the error in the latitude/longitude values. And because the "rise" is
 *   used to compute the slope, the calculated slope value is also subject
 *   to error.
 *
 *   Having an actual speed sensor on the bike during a cycling activity is
 *   helpful because that way the "dist" value can be easily an accurately
 *   computed from the speed value and the time difference, which is typically
 *   fixed at 1 second. If no speed sensor is available, the "dist" value needs
 *   to be computed from the "run" and "rise" values, using Pythagoras's
 *   Theorem, where the "run" value is computed from the latitude/longitude
 *   using the Haversine formula.
 *
 *   References:
 *
 *   https://eos-gnss.com/knowledge-base/articles/elevation-for-beginners
 *   https://en.wikipedia.org/wiki/Haversine_formula
 *
 */

int compMetrics(GpsTrk *pTrk, const CmdArgs *pArgs)
{
    TrkPt *p1 = TAILQ_FIRST(&pTrk->trkPtList);  // previous TrkPt
    TrkPt *p2 = TAILQ_NEXT(p1, tqEntry);    // current TrkPt

    // At this point p1 points to the first trackpoint in the
    // track, which is used as the baseline...
    p1->grade = 0.0;

    // Set the activity's start time
    pTrk->startTime = p1->timestamp;

    // Compute the distance, elevation diff, speed, and grade
    // between each pair of points...
    while (p2 != NULL) {
        double absRise; // always positive!

        // Compute the elevation difference (can be negative)
        p2->rise = p2->elevation - p1->elevation;

        // The "rise" is always positive!
        absRise = fabs(p2->rise);

        if (p2->distance != 0.0) {
            // The TrkPt includes the "distance from start" value,
            // so the incremental distance "dist" between TrkPt's
            // can be computed by simple subtraction.
            if ((p2->dist = p2->distance - p1->distance) == 0.0) {
                // Stopped?
                if (!pArgs->verbatim) {
                    if (!pArgs->quiet) {
                        fprintf(stderr, "WARNING: TrkPt #%d (%s) has a zero dist value !\n",
                                p2->index, fmtTrkPtIdx(p2));
                        printTrkPt(p2);
                    }

                    // Skip and delete this TrkPt
                    p2 = remTrkPt(pTrk, p2);
                    pTrk->numDiscTrkPts++;
                } else {
                    // Carry over the data from the previous point
                    p2->bearing = p1->bearing;
                    p2->distance = p1->distance;
                    p2->grade = p1->grade;
                    p2->speed = p1->speed;

                    // Move on to the next point
                    p2 = nxtTrkPt(&p1, p2);
                }
                continue;
            }

            if (p2->dist > absRise) {
                // Compute the horizontal distance "run" using
                // Pythagoras's Theorem.
                p2->run = sqrt((p2->dist * p2->dist) - (absRise * absRise));
            } else {
                // Bogus data?
                if (!pArgs->quiet) {
                    fprintf(stderr, "WARNING: TrkPt #%d (%s) has inconsistent dist=%.3lf and rise=%.3lf values !\n",
                            p2->index, fmtTrkPtIdx(p2), p2->dist, absRise);
                    printTrkPt(p2);
                }
                p2->run = p2->dist; // assume a null grade
            }
        } else {
            // The TrkPt doesn't include the "distance from start" value,
            // so we need to compute it ourselves...

            // Compute the horizontal distance "run" between
            // the two points, based on their latitude and
            // longitude values.
            if ((p2->run = compHaversine(p1, p2)) == 0.0) {
                // Stopped?
                if (!pArgs->verbatim) {
                    if (!pArgs->quiet) {
                        fprintf(stderr, "WARNING: TrkPt #%d (%s) has a null run value !\n",
                                p2->index, fmtTrkPtIdx(p2));
                        printTrkPt(p2);
                    }

                    // Skip and delete this TrkPt
                    p2 = remTrkPt(pTrk, p2);
                    pTrk->numDiscTrkPts++;
                } else {
                    // Carry over the data from the previous point
                    p2->bearing = p1->bearing;
                    p2->distance = p1->distance;
                    p2->grade = p1->grade;
                    p2->speed = p1->speed;

                    // Move on to the next point
                    p2 = nxtTrkPt(&p1, p2);
                }
                continue;
            }

            // Compute the incremental distance "dist" between
            // the two points.
            if (absRise == 0.0) {
                // When riding on the flats, dist equals run!
                p2->dist = p2->run;
            } else {
                // Use Pythagoras's Theorem to compute the
                // distance (hypotenuse)
                p2->dist = sqrt((p2->run * p2->run) + (absRise * absRise));
            }

            // Compute the "distance from start" value
            p2->distance = p1->distance + p2->dist;
        }

        // Paranoia?
        if (p2->distance < p1->distance) {
            fprintf(stderr, "SPONG! TrkPt #%u (%s) has a non-increasing distance !\n",
                    p2->index, fmtTrkPtIdx(p2));
            fprintf(stderr, "dist=%.10lf run=%.10lf absRise=%.10lf\n", p2->dist, p2->run, absRise);
            dumpTrkPts(pTrk, p2, 2, 0);
        }

        // Compute the time interval between the two points.
        // Typically fixed at 1-sec, but some GPS devices (e.g.
        // Garmin Edge) may use a "smart" recording mode that
        // can have several seconds between points, while
        // other devices (e.g. GoPro Hero) may record multiple
        // points each second.
        p2->deltaT = (p2->timestamp - p1->timestamp);

        // Paranoia?
        if (p2->deltaT <= 0.0) {
            fprintf(stderr, "SPONG! TrkPt #%u (%s) has a non-increasing timestamp ! dist=%.10lf deltaT=%.3lf\n",
                    p2->index, fmtTrkPtIdx(p2), p2->dist, p2->deltaT);
            dumpTrkPts(pTrk, p2, 2, 0);
        }

        if ((p2->speed == nilSpeed) || (p2->speed == 0.0)) {
            // Compute the speed (in m/s) as "distance over time"
            p2->speed = p2->dist / p2->deltaT;
            if (p2->speed > 27.78) {
                // 100 kph ???
                fprintf(stderr, "SPONG! TrkPt #%u (%s) has a bogus speed value ! dist=%.10lf deltaT=%.3lf speed=%.3lf\n",
                         p2->index, fmtTrkPtIdx(p2), p2->dist, p2->deltaT, p2->speed);
            }
        }

        // Update the total time for the activity
        pTrk->time += p2->deltaT;

        // Compute the grade as "rise over run". Notice
        // that the grade value may get updated later.
        // Guard against points with run=0, which can
        // happen when using the "--verbose" option...
        if (p2->run != 0.0) {
            p2->grade = (p2->rise * 100.0) / p2->run;   // in [%]
        } else {
            if (!pArgs->quiet) {
                fprintf(stderr, "WARNING: TrkPt #%d (%s) has a null run value !\n",
                        p2->index, fmtTrkPtIdx(p2));
            }
            p2->grade = p1->grade;  // carry over the previous grade value
        }

        // Sanity check the grade value
        if ((p2->grade < -99.9) || (p2->grade > 99.9)) {
            if (!pArgs->quiet) {
                fprintf(stderr, "WARNING: TrkPt #%d (%s) has a bogus grade value !\n",
                        p2->index, fmtTrkPtIdx(p2));
                printTrkPt(p2);
            }
            p2->grade = (p1->grade != nilGrade) ? p1->grade : 0.0;
        }

        // Compute the bearing
        p2->bearing = compBearing(p1, p2);

        // Compute the absolute grade change
        p2->deltaG = fabs(p2->grade - p1->grade);

        // Update the total distance for the activity
        pTrk->distance = p2->distance;

        // Update the activity's end time
        pTrk->endTime = p2->timestamp;

        p2 = nxtTrkPt(&p1, p2);
    }

    // Compute the activity's min/max values
    computeMinMaxValues(pTrk);

    return 0;
}

static double smaGetVal(const TrkPt *p, SmaMetric smaMetric)
{
    if (smaMetric == elevation) {
        return (double) p->elevation;
    } else if (smaMetric == grade) {
        return (double) p->grade;
    } else {
        return p->speed;
    }
}

static void smaSetVal(TrkPt *p, SmaMetric smaMetric, double value)
{
    if (smaMetric == elevation) {
        p->elevation = value;
    } else if (smaMetric == grade) {
        p->grade = value;
    } else {
        p->speed = value;
    }
}

// Compute the Simple Moving Average of the specified metric
int compSMA(GpsTrk *pTrk, const CmdArgs *pArgs)
{
    TrkPt *p;

    TAILQ_FOREACH(p, &pTrk->trkPtList, tqEntry) {
        if (p->index >= pArgs->smaWindow) {
            TrkPt *tp = p;
            double sum = 0.0;
            for (int n = 0; n < pArgs->smaWindow; n++) {
                sum += smaGetVal(tp, pArgs->smaMetric);
                tp = TAILQ_PREV(tp, TrkPtList, tqEntry);
            }
            smaSetVal(p, pArgs->smaMetric, (sum / pArgs->smaWindow));
        }
    }

    return 0;
}

int saveTrkPts(GpsTrk *pTrk)
{
    TrkPt *p;

    // Delete all TrkPt's from the saved list
    while ((p = TAILQ_FIRST(&pTrk->savedTrkPtList)) != NULL) {
        TAILQ_REMOVE(&pTrk->savedTrkPtList, p, tqEntry);
        free(p);
    }

    // Clone all TrkPt's in the working list
    TAILQ_FOREACH(p, &pTrk->trkPtList, tqEntry) {
        TrkPt *clone = dupTrkPt(p);
        TAILQ_INSERT_TAIL(&pTrk->savedTrkPtList, clone, tqEntry);
    }

    return 0;
}

int restoreTrkPts(GpsTrk *pTrk)
{
    TrkPt *p = TAILQ_FIRST(&pTrk->trkPtList);

    // Delete all TrkPt's from the working list
    do {
        p = remTrkPt(pTrk, p);
    } while (p != NULL);

    // Move all the saved TrkPt's to the working list
    pTrk->numTrkPts = 0;
    while ((p = TAILQ_FIRST(&pTrk->savedTrkPtList)) != NULL) {
        TAILQ_REMOVE(&pTrk->savedTrkPtList, p, tqEntry);
        TAILQ_INSERT_TAIL(&pTrk->trkPtList, p, tqEntry);
        pTrk->numTrkPts++;
    }

    // Update the start time
    pTrk->startTime = TAILQ_FIRST(&pTrk->trkPtList)->timestamp;

    return 0;
}
