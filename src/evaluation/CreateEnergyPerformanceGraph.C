#include <string>
#include <unordered_map>
#include <ROOT/RDataFrame.hxx>
#include <TSystem.h>
#include <TCanvas.h>
#include <TMultiGraph.h>
#include <TLegend.h>
#include <TGraphErrors.h>
#include <TPad.h>

/// \enum GraphType
/// Specifies which performance metric to visualize as a function of energy.
///
/// Options:
///
/// - Efficiency: Efficiency vs energy.
///
/// - Purity: Purity vs energy.
///
/// - FoM: Figure of Merit vs energy.
enum class GraphType {
    Efficiency,
    Purity,
    FoM
};

////////////////////////////////////////////////////////////////////////////////
/// Create an energy-binned performance graph for multiple MVA methods.
///
/// This function reads a ROOT TTree containing precomputed performance metrics and plots
/// a graph of the chosen metric (Efficiency, Purity, or Figure of Merit) versus true
/// neutrino energy. Each method is represented as a line with error bars.
///
/// ### Features:
/// - Reads bin midpoints and metric values from a TTree.
/// - Plots multiple methods on a single TMultiGraph.
/// - Adds error bars for each metric point.
/// - Saves the graph as a PNG image.
///
/// ### Parameters:
/// \param[in] inputFile     Path to the ROOT file containing the "data" TTree with metrics.
/// \param[in] methodColors  Map of method names to ROOT color codes for plotting.
/// \param[in] outputFile    Path where the PNG image will be saved.
/// \param[in] graphType     Metric type to plot (GraphType::Efficiency, GraphType::Purity, GraphType::FoM).
///
/// ### Throws:
/// \throws std::runtime_error If:
/// - The input file does not exist.
/// - The "data" TTree is empty.
/// - The selected metric columns are missing.
///
/// ### Output:
/// PNG image saved at `outputFile`.
///
/// \note Requires ROOT (TGraphErrors, TCanvas, TMultiGraph) and RDataFrame.
///
////////////////////////////////////////////////////////////////////////////////
void CreateEnergyPerformanceGraph(const std::string &inputFile,
                                  const std::unordered_map<std::string, Color_t> &methodColors,
                                  const std::string &outputFile,
                                  GraphType graphType)
{
    std::cout << "Generating energy performance graph..." << std::endl;

    // Validate input file
    if (gSystem->AccessPathName(inputFile.c_str())) {
        throw std::runtime_error("Cannot access input ROOT file: " + inputFile);
    }

    // Load the TTree with metrics
    ROOT::RDataFrame df("data", inputFile);
    int nBins = int(*df.Count());
    if (nBins <= 0) {
        throw std::runtime_error("The TTree 'data' is empty or missing in file: " + inputFile);
    }

    auto binCenters = *df.Take<double>("binMid");
    std::vector<double> xErrors(nBins, 0.0); // No horizontal error bars

    // Determine which metric to plot
    std::string metricSuffix;
    std::string axisTitle;
    switch (graphType) {
        case GraphType::Efficiency:
            metricSuffix = "_eff";
            axisTitle = "Efficiency";
            break;
        case GraphType::Purity:
            metricSuffix = "_pur";
            axisTitle = "Purity";
            break;
        case GraphType::FoM:
            metricSuffix = "_fom";
            axisTitle = "Figure of Merit";
            break;
    }

    // Setup canvas and graph container
    TCanvas canvas("canvas", "Energy Performance Graph", 1200, 800);
    canvas.SetGrid();

    TMultiGraph multiGraph;

    TLegend legend(0.10, .9, 0.95, .98); // full width, just above plot - adjust height if needed
    legend.SetNColumns(5); // five entries per row
    legend.SetTextSize(0.035);
    legend.
    legend.SetMargin(0.3);
    legend.SetColumnSeparation(0.03);       // spacing between columns
    legend.SetEntrySeparation(0.025);

    // Build graphs for each method
    for (const auto &methodPair : methodColors) {
        const std::string &methodName = methodPair.first;
        Color_t color = methodPair.second;

        // Read metric values and errors for this method
        auto yValues = *df.Take<double>(methodName + metricSuffix);
        auto yErrors = *df.Take<double>(methodName + metricSuffix + "_err");

        if (yValues.size() != static_cast<size_t>(nBins)) {
            throw std::runtime_error("Missing metric values for method: " + methodName);
        }

        auto graph = new TGraphErrors(nBins,
                                      binCenters.data(), yValues.data(),
                                      xErrors.data(), yErrors.data());
        graph->SetLineColor(color);
        graph->SetMarkerColor(color);
        graph->SetMarkerStyle(21);
        graph->SetMarkerSize(1.2);
        gPad->SetTickx(); gPad->SetTicky();

        multiGraph.Add(graph);
        // legend.AddEntry(graph, methodName.c_str(), "lp");
        std::string delimiter = "_N";
        std::string updatedName = methodName.substr(0, methodName.find(delimiter));
        if(!methodName.find("BDT_GradBoost")){
            legend.AddEntry(graph, ("#bf{"+updatedName+"}").c_str(), "lp");
            graph->SetLineWidth(3);
        } else {
            legend.AddEntry(graph, updatedName.c_str(), "lp");
            graph->SetLineWidth(1);
        }
    }

    // Set margins first
    // canvas.SetLeftMargin(0.18);
    canvas.SetBottomMargin(0.15);
    canvas.SetTopMargin(0.1);
    canvas.SetRightMargin(0.05);

    // Draw multiGraph
    multiGraph.Draw("ALP");

    // Axis styling
    multiGraph.GetXaxis()->SetTickLength(0.02);
    multiGraph.GetYaxis()->SetTickLength(0.02);
    // multiGraph.GetXaxis()->SetLabelOffset(0.02);
    // multiGraph.GetYaxis()->SetLabelOffset(0.02);

    multiGraph.GetXaxis()->SetTitle("True Neutrino Energy [GeV]");
    multiGraph.GetYaxis()->SetTitle(("Signal " + axisTitle).c_str());

    multiGraph.GetXaxis()->SetLabelSize(0.04);
    multiGraph.GetYaxis()->SetLabelSize(0.04);
    multiGraph.GetXaxis()->SetTitleSize(0.05);
    multiGraph.GetYaxis()->SetTitleSize(0.05);
    multiGraph.GetYaxis()->SetTitleOffset(1);

    multiGraph.GetYaxis()->SetRangeUser(0.2, 1.0);

    // legend.SetFillStyle(0);
    // legend.SetBorderSize(0);
    legend.Draw();

    // Apply changes
    canvas.Modified();
    canvas.Update();

    // Save plot
    canvas.SaveAs(outputFile.c_str());
    std::cout << "Energy performance graph saved to: " << outputFile << std::endl;
}
