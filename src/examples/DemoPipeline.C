#include "../training/TrainClassificationModel.C"
#include "../evaluation/GetOptimalCut.C"
#include "../evaluation/CreateConfusionMatrix.C"
#include "../evaluation/CreateMVAScoreHistogram.C"
#include "../evaluation/CreateEnergyBinnedData.C"
#include "../evaluation/CreateEnergyPerformanceGraph.C"
#include "../application/TMVAReaderWrapper.C"
#include <TSystem.h>
#include <TMVA/Types.h>
#include <iostream>
#include <unordered_map>

////////////////////////////////////////////////////////////////////////////////
/// Demonstrates a full TMVA-based analysis pipeline.
///
/// Workflow:
///
/// 1. Train multiple TMVA models on user-defined input variables.
///
/// 2. Compute optimal FoM cut for each model.
///
/// 3. Generate confusion matrices and MVA score histograms.
///
/// 4. Compute energy-binned metrics and plot efficiency, purity, and FoM.
///
/// 5. Apply trained model to a ROOT TTree using TMVAReaderWrapper.
///
/// \param dataFile Path to the ROOT file containing Signal/Background trees.
/// \param outDir Output directory for models, plots, and results.
///
////////////////////////////////////////////////////////////////////////////////
void DemoPipeline(const std::string &dataFile, const std::string &outDir)
{
    // Ensure output directory exists
    gSystem->mkdir(outDir.c_str(), kTRUE);

    std::string methodSuffix = "demo"; // Unique identifier
    std::string filteredFileName = "filtered.root";

    // Define training variables and spectators
    std::vector<std::string> variables = {"CVNScoreNuE", "CVNScoreNuMu", "CVNScoreNC"};
    std::vector<std::string> spectators = {"TrueNuE"};

    // Define TMVA methods
    std::vector<MVAMethodConfig> methods = {
        {TMVA::Types::kMLP, "MLP", "!H:!V:NeuronType=tanh:VarTransform=N:NCycles=600:HiddenLayers=3:TestRate=5:!UseRegulator"},
        {TMVA::Types::kBDT, "BDT_AdaBoost", "!H:!V:NTrees=800:MinNodeSize=5%:MaxDepth=3:BoostType=AdaBoost:AdaBoostBeta=0.3:UseBaggedBoost:BaggedSampleFraction=0.5:SeparationType=CrossEntropy:nCuts=20"},
        {TMVA::Types::kBDT, "BDT_GradBoost", "!H:!V:NTrees=1000:MinNodeSize=7%:MaxDepth=2:BoostType=Grad:Shrinkage=0.1:UseBaggedBoost:BaggedSampleFraction=0.5:nCuts=30:SeparationType=CrossEntropy"},
    };

    // Step 1: Train models
    std::cout << "Training TMVA models..." << std::endl;
    TrainClassificationModel(methodSuffix, dataFile, outDir, filteredFileName, variables, spectators, methods, 0.3);

    std::string filteredFilePath = outDir + filteredFileName;
    std::string plotsDir = outDir + "models/plots/";

    // Step 2: Compute optimal cuts
    std::unordered_map<std::string, double> methodCuts;
    for (const auto &m : methods) {
        std::string methodName = m.name + "_" + methodSuffix;
        std::string perfPlot = plotsDir + methodName + "_FoM.png";
        double cut = GetOptimalCut(filteredFilePath, methodName, perfPlot, "ModelResults.root");
        methodCuts[methodName] = cut;
        std::cout << "Optimal cut for " << methodName << ": " << cut << std::endl;
    }

    // Step 3: Confusion matrices
    std::cout << "Generating confusion matrices..." << std::endl;
    for (const auto &m : methodCuts) {
        CreateConfusionMatrix(filteredFilePath, m.first, plotsDir, m.second, ConfusionMatrixType::Efficiency);
    }

    // Step 4: Score histograms
    std::cout << "Generating MVA score histograms..." << std::endl;
    for (const auto &m : methodCuts) {
        CreateMVAScoreHistogram(filteredFilePath, plotsDir, m.first, 50, -1.0, 1.0, AxisScale::Linear);
    }

    // Step 5: Energy-binned metrics
    std::cout << "Computing energy-binned metrics..." << std::endl;
    std::vector<double> energyBins = {0, 1, 2, 4, 6, 8, 10}; // Example binning
    std::string energyBinFile = outDir + "energyBins.root";

    CreateEnergyBinnedData(filteredFilePath, energyBinFile, methodCuts, energyBins);

    // Plot performance graphs
    CreateEnergyPerformanceGraph(energyBinFile, {
        {"MLP_" + methodSuffix, kRed},
        {"BDT_AdaBoost_" + methodSuffix, kBlue},
        {"BDT_GradBoost_" + methodSuffix, kGreen}},
        plotsDir + "EnergyVsEfficiency.png", GraphType::Efficiency);

    CreateEnergyPerformanceGraph(energyBinFile, {
        {"MLP_" + methodSuffix, kRed},
        {"BDT_AdaBoost_" + methodSuffix, kBlue},
        {"BDT_GradBoost_" + methodSuffix, kGreen}},
        plotsDir + "EnergyVsPurity.png", GraphType::Purity);

    CreateEnergyPerformanceGraph(energyBinFile, {
        {"MLP_" + methodSuffix, kRed},
        {"BDT_AdaBoost_" + methodSuffix, kBlue},
        {"BDT_GradBoost_" + methodSuffix, kGreen}},
        plotsDir + "EnergyVsFoM.png", GraphType::FoM);

    // Step 6: Apply model to data using TMVAReaderWrapper
    std::cout << "Applying trained model to data..." << std::endl;
    TMVAReaderWrapper reader;
    for (const auto &var : variables) reader.AddVariable(var);
    std::string basePath = gSystem->WorkingDirectory();
    std::string relativeWeightPath = outDir + "models/TMVAClassification_BDT_AdaBoost_" + methodSuffix + ".weights.xml";
    std::string absoluteWeightPath = basePath+"/"+relativeWeightPath;
    reader.BookMethod("BDT_AdaBoost_" + methodSuffix, absoluteWeightPath);
}