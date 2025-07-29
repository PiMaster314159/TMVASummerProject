#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <ROOT/RDataFrame.hxx>
#include <TSystem.h>

/// \struct MethodMetrics
/// Stores efficiency, purity, Figure of Merit (FoM), and associated statistical errors for an MVA method.
///
/// Members:
///
/// - efficiency: Signal efficiency.
///
/// - purity: Signal purity.
///
/// - fom: Figure of Merit (efficiency × purity).
///
/// - effErr: Binomial error in efficiency.
///
/// - purErr: Binomial error in purity.
///
/// - fomErr: Propagated error in FoM.
struct MethodMetrics {
    double efficiency = 0.0;
    double purity = 0.0;
    double fom = 0.0;
    double effErr = 0.0;
    double purErr = 0.0;
    double fomErr = 0.0;
};

////////////////////////////////////////////////////////////////////////////////
/// Compute performance metrics and binomial errors for a given energy bin.
///
/// ### Parameters:
/// \param[in] nSig         Number of signal events passing the cut.
/// \param[in] nBkg         Number of background events passing the cut.
/// \param[in] totalSignal  Total number of signal events in the bin.
///
/// ### Returns:
/// A MethodMetrics struct containing efficiency, purity, FoM, and statistical errors.
///
/// \note Efficiency and purity are computed as:
/// \f[ \text{eff} = \frac{n_{\text{sig}}}{\text{totalSignal}}, \quad
///     \text{pur} = \frac{n_{\text{sig}}}{n_{\text{sig}} + n_{\text{bkg}}} \f]
////////////////////////////////////////////////////////////////////////////////
inline MethodMetrics ComputeMetrics(double nSig, double nBkg, double totalSignal) {
    MethodMetrics metrics;

    metrics.efficiency = (totalSignal > 0) ? nSig / totalSignal : 0.0;
    metrics.purity = ((nSig + nBkg) > 0) ? nSig / (nSig + nBkg) : 0.0;
    metrics.fom = metrics.efficiency * metrics.purity;

    // Binomial errors
    metrics.effErr = (totalSignal > 0) ? sqrt(metrics.efficiency * (1 - metrics.efficiency) / totalSignal) : 0.0;
    metrics.purErr = ((nSig + nBkg) > 0) ? sqrt(metrics.purity * (1 - metrics.purity) / (nSig + nBkg)) : 0.0;
    metrics.fomErr = sqrt(pow(metrics.purity * metrics.effErr, 2) + pow(metrics.efficiency * metrics.purErr, 2));

    return metrics;
}

////////////////////////////////////////////////////////////////////////////////
/// Compute energy-binned performance metrics (Efficiency, Purity, FoM) for multiple MVA methods.
///
/// Reads "Signal" and "Background" TTrees from a ROOT file, divides events into energy bins,
/// and calculates efficiency, purity, and FoM for each MVA method within each bin.
/// Results, along with statistical errors, are stored in a TTree in an output ROOT file.
///
/// \param[in] inputFile        Path to the input ROOT file containing "Signal" and "Background" TTrees.
/// \param[in] outputFile       Path to the output ROOT file for storing computed metrics.
/// \param[in] methodCutValues  Map of method names to their optimal cut thresholds.
/// \param[in] energyBinEdges   Vector of bin boundaries for true energy [GeV].
///
/// \throws std::runtime_error If the input file does not exist, the Signal or Background trees are missing, or the energy bin list is invalid (less than 2 entries).
///
/// ### Output:
///
/// TTree named "data" with:
/// - Bin info: binMin, binMax, binMid, binCount.
/// - For each method: efficiency, purity, FoM, and associated errors.
///
////////////////////////////////////////////////////////////////////////////////
void CreateEnergyBinnedData(const std::string &inputFile,
                             const std::string &outputFile,
                             const std::unordered_map<std::string, double> &methodCutValues,
                             const std::vector<double> &energyBinEdges)
{
    std::cout << "Starting energy-binned performance computation with " << energyBinEdges.size() - 1 << " bins." << std::endl;

    // Validate input
    if (energyBinEdges.size() < 2) {
        throw std::runtime_error("Energy bin list must contain at least two entries.");
    }
    if (gSystem->AccessPathName(inputFile.c_str())) {
        throw std::runtime_error("Cannot access input ROOT file: " + inputFile);
    }

    ROOT::RDataFrame dfSignal("Signal", inputFile);
    ROOT::RDataFrame dfBackground("Background", inputFile);

    // Prepare output ROOT file and TTree
    std::unique_ptr<TFile> file(TFile::Open(outputFile.c_str(), "RECREATE"));
    if (!file || file->IsZombie()) {
        throw std::runtime_error("Cannot create output file: " + outputFile);
    }

    auto tree = std::make_unique<TTree>("data", "Energy-binned Performance Data");

    // Define common bin-level branches
    double binMin = 0, binMax = 0, binMid = 0, binCount = 0;
    tree->Branch("binMin", &binMin);
    tree->Branch("binMax", &binMax);
    tree->Branch("binMid", &binMid);
    tree->Branch("binCount", &binCount);

    // Container for metrics per method
    std::unordered_map<std::string, MethodMetrics> methodMetrics;

    // Dynamically create branches for each method
    for (const auto &m : methodCutValues) {
        const std::string &methodName = m.first;
        methodMetrics[methodName] = MethodMetrics();
        tree->Branch((methodName + "_eff").c_str(), &methodMetrics[methodName].efficiency);
        tree->Branch((methodName + "_eff_err").c_str(), &methodMetrics[methodName].effErr);
        tree->Branch((methodName + "_pur").c_str(), &methodMetrics[methodName].purity);
        tree->Branch((methodName + "_pur_err").c_str(), &methodMetrics[methodName].purErr);
        tree->Branch((methodName + "_fom").c_str(), &methodMetrics[methodName].fom);
        tree->Branch((methodName + "_fom_err").c_str(), &methodMetrics[methodName].fomErr);
    }

    // Loop through energy bins
    for (size_t i = 0; i < energyBinEdges.size() - 1; i++) {
        binMin = energyBinEdges[i];
        binMax = energyBinEdges[i + 1];
        binMid = 0.5 * (binMin + binMax);

        // Filter events within current energy bin
        auto sigBin = dfSignal.Filter(Form("TrueNuE >= %f && TrueNuE < %f", binMin, binMax));
        auto bkgBin = dfBackground.Filter(Form("TrueNuE >= %f && TrueNuE < %f", binMin, binMax));

        double nSigTotal = double(*sigBin.Count());
        double nBkgTotal = double(*bkgBin.Count());
        binCount = nSigTotal + nBkgTotal;

        std::cout << "Bin [" << binMin << ", " << binMax << "] | Signal: " << nSigTotal
                  << " | Background: " << nBkgTotal << std::endl;

        // Compute metrics for each method
        for (const auto &m : methodCutValues) {
            const std::string &methodName = m.first;
            double cut = m.second;

            double nSig = double(*sigBin.Filter(Form("%s > %f", methodName.c_str(), cut)).Count());
            double nBkg = double(*bkgBin.Filter(Form("%s > %f", methodName.c_str(), cut)).Count());

            methodMetrics[methodName] = ComputeMetrics(nSig, nBkg, nSigTotal);

            std::cout << "   [Method: " << methodName << "] Eff: " << methodMetrics[methodName].efficiency
                      << " | Pur: " << methodMetrics[methodName].purity
                      << " | FoM: " << methodMetrics[methodName].fom << std::endl;
        }

        tree->Fill();
    }

    file->cd();
    tree->Write();
    std::cout << "Metrics successfully written to: " << outputFile << std::endl;
}
