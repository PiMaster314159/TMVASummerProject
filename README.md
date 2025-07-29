# ROOT TMVA Neutrino Classification Pipeline

This repository provides a C++ pipeline for training and evaluating Multivariate Analysis (MVA) methods using ROOT’s [TMVA](https://root.cern.ch/tmva) toolkit.  
The pipeline is designed for binary classification of neutrino interactions (i.e., distinguishing between charged-current $\nu_e$.charged-current $\nu_\mu$, and neutral-current interactions) using CVN algorithm scores or other dataset features.

---

## Features
- Train multiple TMVA classifiers (e.g., MLP, BDT).
- Compute optimal FoM-based classifier score cut (Efficiency $\times$ Purity).
- Generate:
  - Confusion matrices
  - MVA signal vs background score histograms
  - Energy-binned efficiency/purity/FoM graphs
- Apply trained models to new data with TMVAReaderWrapper.
- Modular design for easy modification or creation of additional methods, features, or evaluation tools.

---

## Project Structure
```
TMVASummerProject/
│
├── src/
│   ├── application/
│   │    └── TMVAReaderWrapper.C            # Apply trained TMVA models to new data
│   ├── training/
│   │    └── TrainClassificationModel.C     # Train TMVA models
│   ├── evaluation/
│   │    ├── GetOptimalCut.C                # Compute optimal FoM-based cut
│   │    ├── CreateConfusionMatrix.C        # Confusion matrices
│   │    ├── CreateMVAScoreHistogram.C      # Score distribution plots
│   │    ├── CreateEnergyBinnedData.C       # Compute energy-binned metrics
│   │    └── CreateEnergyPerformanceGraph.C # Graph efficiency/purity/FoM vs energy
│   ├── utils/
│   │    ├── CreateTimestampedDir.C         # Generate a unique timestamped directory
│   │    ├── SplitTreeByFilter.C            # Split tree into Signal/Background
│   │    └── UpdateOrInsertByKey.C          # Log results in ROOT TTree
│   ├── examples/
│   │    ├── DemoPipeline.C                 # Full end-to-end workflow
│   │    ├── FilterDataExample.C            # Filter atmospheric neutrino data based on interaction type
│   │    └── DataGeneration.C               # 
│
├── data/
|   ├── example.root                        # Example input ROOT file
│   ├── filtered_data/
│   │   └── exampleFiltered.root            # Example filtered input ROOT File
│
├──  output/
|   ├── demo                                # Demo output files
│   │   ├── models/
│   │   │   ├── TMVAC.root                  # TMVA training diagnostics
│   │   │   ├── weights/                    # TMVA XML weight files
│   │   │   └── plots/                      # All generated plots
│   │   │       ├── *_FoM.png
│   │   │       ├── *_cmat.png
│   │   │       ├── *_scoreOverlay.png
│   │   │       └── EnergyVs*.png
│   │   ├── filtered.root                   # Data with classifier outputs
│   │   ├── energyBins.root                 # Energy-binned performance metrics
│   │   └── Signal_with_BDT.root            # Example of model application
```

---

## Requirements
- ROOT ≥ 6.24 (with TMVA)
- C++17 (or newer)
- Tested with gcc 13.3.0 on Ubuntu 22.04

---

## Build Instructions

### Build with g++
Compile all `.C` files into a single executable:
Compile all `.C` files into a single executable:
```bash
g++ src/evaluation/CreateConfusionMatrix.C \
    src/evaluation/CreateMVAScoreHistogram.C \
    src/evaluation/CreateEnergyBinnedData.C \
    src/evaluation/CreateEnergyPerformanceGraph.C \
    src/utils/SplitTreeByFilter.C \
    src/utils/UpdateOrInsertByKey.C \
    src/application/TMVAReaderWrapper.C \
    src/utils/CreateTimestampedDir.C \
    src/training/TrainClassificationModel.C \
    src/evaluation/GetOptimalCut.C \
    src/examples/DemoPipeline.C \
    -o DemoPipeline `root-config --cflags --libs` -lTMVA
```

Run:
```bash
./DemoPipeline data/example.root output/demo/
```

---

## Output Organization
The pipeline saves outputs under your specified directory (e.g., `output/demo/`). Recommended structure:
```
output/demo/
├── filtered.root
├── energyBins.root
├── models/
│   ├── TMVAC.root
│   ├── weights/
│   └── plots/
│       ├── *_FoM.png
│       ├── *_cmat.png
│       ├── *_scoreOverlay.png
│       └── EnergyVs*.png
└── Signal_with_BDT.root
```

---

## Automating Timestamped Runs
Use the utility macro:
```cpp
.L src/utils/CreateTimestampedDir.C+
std::string runDir = CreateTimestampedDir("output/runs/");
```

This ensures each run is stored in a unique folder:
```
output/runs/run_20250720_1425/
```

---

## Usage Workflow

### 1. Train Models
```cpp
TrainClassificationModel("demo", "data/input/example.root", "output/demo/", "filtered.root",
                         {"CVNScoreNuE", "CVNScoreNuMu", "CVNScoreNC"},
                         {"TrueNuE"},
                         { {TMVA::Types::kMLP, "MLP", "...options..."},
                           {TMVA::Types::kBDT, "BDT_AdaBoost", "...options..."} },
                         0.3); // train/test split ratio
```

### 2. Optimize Cut
```cpp
double cut = GetOptimalCut("output/demo/filtered.root", "MLP_demo", "output/demo/models/plots/MLP_demo_FoM.png");
```

### 3. Evaluate Performance
- Confusion Matrix:
```cpp
CreateConfusionMatrix("output/demo/filtered.root", "MLP_demo", "output/demo/models/plots/", cut, ConfusionMatrixType::Efficiency);
```

- Score Histogram:
```cpp
CreateMVAScoreHistogram("output/demo/filtered.root", "output/demo/models/plots/", "MLP_demo", 50, -1, 1, AxisScale::Linear);
```

- Energy-Binned Graphs:
```cpp
std::vector<double> bins = {0, 1, 2, 4, 6, 8, 10};
CreateEnergyBinnedData("output/demo/filtered.root", "output/demo/eBinData.root", {{"MLP_demo", cut}}, bins);
CreateEnergyPerformanceGraph("output/demo/eBinData.root", {{"MLP_demo", kRed}}, "output/demo/models/plots/eBin_eff.png", GraphType::Efficiency);
```

### 4. Apply Model to New Data
Using TMVAReaderWrapper:
```cpp
TMVAReaderWrapper reader;
reader.AddVariable("CVNScoreNuE");
reader.AddVariable("CVNScoreNuMu");
reader.AddVariable("CVNScoreNC");
reader.BookMethod("BDT_AdaBoost_demo", "output/demo/models/weights/TMVAClassification_BDT_AdaBoost_demo.weights.xml");

// Apply to tree
reader.ApplyToTree("output/demo/filtered.root", "Signal", "BDT_AdaBoost_demo", "output/demo/Signal_with_BDT.root", cut, {"CVNScoreNuE", "CVNScoreNuMu", "CVNScoreNC"});
```

---

## Visualize TMVA Training
```cpp
TMVA::TMVAGui("output/demo/models/TMVAC.root");
```
