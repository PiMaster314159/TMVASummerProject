#include "../application/TMVAReaderWrapper.C"
#include <filesystem>

void ReadData(){
    TMVAReaderWrapper reader;
    std::vector<std::string> variables = {"CVNScoreNuE", "CVNScoreNuMu", "CVNScoreNC"};
    std::vector<std::string> spectators = {"TrueNuE"};
    std::string methodSuffix = "demo"; // Unique identifier
    std::string outDir =  "output/demo/";
    for (const auto &var : variables) reader.AddVariable(var);
    for (const auto &spec : spectators) reader.AddSpectator(spec);
    std::string relativePath = outDir + "models/TMVAClassification_BDT_AdaBoost_" + methodSuffix + ".weights.xml";
    // std::string absolutePath = std::filesystem::absolute(relativePath).string();
    std::string basePath = gSystem->WorkingDirectory(); // e.g., /mnt/c/.../TMVASummerProject
    std::string absolutePath = basePath+"/"+outDir + "models/weights/TMVAClassification_BDT_AdaBoost_" + methodSuffix + ".weights.xml";

    std::cout<< absolutePath<<std::endl;
    reader.BookMethod("BDT_AdaBoost_" + methodSuffix, absolutePath);
}
