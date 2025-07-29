#include <string>
#include "../utils/FilterInputData.C"
void FilterDataExample(){
    std::string inputFile = "data/ana_tree_newmodel.root";
    std::string inputTreeName = "analysistree/atmoOutput";
    std::string outputFile = "data/filtered_data/filteredNuMu_complete.root";
    std::vector<std::string> branchesToKeep = {"CVNScoreNuE", "CVNScoreNuMu", "CVNScoreNC", "TrueNuE"};
    // std::string exclusionFilter = "CVNScoreNuE != -999";
    // std::string signalFilter = "(TrueNuPdg == 12 || TrueNuPdg == -12) && IsCC";
    
    // SplitTreeByFilter(inputFile, inputTreeName, outputFile, branchesToKeep, signalFilter, exclusionFilter);
    FilterInputData(inputFile, inputTreeName, outputFile, branchesToKeep, InteractionType::NuMu, kTRUE);
}