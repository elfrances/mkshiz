// This constant indicates a nil/unspec distance value
const double nilDist = -9999.99;

// This constant indicates a nil/unspec elevation value
const double nilElev = -9999.99;

// This constant indicates a nil/unspec grade/slope value
const double nilGrade = -99.99;

// This constant indicates a nil/unspec speed value
const double nilSpeed = -9999.99;

const double degToRad = (double) 0.01745329252;  // decimal degrees to radians
const double earthMeanRadius = (double) 6372797.560856;  // in meters
const double kmToMile = (double) 0.621371;
const double meterToFoot = (double) 3.28084;

// Banner line used in CSV files
const char *csvBannerLine = "<trkPt>,<inFile>,<lineNum>,<time>,<latitude>,<longitude>,<elevation>,<distance>,<speed>,<grade>";
