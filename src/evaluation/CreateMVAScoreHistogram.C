#include <string>
#include <iostream>
#include <ROOT/RDataFrame.hxx>
#include <TCanvas.h>
#include <TLegend.h>
#include <string>
#include <TStyle.h>
#include <TSystem.h>
/// \enum AxisScale
/// \brief Defines axis scaling options for histogram visualization.
enum class AxisScale {
    Linear, ///< Both axes use linear scale
    LogY    ///< Y-axis uses logarithmic scale
};

////////////////////////////////////////////////////////////////////////////////
/// \brief Create overlaid histograms of MVA scores for Signal and Background classes.
///
/// This function reads "Signal" and "Background" TTrees from an input ROOT file
/// and generates overlaid histograms for a specified MVA score branch.
/// The result is saved as a PNG file with configurable axis scaling.
///
/// Features:
///
/// - Plots Signal and Background distributions for the given MVA method.
///
/// - Supports linear or logarithmic Y-axis scaling.
///
/// - Outputs a PNG file in the specified directory.
///
/// ### Parameters:
/// \param[in] inputFile    Path to the input ROOT file containing "Signal" and "Background" TTrees.
/// \param[in] outputDir    Directory where output plots will be saved (must end with '/').
/// \param[in] mvaBranch    Name of the branch representing the MVA score (e.g., "BDT", "MLP").
/// \param[in] nBins        Number of histogram bins (must be > 0).
/// \param[in] histMin      Minimum value for histogram x-axis range.
/// \param[in] histMax      Maximum value for histogram x-axis range.
/// \param[in] axisScale    Axis scaling mode: AxisScale::Linear or AxisScale::LogY (default = Linear).
///
/// ### Throws:
/// \throws std::runtime_error If the input file does not exist, the output directory is empty, or nBins <= 0.
///
////////////////////////////////////////////////////////////////////////////////
void CreateMVAScoreHistogram(const std::string &inputFile,
                              const std::string &outputDir,
                              const std::string &mvaBranch,
                              int nBins,
                              double histMin,
                              double histMax,
                              AxisScale axisScale = AxisScale::Linear)
{
    std::cout << "Generating histogram overlay for method: " << mvaBranch
              << " | Axis Scale: " << (axisScale == AxisScale::Linear ? "Linear" : "LogY") << std::endl;

    // Validate input parameters
    if (nBins <= 0) {
        throw std::runtime_error("Number of bins must be greater than zero.");
    }
    if (outputDir.empty()) {
        throw std::runtime_error("Output directory cannot be empty.");
    }
    if (gSystem->AccessPathName(inputFile.c_str())) {
        throw std::runtime_error("Input ROOT file cannot be accessed: " + inputFile);
    }

    // Load Signal and Background TTrees using RDataFrame
    ROOT::RDataFrame dfSignal("Signal", inputFile);
    ROOT::RDataFrame dfBackground("Background", inputFile);

    // Debug: Print score ranges
    auto sigMin = dfSignal.Min(mvaBranch).GetValue();
    auto sigMax = dfSignal.Max(mvaBranch).GetValue();
    auto bkgMin = dfBackground.Min(mvaBranch).GetValue();
    auto bkgMax = dfBackground.Max(mvaBranch).GetValue();

    std::cout << "Signal Score Range: [" << sigMin << ", " << sigMax << "]" << std::endl;
    std::cout << "Background Score Range: [" << bkgMin << ", " << bkgMax << "]" << std::endl;

    // Create canvas for plotting
    TCanvas canvas("canvas", "MVA Score Distributions", 1200, 800);
    canvas.SetLeftMargin(0.15);

    // Create and style Signal histogram
    auto hSignal = dfSignal.Histo1D(
        {"SignalHist", (mvaBranch + " Score").c_str(), nBins, histMin, histMax},
        mvaBranch.c_str()
    );
    hSignal->SetLineColor(kBlue);
    hSignal->SetFillColor(kAzure - 4);
    hSignal->GetXaxis()->SetTitle((mvaBranch + " Score").c_str());
    hSignal->GetYaxis()->SetTitle("# of Events");
    hSignal->Draw();

    // Create and style Background histogram
    auto hBackground = dfBackground.Histo1D(
        {"BackgroundHist", (mvaBranch + " Score").c_str(), nBins, histMin, histMax},
        mvaBranch.c_str()
    );
    hBackground->SetLineColor(kRed);
    hBackground->SetFillColor(kRed);
    hBackground->SetFillStyle(3004);
    hBackground->Draw("same");

    // Adjust Y-axis range for both histograms
    double maxY = std::max(hSignal->GetMaximum(), hBackground->GetMaximum()) * 1.2;
    hSignal->SetMaximum(maxY);

    // Create legend
    TLegend legend(0.70, 0.75, 0.88, 0.88);
    legend.SetBorderSize(0);
    legend.SetFillStyle(0);
    legend.AddEntry(hSignal.GetPtr(), "Signal", "f");
    legend.AddEntry(hBackground.GetPtr(), "Background", "f");
    legend.Draw();

    gStyle->SetOptStat(0);

    // Apply axis scaling if requested
    if (axisScale == AxisScale::LogY) {
        canvas.SetLogy();
    }

    // Generate output filename based on axis scale
    std::string scaleSuffix = (axisScale == AxisScale::LogY) ? "_logy" : "_linear";
    std::string outputPath = outputDir + mvaBranch + "_scoreOverlay" + scaleSuffix + ".png";

    std::cout << "Saving plot to: " << outputPath << std::endl;
    canvas.SaveAs(outputPath.c_str());

    std::cout << "Histogram overlay generation complete." << std::endl;
}
