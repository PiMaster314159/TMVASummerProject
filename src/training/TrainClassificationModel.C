#include <TMVA/Types.h>
#include <string>
#include <iostream>
#include <TROOT.h>
#include <TMVA/Tools.h>
#include <TFile.h>
#include <TMVA/DataLoader.h>
#include <TMVA/Factory.h>
#include "../utils/SplitTreeByFilter.C"
#include <TSystem.h>
////////////////////////////////////////////////////////////////////////////////
/// \struct
/// Structure defining a TMVA method configuration.
///
/// This struct represents the configuration for a single TMVA classifier.
/// It includes:
///
/// - The TMVA method type (e.g., kMLP, kBDT, etc.).
///
/// - A base name for the method (to be later combined with a unique suffix).
///
/// - The configuration string specifying hyperparameters and TMVA options.
///
/// These configurations allow for the dynamic registration of multiple TMVA methods for
/// training within TrainClassificationModel.
///
/// \param type       TMVA method type (see TMVA::Types::EMVA).
/// \param name       Base name for identifying the method.
/// \param options    TMVA configuration string for hyperparameters.
///
////////////////////////////////////////////////////////////////////////////////
struct MVAMethodConfig {
    TMVA::Types::EMVA type;      ///< Method type (e.g., TMVA::Types::kMLP, TMVA::Types::kBDT)
    std::string name;            ///< Base name of the method
    std::string options;         ///< TMVA configuration string
};

