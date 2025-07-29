#include <string>
#include <iostream>
#include <ROOT/RDataFrame.hxx>
#include <vector>
#include <RooSpline.h>
#include <RooRealVar.h>
#include <TCanvas.h>
#include <RooPlot.h>
#include <TLegend.h>
#include <TF1.h>
#include "../utils/UpdateOrInsertByKey.C"
#include <TSystem.h>
////////////////////////////////////////////////////////////////////////////////
/// Compute the optimal MVA classifier score cut value using a Figure of Merit (FoM) spline and log results.
///
/// This method determines the cut value on an MVA score that maximizes:
///
/// \[ \text{FoM} = \text{Efficiency} \times \text{Purity} \]
///
/// Workflow:
///
/// 1. Build histograms of the MVA score for signal and background events.
///
/// 2. Compute efficiency, purity, and FoM for each candidate cut.
///
/// 3. Use RooSpline interpolation for smooth curves.
///
/// 4. Find the cut that maximizes FoM.
///
/// 5. (Optional) Generate and save a visualization of efficiency, purity, and FoM.
///
/// 6. (Optional) Log results to a ROOT file for later analysis.
///
/// ### Parameters:
/// \param[in] inputFile       Path to the ROOT file containing "Signal" and "Background" TTrees.
/// \param[in] mvaBranch       Name of the branch holding the MVA score (e.g., "BDT_base").
/// \param[in] plotFile        File path to save FoM visualization (leave empty to skip plotting).
/// \param[in] resultsFile     File path to log results (leave empty to skip logging).
/// \param[in] resultsTree     Name of TTree inside results file for logging (default: "Performance").
/// \param[in] nBins           Number of histogram bins for discretizing MVA scores (default: 1000).
/// \param[in] minScore        Minimum expected MVA score (default: -1.0).
/// \param[in] maxScore        Maximum expected MVA score (default: 1.0).
///
/// \return Optimal cut value that maximizes FoM.
///
/// \throws std::runtime_error If the input ROOT file cannot be accessed or histograms are empty.
///
////////////////////////////////////////////////////////////////////////////////
double GetOptimalCut(const std::string &inputFile,
                     const std::string &mvaBranch,
                     const std::string &plotFile = "",
                     const std::string &resultsFile = "",
                     const std::string &resultsTree = "Performance",
                     int nBins = 1000,
                     double minScore = -1.0,
                     double maxScore = 1.0)
{
    if (gSystem->AccessPathName(inputFile.c_str())) {
        throw std::runtime_error("Input file does not exist or cannot be accessed: " + inputFile);
    }
    std::cout << "[INFO] Computing optimal cut for MVA branch: " << mvaBranch << std::endl;

    // Load signal and background datasets
    ROOT::RDataFrame dfSignal("Signal", inputFile);
    ROOT::RDataFrame dfBackground("Background", inputFile);

    std::cout << "[INFO] Building histograms for signal and background..." << std::endl;

    // Create histograms of MVA scores
    auto hSignal = dfSignal.Histo1D({"hSignal", "Signal Distribution", nBins, minScore, maxScore}, mvaBranch).GetValue();
    auto hBackground = dfBackground.Histo1D({"hBackground", "Background Distribution", nBins, minScore, maxScore}, mvaBranch).GetValue();

    const double totalSignal = hSignal.Integral();
    if (totalSignal <= 0) {
        throw std::runtime_error("[ERROR] Signal histogram is empty. Cannot compute FoM.");
    }

    // Vectors for storing metrics
    std::vector<double> cuts, efficiencies, purities, fomValues;
    cuts.reserve(nBins);
    efficiencies.reserve(nBins);
    purities.reserve(nBins);
    fomValues.reserve(nBins);

    std::cout << "[INFO] Calculating efficiency, purity, and FoM for candidate cuts..." << std::endl;

    for (int i = 1; i <= nBins; ++i) {
        double cutValue = hSignal.GetBinLowEdge(i);
        double tp = hSignal.Integral(i, nBins);  // True Positives
        double fp = hBackground.Integral(i, nBins); // False Positives

        double efficiency = tp / totalSignal;
        double purity = (tp + fp > 0) ? tp / (tp + fp) : 0.0;
        double fom = efficiency * purity;

        cuts.push_back(cutValue);
        efficiencies.push_back(efficiency);
        purities.push_back(purity);
        fomValues.push_back(fom);
    }

    // Build RooSplines for smooth interpolation
    RooRealVar x("x", (mvaBranch + " Score").c_str(), minScore, maxScore);
    RooSpline effSpline("effSpline", "Efficiency Spline", x, cuts, efficiencies);
    RooSpline purSpline("purSpline", "Purity Spline", x, cuts, purities);
    RooSpline fomSpline("fomSpline", "FoM Spline", x, cuts, fomValues);

    // Visualization (optional)
    if (!plotFile.empty()) {
        std::cout << "[INFO] Generating FoM visualization: " << plotFile << std::endl;
        TCanvas canvas("canvas", "Efficiency, Purity, FoM", 1200, 800);
        RooPlot *frame = x.frame();
        effSpline.plotOn(frame, RooFit::LineColor(kRed));
        purSpline.plotOn(frame, RooFit::LineColor(kBlue));
        fomSpline.plotOn(frame, RooFit::LineColor(kGreen));

        frame->SetAxisRange(0, 1, "Y");
        frame->SetXTitle((mvaBranch + " Score").c_str());

        TLegend legend(0.15, 0.15, 0.45, 0.30);
        legend.SetFillStyle(0);
        legend.SetTextSize(0.04);
        legend.AddEntry((TObject *)frame->getObject(0), "Efficiency", "l");
        legend.AddEntry((TObject *)frame->getObject(1), "Purity", "l");
        legend.AddEntry((TObject *)frame->getObject(2), "FoM", "l");

        frame->Draw();
        legend.Draw();
        canvas.SaveAs(plotFile.c_str());
    }

    // Find cut maximizing FoM using TF1
    auto splineEval = [&](double *xx, double *) {
        x.setVal(xx[0]);
        return fomSpline.getVal();
    };

    TF1 fomFunction("fomFunction", splineEval, minScore, maxScore, 0);
    double bestCut = fomFunction.GetMaximumX();
    x.setVal(bestCut);

    double bestFoM = fomFunction.GetMaximum();
    double bestEff = effSpline.getVal();
    double bestPur = purSpline.getVal();

    std::cout << "[RESULT] Optimal Cut: " << bestCut
              << " | FoM: " << bestFoM
              << " | Efficiency: " << bestEff
              << " | Purity: " << bestPur << std::endl;

    // Log results into ROOT file if requested
    if (!resultsFile.empty()) {
        std::cout << "[INFO] Logging results to file: " << resultsFile << std::endl;
        std::unordered_map<std::string, double> logValues = {
            {"MaxCut", bestCut},
            {"Efficiency", bestEff},
            {"Purity", bestPur},
            {"FoM", bestFoM}
        };

        UpdateOrInsertByKey(resultsFile, resultsTree, "Method", mvaBranch, logValues);
    }

    return bestCut;
}
