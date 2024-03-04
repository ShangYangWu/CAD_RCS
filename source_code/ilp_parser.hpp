#pragma once

#include <string>
#include <vector>
#include <fstream>

using namespace std;

// enum class LogicGate { AND, OR, INV };

struct IlpNode {
  string ilp_node;
  size_t level;
};

struct IlpParser {
  string SolFile;
  vector<IlpNode> ilp_nodes;
  size_t max_level;

  void ilpParse(const string file);
  vector<IlpNode> getAllIlpNodes() { return ilp_nodes; }
};
