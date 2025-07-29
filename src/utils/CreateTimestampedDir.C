#include <TSystem.h>
#include <TDatime.h>
#include <iostream>
#include <string>

////////////////////////////////////////////////////////////////////////////////
/// \brief Creates a unique timestamped output directory.
///
/// This macro automatically:
///
/// 1. Generates a unique timestamped folder inside the specified base directory.
///
/// 2. Creates the folder structure and prints + returns directory path.
///
/// The generated directory will follow the format: baseOutputDir/run_YYYYMMDD_HHMM/
///
/// \param baseOutputDir Base directory within which timestamped directory will be created (must end with '/').
///
/// \returns The path to the newly created directory as a std::string.
///
////////////////////////////////////////////////////////////////////////////////
std::string CreateTimestampedDir(const std::string &baseOutputDir = "output/runs/")
{
    // Generate timestamp string using current date and time
    TDatime now;
    int year = now.GetYear();
    int month = now.GetMonth();
    int day = now.GetDay();
    int hour = now.GetHour();
    int minute = now.GetMinute();

    char timestamp[32];
    sprintf(timestamp, "run_%04d%02d%02d_%02d%02d", year, month, day, hour, minute);

    // Construct full output directory path
    std::string outputDir = baseOutputDir + timestamp + "/";
    gSystem->mkdir(outputDir.c_str(), kTRUE);

    std::cout << "Created unique run directory: " << outputDir << std::endl;

    // Return the new directory path
    return outputDir;
}