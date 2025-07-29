#include <string>
#include <iostream>
#include <TSystem.h>
#include <ROOT/RDataFrame.hxx>
#include <TCanvas.h>
#include <TStyle.h>
#include <TF2.h>

/// \enum ConfusionMatrixType
/// \brief Enumeration for selecting confusion matrix normalization mode.
enum class ConfusionMatrixType {
    Counts,     ///< Raw event counts (TP, FP, TN, FN)
    Efficiency, ///< Row-normalized by true class counts
    Purity      ///< Column-normalized by predicted class counts
};

////////////////////////////////////////////////////////////////////////////////
/// Create and save a confusion matrix for a binary classifier.
///
/// Computes TP, FP, TN, and FN from signal and background samples using an MVA method
/// and a given classification threshold. Displays the confusion matrix normalized according
/// to the selected mode:
///
/// - Counts: Raw event counts in each cell.
///
/// - Efficiency: Rows normalized by true class counts (signal/background).
///
/// - Purity: Columns normalized by predicted class counts.
///
/// The result is visualized as a 2x2 color-coded matrix with numeric values and saved as an image.
///
/// \param[in] inputFile    Path to the ROOT file containing "Signal" and "Background" TTrees.
/// \param[in] mvaBranch    Name of the MVA method branch (e.g., "BDT_base").
/// \param[in] outputDir    Directory for saving the confusion matrix image (must end with '/').
/// \param[in] optimalCut   Classification threshold on the MVA score.
/// \param[in] matrixType   Normalization mode (Counts, Efficiency, or Purity).
///
/// \throws std::runtime_error If the input file does not exist or TTrees cannot be accessed.
///
/// \note Requires ROOT graphics (TCanvas, TH2F) and RDataFrame.
/// \note Uses `kCool` palette for color coding.
///
////////////////////////////////////////////////////////////////////////////////
void CreateConfusionMatrix(const std::string &inputFile,
                           const std::string &mvaBranch,
                           const std::string &outputDir,
                           double optimalCut,
                           ConfusionMatrixType matrixType)
{
    std::cout << "Generating confusion matrix for method: " << mvaBranch
              << " | Type: " << (matrixType == ConfusionMatrixType::Counts ? "Counts" :
                                 matrixType == ConfusionMatrixType::Efficiency ? "Efficiency" : "Purity")
              << std::endl;

    // Validate input file
    if (gSystem->AccessPathName(inputFile.c_str())) {
        throw std::runtime_error("Input ROOT file cannot be accessed: " + inputFile);
    }

    // Load signal and background TTrees into RDataFrames
    ROOT::RDataFrame signalDF("Signal", inputFile);
    ROOT::RDataFrame backgroundDF("Background", inputFile);

    // Compute counts using optimal cut
    std::cout << "Computing classification counts..." << std::endl;
    double tp = static_cast<double>(*signalDF.Filter(mvaBranch + ">" + std::to_string(optimalCut)).Count()); // True Positives
    double totalSignal = static_cast<double>(*signalDF.Count());
    double fn = totalSignal - tp; // False Negatives

    double fp = static_cast<double>(*backgroundDF.Filter(mvaBranch + ">" + std::to_string(optimalCut)).Count()); // False Positives
    double totalBackground = static_cast<double>(*backgroundDF.Count());
    double tn = totalBackground - fp; // True Negatives

    if (totalSignal == 0 || totalBackground == 0) {
        throw std::runtime_error("Signal or Background dataset is empty. Cannot build confusion matrix.");
    }

    std::cout << "TP: " << tp << " | FN: " << fn << " | FP: " << fp << " | TN: " << tn << std::endl;

    // Normalize values based on selected type
    double normTP = tp, normFN = fn, normFP = fp, normTN = tn;
    std::string titleSuffix;

    if (matrixType == ConfusionMatrixType::Efficiency) {
        // Normalize rows by true class totals
        normTP = (totalSignal > 0) ? tp / totalSignal : 0.0;
        normFN = (totalSignal > 0) ? fn / totalSignal : 0.0;
        normFP = (totalBackground > 0) ? fp / totalBackground : 0.0;
        normTN = (totalBackground > 0) ? tn / totalBackground : 0.0;
        titleSuffix = " (Efficiency)";
    }
    else if (matrixType == ConfusionMatrixType::Purity) {
        // Normalize columns by predicted class totals
        double predSignalTotal = tp + fp;
        double predBackgroundTotal = tn + fn;
        normTP = (predSignalTotal > 0) ? tp / predSignalTotal : 0.0;
        normFP = (predSignalTotal > 0) ? fp / predSignalTotal : 0.0;
        normFN = (predBackgroundTotal > 0) ? fn / predBackgroundTotal : 0.0;
        normTN = (predBackgroundTotal > 0) ? tn / predBackgroundTotal : 0.0;
        titleSuffix = " (Purity)";
    }
    else {
        titleSuffix = " (Counts)";
    }

    // Create a 2×2 histogram for confusion matrix
    TH2F confusionMatrix("confusionMatrix",
                         (mvaBranch + " Confusion Matrix" + titleSuffix + ";Predicted Class;True Class").c_str(),
                         2, 0, 2, 2, 0, 2);

    // Label axes
    confusionMatrix.GetXaxis()->SetBinLabel(1, "Signal");
    confusionMatrix.GetXaxis()->SetBinLabel(2, "Background");
    confusionMatrix.GetYaxis()->SetBinLabel(1, "Signal");
    confusionMatrix.GetYaxis()->SetBinLabel(2, "Background");

    // Fill normalized or raw values
    confusionMatrix.SetBinContent(1, 1, normTP); // True signal predicted as signal
    confusionMatrix.SetBinContent(1, 2, normFP); // Background predicted as signal
    confusionMatrix.SetBinContent(2, 1, normFN); // Signal predicted as background
    confusionMatrix.SetBinContent(2, 2, normTN); // Background predicted as background

    // Visualization settings
    TCanvas canvas("confusionCanvas", "Confusion Matrix", 1200, 800);
    canvas.SetLeftMargin(0.15);

    gStyle->SetOptStat(0);
    gStyle->SetPalette(kCool);
    gStyle->SetTextSize(0.05);
    gStyle->SetPaintTextFormat((matrixType == ConfusionMatrixType::Counts) ? "0.0f" : "0.2f");

    confusionMatrix.SetMarkerSize(2.5);
    confusionMatrix.Draw("COLZ TEXT");

    // Save plot
    std::string fileSuffix = (matrixType == ConfusionMatrixType::Counts) ? "_counts" :
                              (matrixType == ConfusionMatrixType::Efficiency) ? "_eff" : "_pur";
    std::string outputPath = outputDir + mvaBranch + fileSuffix + "_cmat.png";

    canvas.SaveAs(outputPath.c_str());
    std::cout << "Confusion matrix saved to: " << outputPath << std::endl;
}
