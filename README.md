# mkshiz
The **FulGaz** virtual cycling app uses a SHIZ file to control the resistance of the smart trainer and the playback speed of the video.  The SHIZ file is a plain text file that uses the JSON enconding, and includes an array of trackpoints, where each trackpoint indicates the time, distance, speed, grade, bearing, etc. at the time.
 
**mkshiz** is an interactive command-line tool for generating a SHIZ file from a FIT file created by a bike computer during a real outdoor activity.
