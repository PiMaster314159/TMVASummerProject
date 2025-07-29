#include <string>
#include <vector>
#include <TROOT.h>
#include <filesystem>
#include <TSystem.h>
#include <ROOT/RDataFrame.hxx>
#include <algorithm>
#include <stdexcept>
#include "../utils/SplitTreeByFilter.C"

enum class InteractionType {
    NuE,
    NuMu,
    NC
};

void FilterInputData(const std::string &inputFile,
                     const std::string &inputTreeName,
                     const std::string &outputFile,
                     const std::vector<std::string> &branchesToKeep,
                     InteractionType signalType,
                     bool includeCVNMax = false)
{
    // Validate input file
    if (!std::filesystem::exists(inputFile)) {
        throw std::runtime_error("Input file does not exist: " + inputFile);
    }
    if (gSystem->AccessPathName(inputFile.c_str())) {
        throw std::runtime_error("Unable to access ROOT file: " + inputFile);
    }

    // Define signal filter expression based on InteractionType
    std::string signalFilterExpr;
    switch (signalType) {
        case InteractionType::NuE:
            signalFilterExpr = "(TrueNuPdg == 12 || TrueNuPdg == -12) && IsCC"; // NuE
            break;
        case InteractionType::NuMu:
            signalFilterExpr = "(TrueNuPdg == 14 || TrueNuPdg == -14) && IsCC"; // NuMu
            break;
        case InteractionType::NC:
            signalFilterExpr = "!IsCC"; // NC
            break;
    }

    // If includeCVNMax is true, add a derived column to the tree first
    std::string tmpAugmentedFile = outputFile + "_tmp.root";

    if (includeCVNMax) {
        ROOT::RDataFrame df(inputTreeName, inputFile);

        auto addCVNMax = [signalType](double cvnNuE, double cvnNuMu, double cvnNC) {
            InteractionType predictedClass = InteractionType::NuMu;
            double maxScore = cvnNuMu;
            if (cvnNuE > maxScore) { maxScore = cvnNuE; predictedClass = InteractionType::NuE; }
            if (cvnNC > maxScore)   { maxScore = cvnNuMu; predictedClass = InteractionType::NC; }

            return (predictedClass == signalType) ? 1.0 : 0.0;
        };

        auto addLinearCut = [](double cvnNuE, double cvnNuMu, double cvnNC){
            // return (cvnNuMu<0.14 && cvnNC<0.45) ? 1. : 0.; //NuE
            return (cvnNuE<0.3 && cvnNC<0.43) ? 1. : 0.;//NuMu
            // return (cvnNuE<0.49 && cvnNuMu<0.46) ? 1. : 0.; //NC
        };

        auto dfWithCVN = df.Define("CVNMax_NuMu", addCVNMax,
                                   {"CVNScoreNuE", "CVNScoreNuMu", "CVNScoreNC"});
        auto dfWithLinCut = dfWithCVN.Define("LinearCut_NuMu", addLinearCut,
                                   {"CVNScoreNuE", "CVNScoreNuMu", "CVNScoreNC"});

        std::vector<std::string> augmentedBranches = branchesToKeep;
        // augmentedBranches.push_back("CVNMax");

        dfWithLinCut.Snapshot(inputTreeName, tmpAugmentedFile);
    } else {
        tmpAugmentedFile = inputFile; // Use original file if no augmentation
    }

    // Use SplitTreeByFilter for actual splitting
    std::vector<std::string> finalBranches = branchesToKeep;
    if(includeCVNMax){
        finalBranches.push_back("CVNMax_NuMu");
        finalBranches.push_back("LinearCut_NuMu");
    }
    SplitTreeByFilter(tmpAugmentedFile, inputTreeName, outputFile, finalBranches, signalFilterExpr, "CVNScoreNuE != -999");

    // Clean up temporary file if it was created
    if (includeCVNMax && std::filesystem::exists(tmpAugmentedFile)) {
        std::filesystem::remove(tmpAugmentedFile);
    }

    std::cout << "Filtered data written to: " << outputFile << std::endl;
}
