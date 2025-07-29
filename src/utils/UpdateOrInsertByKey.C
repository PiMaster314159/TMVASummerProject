#include <iostream>
#include <string>
#include <unordered_map>
#include <TFile.h>
#include <TTree.h>

////////////////////////////////////////////////////////////////////////////////
/// Update or insert an entry in a ROOT TTree using a string key.
///
/// This function opens (or creates) a ROOT file and updates the specified TTree.
///
/// If the tree exists:
///
///   - Copies all existing entries into a new TTree.
///
///   - Updates the row where `keyBranch == keyValue` with the provided branch values.
///
///   - If no matching key is found, appends a new entry.
///
///   - Replaces the old tree with the updated version.
///
/// If the tree does not exist:
///
///   - Creates a new TTree with one entry containing `keyValue` and the provided values.
///
/// Features:
///
/// - Handles dynamic branches based on the `values` map.
///
/// - Uses `std::unordered_map` for flexibility in branch names.
///
/// - Maintains backward compatibility by copying existing rows.
///
/// \param[in] filePath       Path to the ROOT file to update or create.
/// \param[in] treeName       Name of the TTree to update or create.
/// \param[in] keyBranch      Name of the branch acting as a unique key (string).
/// \param[in] keyValue       Value for the key branch.
/// \param[in] values         Map of branch names to their (double) values to update or insert.
///
/// \throws std::runtime_error If the ROOT file cannot be opened for writing.
///
/// \note If the tree exists, the original is replaced with an updated version.
///
////////////////////////////////////////////////////////////////////////////////
void UpdateOrInsertByKey(const std::string &filePath,
                         const std::string &treeName,
                         const std::string &keyBranch,
                         const std::string &keyValue,
                         const std::unordered_map<std::string, double> &values)
{
    // Open or create the ROOT file in update mode
    TFile file(filePath.c_str(), "UPDATE");
    if (!file.IsOpen()) {
        throw std::runtime_error("Error: Cannot open ROOT file: " + filePath);
    }

    // Attempt to retrieve the tree; if not found, create a new one
    TTree *tree = file.Get<TTree>(treeName.c_str());
    if (!tree) {
        std::cout << "Tree not found. Creating a new tree: " << treeName << std::endl;
        tree = new TTree(treeName.c_str(), "Auto-created tree");
    }

    // Holders for writing/updating
    std::string keyHolder;
    std::unordered_map<std::string, double> branchValuesHolder;

    // Bind branches for writing
    tree->Branch(keyBranch.c_str(), &keyHolder);
    for (const auto &kv : values) {
        branchValuesHolder[kv.first] = 0.0; // Initialize to zero
        tree->Branch(kv.first.c_str(), &branchValuesHolder[kv.first]);
    }

    bool entryUpdated = false;

    if (tree->GetEntries() > 0) { // If tree already has data
        std::cout << "Tree exists. Reading and updating entries..." << std::endl;

        // Holders for reading old entries
        std::string currentKey;
        std::unordered_map<std::string, double> currentValuesHolder = branchValuesHolder;

        // Set addresses for reading
        std::string *keyPtr = &currentKey;
        tree->SetBranchAddress(keyBranch.c_str(), &keyPtr);
        for (auto &kv : currentValuesHolder) {
            tree->SetBranchAddress(kv.first.c_str(), &kv.second);
        }

        // Create a temporary tree for updated data
        TTree *updatedTree = new TTree("tmpTree", "Updated tree");
        updatedTree->Branch(keyBranch.c_str(), &keyHolder);
        for (auto &kv : branchValuesHolder) {
            updatedTree->Branch(kv.first.c_str(), &kv.second);
        }

        // Iterate through old entries and copy them, updating if needed
        for (Long64_t i = 0; i < tree->GetEntries(); ++i) {
            tree->GetEntry(i);

            // Copy current row
            keyHolder = currentKey;
            for (auto &kv : branchValuesHolder) {
                kv.second = currentValuesHolder[kv.first];
            }

            // If key matches, update values
            if (currentKey == keyValue) {
                std::cout << "Updating entry for key: " << keyValue << std::endl;
                for (const auto &v : values) {
                    branchValuesHolder[v.first] = v.second;
                }
                entryUpdated = true;
            }

            updatedTree->Fill();
        }

        // If no matching key found, append a new row
        if (!entryUpdated) {
            std::cout << "Adding new entry for key: " << keyValue << std::endl;
            keyHolder = keyValue;
            for (const auto &v : values) {
                branchValuesHolder[v.first] = v.second;
            }
            updatedTree->Fill();
        }

        // Replace old tree in the ROOT file
        updatedTree->SetDirectory(&file);
        updatedTree->Write(treeName.c_str(), TObject::kOverwrite);

    } else { // If tree is new
        std::cout << "Creating first entry for new tree..." << std::endl;
        keyHolder = keyValue;
        for (const auto &v : values) {
            branchValuesHolder[v.first] = v.second;
        }
        tree->Fill();
        tree->Write();
    }

    file.Close();
    std::cout << (entryUpdated ? "Updated entry for key: " : "Added entry for key: ")
              << keyBranch << " = " << keyValue << std::endl;
}
