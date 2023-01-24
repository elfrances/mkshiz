#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "const.h"
#include "defs.h"
#include "trkpt.h"

// FIT SDK files
//#include "fit/decode.c"
#include "fit/fit.c"
#include "fit/fit_example.c"
#include "fit/fit_crc.c"
#include "fit/fit_convert.c"

// FIT uses December 31, 1989 UTC as their Epoch. See below
// for the details:
//   https://developer.garmin.com/fit/cookbook/datetime/
static const time_t fitEpoch = 631065600;

// Parse the FIT file and create a list of Track Points (TrkPt's)
int parseFitFile(CmdArgs *pArgs, GpsTrk *pTrk, const char *inFile)
{
    FILE *fp;
    TrkPt *pTrkPt = NULL;
    FIT_UINT8 inBuf[8];
    FIT_CONVERT_RETURN conRet = FIT_CONVERT_CONTINUE;
    FIT_UINT32 bufSize;
    FIT_UINT32 mesgIndex = 0;
    FIT_MANUFACTURER manufacturer = FIT_MANUFACTURER_INVALID;
    Bool timerRunning = true;

    // Open the FIT file for reading
    if ((fp = fopen(inFile, "r")) == NULL) {
        fprintf(stderr, "Failed to open input file %s\n", inFile);
        return -1;
    }

    FitConvert_Init(FIT_TRUE);

    while (!feof(fp) && (conRet == FIT_CONVERT_CONTINUE)) {
        for (bufSize = 0; (bufSize < sizeof (inBuf)) && !feof(fp); bufSize++) {
            inBuf[bufSize] = (FIT_UINT8) getc(fp);
        }

        do {
            if ((conRet = FitConvert_Read(inBuf, bufSize)) == FIT_CONVERT_MESSAGE_AVAILABLE) {
                const FIT_UINT8 *mesg = FitConvert_GetMessageData();
                FIT_UINT16 mesgNum = FitConvert_GetMessageNumber();

                //printf("Mesg %d (%d) - ", mesgIndex, mesgNum);

                switch (mesgNum) {
                case FIT_MESG_NUM_FILE_ID: {
                    const FIT_FILE_ID_MESG *id = (FIT_FILE_ID_MESG *) mesg;
                    //printf("File ID: type=%u, number=%u manufacturer=%u\n",
                    //        id->type, id->number, id->manufacturer);
                    manufacturer = id->manufacturer;
                    break;
                }

                case FIT_MESG_NUM_DEVICE_SETTINGS: {
                    //const FIT_DEVICE_SETTINGS_MESG *dev = (FIT_DEVICE_SETTINGS_MESG *) mesg;
                    //printf("Device Settings: utc_offset=%u time_offset=%u:%u clock_time=%u\n",
                    //        dev->utc_offset, dev->time_offset[0], dev->time_offset[1], dev->clock_time);
                    break;
                }

                case FIT_MESG_NUM_USER_PROFILE: {
                    //const FIT_USER_PROFILE_MESG *user_profile = (FIT_USER_PROFILE_MESG *) mesg;
                    //printf("User Profile: weight=%0.1fkg gender=%u age=%u\n",
                    //        user_profile->weight / 10.0f, user_profile->gender, user_profile->age);
                    break;
                }

                case FIT_MESG_NUM_ZONES_TARGET: {
                    //printf("Zones Target: \n");
                    break;
                }

                case FIT_MESG_NUM_HR_ZONE: {
                    //printf("HR Zone: \n");
                    break;
                }

                case FIT_MESG_NUM_POWER_ZONE: {
                    //printf("Power Zone: \n");
                    break;
                }

                case FIT_MESG_NUM_SPORT: {
                    const FIT_SPORT_MESG *sport = (FIT_SPORT_MESG *) mesg;
                    //printf("Sport: sport=%u sub_sport=%u\n", sport->sport, sport->sub_sport);
                    if (sport->sport == FIT_SPORT_RUNNING) {
                        pTrk->actType = run;
                    } else if (sport->sport == FIT_SPORT_CYCLING) {
                        pTrk->actType = ride;
                    } else if (sport->sport == FIT_SPORT_WALKING) {
                        pTrk->actType = walk;
                    } else if (sport->sport == FIT_SPORT_HIKING) {
                        pTrk->actType = hike;
                    } else {
                        pTrk->actType = other;
                    }
                    break;
                }

                case FIT_MESG_NUM_SESSION: {
                    //const FIT_SESSION_MESG *session = (FIT_SESSION_MESG *) mesg;
                    //printf("Session: timestamp=%u start_lat=%d start_long=%d elapsed_time=%d distance=%d num_laps: %d\n",
                    //        session->timestamp, session->start_position_lat, session->start_position_long,
                    //        session->total_elapsed_time, session->total_distance, session->num_laps);
                    break;
                }

                case FIT_MESG_NUM_LAP: {
                    //const FIT_LAP_MESG *lap = (FIT_LAP_MESG *) mesg;
                    //printf("Lap: timestamp=%u start_lat=%d start_long=%d end_lat=%d end_long=%d elapsed_time=%d distance=%d\n",
                    //        lap->timestamp, lap->start_position_lat, lap->end_position_long, lap->end_position_lat, lap->end_position_long,
                    //        lap->total_elapsed_time, lap->total_distance);
                    break;
                }

                case FIT_MESG_NUM_RECORD: {
                    const FIT_RECORD_MESG *record = (FIT_RECORD_MESG *) mesg;
#if 0
                    printf("Record: timestamp=%u latitude=%d longitude=%d distance=%u altitude=%u speed=%u power=%u heart_rate=%u cadence=%u temp=%d",
                            record->timestamp, record->position_lat, record->position_long, record->distance,
                            record->altitude, record->speed, record->power,
                            record->heart_rate, record->cadence,
                            record->temperature);

                    if ((record->compressed_speed_distance[0] != FIT_BYTE_INVALID) ||
                        (record->compressed_speed_distance[1] != FIT_BYTE_INVALID) ||
                        (record->compressed_speed_distance[2] != FIT_BYTE_INVALID)) {
                        static FIT_UINT32 accumulated_distance16 = 0;
                        static FIT_UINT32 last_distance16 = 0;
                        FIT_UINT16 speed100;
                        FIT_UINT32 distance16;

                        speed100 = record->compressed_speed_distance[0] | ((record->compressed_speed_distance[1] & 0x0F) << 8);
                        printf(", speed = %0.2fm/s", speed100 / 100.0f);

                        distance16 = (record->compressed_speed_distance[1] >> 4) | (record->compressed_speed_distance[2] << 4);
                        accumulated_distance16 += (distance16 - last_distance16) & 0x0FFF;
                        last_distance16 = distance16;

                        printf(", distance = %0.3fm", accumulated_distance16 / 16.0f);
                    }
#endif
                    if (timerRunning) {
                        // The Strava app generates a pair of FIT RECORD messages
                        // for each trackpoint (i.e. timestamp). The first one seems
                        // to always have a valid distance value of 0.000, but no
                        // latitude/longitude/altitude values: e.g.
                        //
                        // Mesg 9 (21) - Event: timestamp=1018803532 event=0 event_type=0
                        // Mesg 10 (20) - Record: timestamp=1018803532 distance=0.000
                        // Mesg 11 (20) - Record: timestamp=1018803532 latitude=43.6232699098 longitude=-114.3533090010 enh_altitude=1712.000 speed=0.310
                        // Mesg 12 (20) - Record: timestamp=1018803533 distance=0.000
                        // Mesg 13 (20) - Record: timestamp=1018803533 latitude=43.6232681496 longitude=-114.3533167124 enh_altitude=1712.000 speed=0.112
                        //
                        // So here we detect, and skip, such RECORD messages...
                        if ((manufacturer == FIT_MANUFACTURER_STRAVA) &&
                            ((record->position_lat == FIT_SINT32_INVALID) ||
                             (record->position_long == FIT_SINT32_INVALID) ||
                             (record->enhanced_altitude == FIT_UINT32_INVALID))) {
                            //printf(" *** SKIPPED ***");
                        } else {
                            // Alloc and init new TrkPt object
                            if ((pTrkPt = newTrkPt(pTrk->numTrkPts++, inFile, mesgIndex)) == NULL) {
                                fprintf(stderr, "Failed to create TrkPt object !!!\n");
                                return -1;
                            }

                            pTrkPt->timestamp = (double) ((time_t) record->timestamp + fitEpoch);

                            if (record->position_lat != FIT_SINT32_INVALID) {
                                pTrkPt->latitude = ((double) record->position_lat / (double) 0x7FFFFFFF) * 180.0;
                            }

                            if (record->position_long != FIT_SINT32_INVALID) {
                                pTrkPt->longitude = ((double) record->position_long / (double) 0x7FFFFFFF) * 180.0;
                            }

                            if (record->distance != FIT_UINT32_INVALID) {
                                pTrkPt->distance = ((double) record->distance / 100.0); // in m
                            }

                            if (record->enhanced_altitude != FIT_UINT32_INVALID) {
                                pTrkPt->elevation = (((double) record->enhanced_altitude / 5.0) - 500.0);    // in m
                            } else if (record->altitude != FIT_UINT16_INVALID) {
                                pTrkPt->elevation = (((double) record->altitude / 5.0) - 500.0);    // in m
                            }

                            if (record->enhanced_speed != FIT_UINT32_INVALID) {
                                pTrkPt->speed = ((double) record->enhanced_speed / 1000.0);  // in m/s
                            } else if (record->speed != FIT_UINT16_INVALID) {
                                pTrkPt->speed = ((double) record->speed / 1000.0);  // in m/s
                            }

                            if (record->grade != FIT_SINT16_INVALID) {
                                pTrkPt->grade = record->grade;
                            }

                            if (record->temperature != FIT_SINT8_INVALID) {
                                pTrkPt->ambTemp = record->temperature;
                                pTrk->inMask |= SD_ATEMP;
                            }

                            if (record->cadence != FIT_UINT8_INVALID) {
                                pTrkPt->cadence = record->cadence;
                                pTrk->inMask |= SD_CADENCE;
                            }

                            if (record->heart_rate != FIT_UINT8_INVALID) {
                                pTrkPt->heartRate = record->heart_rate;
                                pTrk->inMask |= SD_HR;
                            }

                            if (record->power != FIT_UINT16_INVALID) {
                                pTrkPt->power = record->power;
                                pTrk->inMask |= SD_POWER;
                            }

                            // Insert track point at the tail of the queue
                            TAILQ_INSERT_TAIL(&pTrk->trkPtList, pTrkPt, tqEntry);

                            pTrkPt = NULL;
                        }
                    } else {
                        fprintf(stderr, "Hu? Timer not running !!!\n");
                    }
                    //printf("\n");
                    break;
                }

                case FIT_MESG_NUM_EVENT: {
                    const FIT_EVENT_MESG *event = (FIT_EVENT_MESG *) mesg;
                    //printf("Event: timestamp=%u event=%u event_type=%u\n",
                    //        event->timestamp, event->event, event->event_type);
                    if (event->event == FIT_EVENT_TIMER) {
                        if (event->event_type == FIT_EVENT_TYPE_START) {
                            timerRunning = true;
                        } else if (event->event_type == FIT_EVENT_TYPE_STOP) {
                            timerRunning = false;
                        }
                    }
                    break;
                }

                case FIT_MESG_NUM_DEVICE_INFO: {
                    //const FIT_DEVICE_INFO_MESG *device_info = (FIT_DEVICE_INFO_MESG *) mesg;
                    //printf("Device Info: timestamp=%u manufacturer=%u product=%u\n",
                    //        device_info->timestamp, device_info->manufacturer, device_info->product);
                    break;
                }

                case FIT_MESG_NUM_ACTIVITY: {
                    //const FIT_ACTIVITY_MESG *activity = (FIT_ACTIVITY_MESG *) mesg;
                    //printf("Activity: timestamp=%u, type=%u, event=%u, event_type=%u, num_sessions=%u\n",
                    //       activity->timestamp, activity->type,
                    //       activity->event, activity->event_type,
                    //       activity->num_sessions);
                    {
                        FIT_ACTIVITY_MESG old_mesg;
                        old_mesg.num_sessions = 1;
                        FitConvert_RestoreFields(&old_mesg);
                        //printf("Restored num_sessions=1 - Activity: timestamp=%u, actType=%u, event=%u, event_type=%u, num_sessions=%u\n",
                        //       activity->timestamp, activity->type,
                        //       activity->event, activity->event_type,
                        //       activity->num_sessions);
                    }
                    break;
                }

                case FIT_MESG_NUM_FILE_CREATOR: {
                    //const FIT_FILE_CREATOR_MESG *creator = (FIT_FILE_CREATOR_MESG *) mesg;
                    //printf("File Creator: sw_ver=%u hw_ver=%u\n", creator->software_version, creator->hardware_version);
                    break;
                }

                case FIT_MESG_NUM_HRV: {
                    //const FIT_HRV_MESG *hrv = (FIT_HRV_MESG *) mesg;
                    //printf("HRV: time=%.3lf\n", ((double) hrv->time[0] / 1000.0));
                    break;
                }

                case FIT_MESG_NUM_FIELD_DESCRIPTION: {
                    //const FIT_FIELD_DESCRIPTION_MESG *desc = (FIT_FIELD_DESCRIPTION_MESG *) mesg;
                    //printf("Field Description: field_name=%s\n", desc->field_name);
                    break;
                }

                case FIT_MESG_NUM_DEVELOPER_DATA_ID: {
                    //const FIT_DEVELOPER_DATA_ID_MESG *id = (FIT_DEVELOPER_DATA_ID_MESG *) mesg;
                    //printf("Developer Data ID: manufacturer_id=%u\n", id->manufacturer_id);
                    break;
                }

                default:
                    //printf("Unknown\n");
                    break;
                }
            }

            mesgIndex++;
        } while (conRet == FIT_CONVERT_MESSAGE_AVAILABLE);
    }

    fclose(fp);

    if (conRet != FIT_CONVERT_END_OF_FILE) {
        const char *errMsg = NULL;
        if (conRet == FIT_CONVERT_ERROR) {
            errMsg = "Error decoding file";
        } else if (conRet == FIT_CONVERT_CONTINUE) {
            errMsg = "Unexpected end of file";
        } else if (conRet == FIT_CONVERT_DATA_TYPE_NOT_SUPPORTED) {
            errMsg = "File is not FIT";
        } else if (conRet == FIT_CONVERT_PROTOCOL_VERSION_NOT_SUPPORTED) {
            errMsg = "Protocol version not supported";
        }

        fprintf(stderr, "%s !!!\n", errMsg);

        return -1;
    }

    return 0;
}