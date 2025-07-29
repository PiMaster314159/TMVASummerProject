#include <string>
#include <vector>
#include <iostream>
#include <ROOT/RDataFrame.hxx>
#include <filesystem>
#include <TSystem.h>
////////////////////////////////////////////////////////////////////////////////
/// Split a ROOT TTree into "Signal" and "Background" trees using classification filters.
///
/// This function reads in a TTree from a ROOT file and applies two filtering steps:
///
/// 1. Exclusion filter: Removes unwanted events before classification (default: `"1"` means no exclusion).
///
/// 2. Signal filter: Classifies the remaining events into:
///
///     - Signal: Events that pass the signal filter.
///
///     - Background: Events that fail the signal filter (i.e., complement of the signal set).
///
/// The filtered events are stored in two separate TTrees:
///
/// - `"Signal"`
///
/// - `"Background"`
///
/// Both trees are written to the base directory of the specified output ROOT file. If the file exists, the Signal and Background
/// tree data are updated.
///
/// \param[in] inputFile        Path to the input ROOT file.
/// \param[in] inputTreeName    Name of the TTree to process.
/// \param[in] outputFile       Path to the output ROOT file for results.
/// \param[in] branchesToKeep   List of branch names to include in the output trees.
/// \param[in] signalFilterExpr Expression used to classify events as Signal.
/// \param[in] exclusionFilter  Expression to filter out unwanted events before classification (default: "1").
///
/// \throws std::runtime_error  If the input file cannot be found or the TTree cannot be opened.
////////////////////////////////////////////////////////////////////////////////
void SplitTreeByFilter(const std::string &inputFile,
                       const std::string &inputTreeName,
                       const std::string &outputFile,
                       const std::vector<std::string> &branchesToKeep,
                       const std::string &signalFilterExpr,
                       const std::string &exclusionFilter = "1")
{
    // Validate input file and input TTree existence
    if (!std::filesystem::exists(inputFile)) {
        throw std::runtime_error("Input file does not exist: " + inputFile);
    }
    if (gSystem->AccessPathName(inputFile.c_str())) {
        throw std::runtime_error("Unable to access ROOT file: " + inputFile);
    }

    std::cout << "Opening input file: " << inputFile << std::endl;
    // Open TTree as RDataFrame
    ROOT::RDataFrame df(inputTreeName, inputFile);

    // Apply exclusion filter first
    std::cout << "Applying exclusion filter: \"" << exclusionFilter << "\"" << std::endl;
    auto dfFiltered = df.Filter(exclusionFilter);
    double removed = double(*df.Count()-*dfFiltered.Count());
    double percent = removed / double(*df.Count());
    std::cout<< "Details: " << percent << " | " << double(*df.Count()) << std::endl;

    // Define complementary background filter
    std::string backgroundFilter = "!(" + signalFilterExpr + ")";
    std::cout << "Splitting tree using signal filter: \"" << signalFilterExpr << "\"" << std::endl;

    // Print out the size of the respective TTrees
    auto signalCount = dfFiltered.Filter(signalFilterExpr).Count();
    auto backgroundCount = dfFiltered.Filter(backgroundFilter).Count();
    std::cout << "Signal events: " << *signalCount << " | Background events: " << *backgroundCount << std::endl;

    // Configure snapshot options: overwrite for Signal, append for Background
    ROOT::RDF::RSnapshotOptions optCreate, optUpdate;
    optCreate.fMode = "RECREATE"; // Create or overwrite signal tree
    optUpdate.fMode = "UPDATE"; // Append background tree to same file as signal tree

    // Write Signal tree
    std::cout << "Writing Signal tree to: " << outputFile << std::endl;
    dfFiltered.Filter(signalFilterExpr).Snapshot("Signal", outputFile, branchesToKeep, optCreate);

    // Write Background tree
    std::cout << "Appending Background tree to: " << outputFile << std::endl;
    dfFiltered.Filter(backgroundFilter).Snapshot("Background", outputFile, branchesToKeep, optUpdate);

    std::cout << "Finished writing trees. Signal and Background saved to: " << outputFile << std::endl;
}