////////////////////////////////////////////////////////////////////////////////
/// Train a TMVA classification model and export the results.
///
/// This function trains multiple TMVA classification methods on a dataset consisting of
/// `"Signal"` and `"Background"` TTrees. It performs the following steps:
///
/// 1. Load Input Trees:
///
///    - Reads the "Signal" and "Background" trees from the specified ROOT file.
///
///    - Validates their existence.
///
/// 2. Configure TMVA Components:
///
///    - Initializes TMVA Tools and a Factory for classification.
///
///    - Creates a DataLoader and registers input variables and spectators.
///
/// 3. Prepare Training and Test Splits:
///
///    - Splits signal and background data into training and test sets based on a user-defined ratio.
///
/// 4. Book TMVA Methods Dynamically:
///
///    - Loops over user-provided configurations and books each method with a unique name.
///
/// 5. Run Training Pipeline:
///
///    - Trains, tests, and evaluates all booked methods.
///
///    - Writes TMVA results (weights, evaluation metrics) to an output ROOT file.
///
/// 6. Post-Processing:
///
///    - Creates a filtered lightweight ROOT file with essential variables and MVA scores.
///
///    - Ensures output directories for models and plots exist.
///
/// \param[in] methodSuffix        Suffix added to each method name for unique identification.
/// \param[in] inputFile           Path to the ROOT file containing "Signal" and "Background" trees.
/// \param[in] outputDir           Directory for storing TMVA outputs and trained models (must end with '/').
/// \param[in] filteredFileName    Name of the lightweight ROOT file containing filtered branches and MVA scores.
/// \param[in] inputVars           List of input variables (branch names) for training.
/// \param[in] spectatorVars       List of spectator variables (monitored but not used in training).
/// \param[in] methods             Vector of MVA method configurations.
/// \param[in] trainRatio          Fraction of signal events used for training (default: 0.3).
///
/// \throws std::runtime_error     If required input file is missing or input trees are missing.
///
/// \note Output TMVA ROOT file will be saved as "<outputDir>/TMVAC.root".
////////////////////////////////////////////////////////////////////////////////
void TrainClassificationModel(const std::string &methodSuffix,
                              const std::string &inputFile,
                              const std::string &outputDir,
                              const std::string &filteredFileName,
                              const std::vector<std::string> &inputVars,
                              const std::vector<std::string> &spectatorVars,
                              const std::vector<MVAMethodConfig> &methods,
                              double trainRatio = 0.3)
{
    if (gSystem->AccessPathName(inputFile.c_str())) {
        throw std::runtime_error("Input file does not exist or cannot be accessed: " + inputFile);
    }

    std::cout << "Initializing TMVA training for suffix: " << methodSuffix << std::endl;

    ROOT::EnableImplicitMT(); // Enable multi-threading for increased performance
    TMVA::Tools::Instance();  // Initialize TMVA

    // Open input ROOT file
    TFile inputFileHandle(inputFile.c_str());
    TTree *signalTree = inputFileHandle.Get<TTree>("Signal");
    TTree *backgroundTree = inputFileHandle.Get<TTree>("Background");
    if (!signalTree || !backgroundTree) {
        throw std::runtime_error("Error: Missing 'Signal' or 'Background' tree in file: " + inputFile);
    }

    const Long64_t nSignal = signalTree->GetEntries();
    const Long64_t nBackground = backgroundTree->GetEntries();
    std::cout << "Signal entries: " << nSignal << ", Background entries: " << nBackground << std::endl;

    // Configure TMVA DataLoader
    std::cout << "Configuring TMVA DataLoader..." << std::endl;
    auto dataloader = std::make_unique<TMVA::DataLoader>(outputDir + "models");
    dataloader->AddSignalTree(signalTree, 1.0);
    dataloader->AddBackgroundTree(backgroundTree, 1.0);

    // Collect variables for the filtered output later
    std::vector<std::string> allColumns;
    allColumns.reserve(inputVars.size() + spectatorVars.size() + methods.size());

    // Add training variables
    for (const auto &var : inputVars) {
        dataloader->AddVariable(var);
        allColumns.push_back(var);
    }

    // Add spectator variables
    for (const auto &spec : spectatorVars) {
        dataloader->AddSpectator(spec);
        allColumns.push_back(spec);
    }

    // Prepare TMVA output ROOT file
    const std::string tmvaOutputPath = outputDir + "TMVAC.root";
    auto tmvaOutputFile = std::make_unique<TFile>(tmvaOutputPath.c_str(), "RECREATE");

    // Configure TMVA Factory
    std::cout << "Configuring TMVA Factory..." << std::endl;
    std::string factoryOptions = "!V:!Silent:Color:DrawProgressBar";
    factoryOptions += ":Transformations=I;G;N:AnalysisType=Classification";
    auto factory = std::make_unique<TMVA::Factory>("TMVAClassification", tmvaOutputFile.get(), factoryOptions.c_str());

    // Compute train/test split
    const Long64_t nTrain = std::llround(trainRatio * nSignal);
    const Long64_t nSignalTest = nSignal - nTrain;
    const Long64_t nBackgroundTest = (nBackground * nSignalTest) / nSignal;

    dataloader->PrepareTrainingAndTestTree(
        "", "",
        "nTrain_Signal=" + std::to_string(nTrain) +
        ":nTrain_Background=" + std::to_string(nTrain) +
        ":nTest_Signal=" + std::to_string(nSignalTest) +
        ":nTest_Background=" + std::to_string(nBackgroundTest) +
        ":SplitMode=Random:SplitSeed=42:NormMode=NumEvents:!V");

    // Book all TMVA methods dynamically
    std::cout << "Booking TMVA methods..." << std::endl;
    for (const auto &method : methods) {
        std::string uniqueMethodName = method.name + "_" + methodSuffix;
        factory->BookMethod(dataloader.get(), method.type, uniqueMethodName, method.options);
        allColumns.push_back(uniqueMethodName);
        std::cout << "Booked method: " << uniqueMethodName << std::endl;
    }

    // Train, test, and evaluate
    std::cout << "Starting training..." << std::endl;
    factory->TrainAllMethods();
    std::cout << "Testing methods..." << std::endl;
    factory->TestAllMethods();
    std::cout << "Evaluating performance..." << std::endl;
    factory->EvaluateAllMethods();

    // Save TMVA results
    std::cout << "Writing TMVA output file..." << std::endl;
    tmvaOutputFile->Write();
    tmvaOutputFile->Close();

    // Create a filtered lightweight ROOT file for downstream analysis
    std::cout << "Generating filtered output file: " << filteredFileName << std::endl;
    SplitTreeByFilter(tmvaOutputFile->GetName(), outputDir + "models/TestTree",
                  outputDir + filteredFileName, allColumns, "classID==0");

    // Ensure plots directory exists
    gSystem->mkdir((outputDir + "models/plots").c_str(), kTRUE);
    std::cout << "Training pipeline completed for suffix: " << methodSuffix << std::endl;
}
