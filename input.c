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
#include "fit/fit_strings.c"

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
                FIT_MESG_NUM mesgNum = FitConvert_GetMessageNumber();

                switch (mesgNum) {
                    case FIT_MESG_NUM_FILE_ID: {
                        const FIT_FILE_ID_MESG *id = (FIT_FILE_ID_MESG *) mesg;
                        //printf("%s: type=%u, number=%u manufacturer=%u\n",
                        //        fitMesgNum(mesgNum),
                        //        id->type, id->number, id->manufacturer);
                        manufacturer = id->manufacturer;
                        break;
                    }

                    case FIT_MESG_NUM_DEVICE_SETTINGS: {
                        //const FIT_DEVICE_SETTINGS_MESG *dev = (FIT_DEVICE_SETTINGS_MESG *) mesg;
                        //printf("%s: utc_offset=%u time_offset=%u:%u clock_time=%u\n",
                        //        fitMesgNum(mesgNum),
                        //        dev->utc_offset, dev->time_offset[0], dev->time_offset[1], dev->clock_time);
                        break;
                    }

                    case FIT_MESG_NUM_USER_PROFILE: {
                        //const FIT_USER_PROFILE_MESG *user_profile = (FIT_USER_PROFILE_MESG *) mesg;
                        //printf("%s: weight=%0.1fkg gender=%u age=%u\n",
                        //        fitMesgNum(mesgNum),
                        //        user_profile->weight / 10.0f, user_profile->gender, user_profile->age);
                        break;
                    }

                    case FIT_MESG_NUM_ZONES_TARGET: {
                        //printf("%s: \n");
                        break;
                    }

                    case FIT_MESG_NUM_HR_ZONE: {
                        //printf("%s: \n");
                        break;
                    }

                    case FIT_MESG_NUM_POWER_ZONE: {
                        //printf("%s: \n");
                        break;
                    }

                    case FIT_MESG_NUM_SPORT: {
                        const FIT_SPORT_MESG *sport = (FIT_SPORT_MESG *) mesg;
                        //printf("%s: sport=%u sub_sport=%u\n",
                        //        fitMesgNum(mesgNum),
                        //        sport->sport, sport->sub_sport);

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
                        //printf("%s: timestamp=%u start_lat=%d start_long=%d elapsed_time=%d distance=%d num_laps: %d\n",
                        //        fitMesgNum(mesgNum),
                        //        session->timestamp, session->start_position_lat, session->start_position_long,
                        //        session->total_elapsed_time, session->total_distance, session->num_laps);
                        break;
                    }

                    case FIT_MESG_NUM_LAP: {
                        //const FIT_LAP_MESG *lap = (FIT_LAP_MESG *) mesg;
                        //printf("%s: timestamp=%u start_lat=%d start_long=%d end_lat=%d end_long=%d elapsed_time=%d distance=%d\n",
                        //        fitMesgNum(mesgNum),
                        //        lap->timestamp, lap->start_position_lat, lap->end_position_long, lap->end_position_lat, lap->end_position_long,
                        //        lap->total_elapsed_time, lap->total_distance);
                        break;
                    }

                    case FIT_MESG_NUM_RECORD: {
                        const FIT_RECORD_MESG *record = (FIT_RECORD_MESG *) mesg;
#if 0
                        printf("%s: timestamp=%u latitude=%d longitude=%d distance=%u enh_speed=%u enh_altitude=%u altitude=%u speed=%u power=%u grade=%u vertical_speed=%u heart_rate=%u cadence=%u temp=%d\n",
                                fitMesgNum(mesgNum),
                                record->timestamp, record->position_lat, record->position_long, record->distance,
                                record->enhanced_speed, record->enhanced_altitude,
                                record->altitude, record->speed, record->power, record->grade,
                                record->vertical_speed, record->heart_rate, record->cadence,
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

                                if (record->timestamp != FIT_DATE_TIME_INVALID) {
                                    pTrkPt->timestamp = (double) ((time_t) record->timestamp + fitEpoch);   // in s since UTC Epoch
                                }

                                if (record->position_lat != FIT_SINT32_INVALID) {
                                    pTrkPt->latitude = ((double) record->position_lat / (double) 0x7FFFFFFF) * (double) 180.0;
                                }

                                if (record->position_long != FIT_SINT32_INVALID) {
                                    pTrkPt->longitude = ((double) record->position_long / (double) 0x7FFFFFFF) * (double) 180.0;
                                }

                                if (record->distance != FIT_UINT32_INVALID) {
                                    pTrkPt->distance = ((double) record->distance / (double) 100.0); // in m
                                }

                                if (record->enhanced_altitude != FIT_UINT32_INVALID) {
                                    pTrkPt->elevation = (((double) record->enhanced_altitude / (double) 5.0) - (double) 500.0);    // in m
                                } else if (record->altitude != FIT_UINT16_INVALID) {
                                    pTrkPt->elevation = (((double) record->altitude / (double) 5.0) - (double) 500.0);    // in m
                                }

                                if (record->enhanced_speed != FIT_UINT32_INVALID) {
                                    pTrkPt->speed = ((double) record->enhanced_speed / (double) 1000.0);  // in m/s
                                } else if (record->speed != FIT_UINT16_INVALID) {
                                    pTrkPt->speed = ((double) record->speed / (double) 1000.0);  // in m/s
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
                        //printf("%s: timestamp=%u event=%s event_type=%s\n",
                        //        fitMesgNum(mesgNum),
                        //        event->timestamp, fitEvent(event->event), fitEventType(event->event_type));
                        if (event->event == FIT_EVENT_TIMER) {
                            if (event->event_type == FIT_EVENT_TYPE_START) {
                                timerRunning = true;
                            } else if (event->event_type == FIT_EVENT_TYPE_STOP) {
                                timerRunning = false;
                            }
                        }
                        break;
                    }

                    case FIT_MESG_NUM_WORKOUT: {
                        //const FIT_WORKOUT_MESG *workout = (FIT_WORKOUT_MESG *) mesg;
                        //printf("%s: sport=%u sub_sport=%u\n",
                        //        fitMesgNum(mesgNum),
                        //        workout->sport, workout->sub_sport);
                        break;
                    }

                    case FIT_MESG_NUM_DEVICE_INFO: {
                        //const FIT_DEVICE_INFO_MESG *device_info = (FIT_DEVICE_INFO_MESG *) mesg;
                        //printf("%s: timestamp=%u manufacturer=%s product=%s product_name=%s device_type=%s battery_status=%s descriptor=%s\n",
                        //        fitMesgNum(mesgNum),
                        //        device_info->timestamp, fitManufacturer(device_info->manufacturer), fitProduct(device_info->manufacturer, device_info->product), device_info->product_name,
                        //        fitAntPlusDeviceType(device_info->device_type), fitBatteryStatus(device_info->battery_status), device_info->descriptor);
                        break;
                    }

                    case FIT_MESG_NUM_ACTIVITY: {
                        //const FIT_ACTIVITY_MESG *activity = (FIT_ACTIVITY_MESG *) mesg;
                        //printf("%s: timestamp=%u, type=%u, event=%u, event_type=%u, num_sessions=%u\n",
                        //        fitMesgNum(mesgNum),
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
                        //printf("%s: sw_ver=%u hw_ver=%u\n",
                        //        fitMesgNum(mesgNum),
                        //        creator->software_version, creator->hardware_version);
                        break;
                    }

                    case FIT_MESG_NUM_TRAINING_FILE: {
                        //const FIT_TRAINING_FILE_MESG *tf = (FIT_TRAINING_FILE_MESG *) mesg;
                        //printf("%s: timestamp=%u manufacturer=%s product=%s type=%s\n",
                        //        fitMesgNum(mesgNum),
                        //        tf->time_created, fitManufacturer(tf->manufacturer), fitProduct(tf->manufacturer, tf->product), fitFile(tf->type));
                    }

                    case FIT_MESG_NUM_HRV: {
                        //const FIT_HRV_MESG *hrv = (FIT_HRV_MESG *) mesg;
                        //printf("%s: time=%.3lf\n",
                        //        fitMesgNum(mesgNum),
                        //        ((double) hrv->time[0] / 1000.0));
                        break;
                    }

                    case FIT_MESG_NUM_FIELD_DESCRIPTION: {
                        //const FIT_FIELD_DESCRIPTION_MESG *desc = (FIT_FIELD_DESCRIPTION_MESG *) mesg;
                        //printf("%s: field_name=%s\n",
                        //        fitMesgNum(mesgNum),
                        //        desc->field_name);
                        break;
                    }

                    case FIT_MESG_NUM_DEVELOPER_DATA_ID: {
                        //const FIT_DEVELOPER_DATA_ID_MESG *id = (FIT_DEVELOPER_DATA_ID_MESG *) mesg;
                        //printf("%s: manufacturer_id=%u\n",
                        //        fitMesgNum(mesgNum),
                        //        id->manufacturer_id);
                        break;
                    }

                    default: {
                        if ((mesgNum >= FIT_MESG_NUM_MFG_RANGE_MIN) && (mesgNum <= FIT_MESG_NUM_MFG_RANGE_MAX)) {
                            // TBD
                        } else {
                            //printf("%s: %u\n", mesgNum);
                        }
                        break;
                    }
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
