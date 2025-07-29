#include <TMVA/Tools.h>
#include <TMVA/Reader.h>
#include <ROOT/RDataFrame.hxx>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <TSystem.h>

////////////////////////////////////////////////////////////////////////////////
/// \class TMVAReaderWrapper
/// \brief Manages a TMVA Reader for efficient and reusable MVA evaluation.
///
/// Features:
///
/// - Initialize a TMVA Reader instance with support for multiple MVA methods.
///
/// - Add input variables and spectator variables dynamically.
///
/// - Evaluate individual events after setting variable values.
///
/// - Apply trained models to entire ROOT TTrees using RDataFrame.
///
/// - Encapsulates all TMVA::Reader logic for streamlined usage.
///
////////////////////////////////////////////////////////////////////////////////
class TMVAReaderWrapper {
private:
    std::unique_ptr<TMVA::Reader> reader; ///< TMVA Reader instance
    std::unordered_map<std::string, float> variables; ///< Map of input variables and values
    std::unordered_map<std::string, float> spectators; ///< Map of spectator variables and values

public:
    /// Constructor: Initializes TMVA tools and the Reader instance.
    TMVAReaderWrapper() {
        TMVA::Tools::Instance();
        reader = std::make_unique<TMVA::Reader>("Color:Silent");
    }

    /// Add an input variable to the TMVA Reader.
    /// \param[in] name Name of the variable.
    void AddVariable(const std::string &name) {
        if (variables.find(name) != variables.end()) {
            std::cerr << "Variable '" << name << "' is already registered!" << std::endl;
            return;
        }
        variables[name] = 0.0f; // Initialize storage
        reader->AddVariable(name, &variables[name]);
    }

    /// Add a spectator variable (not used in training but monitored).
    /// \param[in] name Name of the spectator variable.
    void AddSpectator(const std::string &name) {
        if (spectators.find(name) != spectators.end()) {
            std::cerr << "Spectator '" << name << "' is already registered!" << std::endl;
            return;
        }
        spectators[name] = 0.0f;
        reader->AddSpectator(name, &spectators[name]);
    }

    /// Book an MVA method and associate it with its weight file.
    /// \param[in] methodName Name of the MVA method (e.g., "BDT").
    /// \param[in] weightFile Path to the XML weight file.
    /// \throws std::runtime_error If the weight file cannot be accessed.
    void BookMethod(const std::string &methodName, const std::string &weightFile) {
        if (gSystem->AccessPathName(weightFile.c_str())) {
            throw std::runtime_error("Weight file not found: " + weightFile);
        }
        reader->BookMVA(methodName, weightFile);
    }

    /// Set a variable value for evaluation.
    /// \param[in] name Name of the variable.
    /// \param[in] value Value to assign.
    void SetVariableValue(const std::string &name, float value) {
        auto it = variables.find(name);
        if (it != variables.end()) {
            it->second = value;
        } else {
            std::cerr << "Attempted to set unregistered variable '" << name << "'!" << std::endl;
        }
    }

    /// Evaluate an MVA method using the currently set variable values.
    /// \param[in] methodName Name of the booked MVA method.
    /// \return The MVA score as a double.
    double Evaluate(const std::string &methodName) {
        return reader->EvaluateMVA(methodName);
    }

    /// Apply the MVA method to an entire ROOT TTree and save results.
    ///
    /// Uses ROOT RDataFrame to define a new branch indicating whether each event
    /// passes the classification cut for the selected method.
    ///
    /// \param[in] inputFile  Path to input ROOT file.
    /// \param[in] treeName   Name of the TTree in the input file.
    /// \param[in] methodName Name of the booked MVA method.
    /// \param[in] outputFile Path to save the modified ROOT file.
    /// \param[in] optCut     Optimal cut threshold on the MVA score.
    /// \param[in] varNames   Names of variables used in the evaluation.
    ///
    /// \throws std::runtime_error If input file cannot be accessed.
    ///
    void ApplyToTree(const std::string &inputFile,
                     const std::string &treeName,
                     const std::string &methodName,
                     const std::string &outputFile,
                     double optCut,
                     const std::vector<std::string> &varNames) {
        if (gSystem->AccessPathName(inputFile.c_str())) {
            throw std::runtime_error("Cannot access input ROOT file: " + inputFile);
        }

        ROOT::RDataFrame df(treeName, inputFile);

        // Define a new branch for MVA classification result
        auto dfWithMVA = df.Define(methodName + "_output",
                                   [this, &methodName, optCut, varNames](float a, float b, float c) {
                                       // Assign values to variables dynamically
                                       for (size_t i = 0; i < varNames.size(); i++) {
                                           float val = (i == 0) ? a : (i == 1) ? b : c;
                                           this->SetVariableValue(varNames[i], val);
                                       }
                                       double mvaScore = this->Evaluate(methodName);
                                       return (mvaScore > optCut) ? 1.0 : 0.0;
                                   },
                                   varNames);

        dfWithMVA.Snapshot(treeName, outputFile);
        std::cout << "Applied method '" << methodName
                  << "' to tree and saved results to: " << outputFile << std::endl;
    }
};