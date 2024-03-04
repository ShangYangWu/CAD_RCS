#pragma once

#include <string>
#include <vector>

using namespace std;

enum class LogicGate { AND, OR, INV };

struct BlifNode {
  string output;
  vector<string> inputs;
  LogicGate gate;

  void print() const;
};

struct Parser {
  string ModelName;
  vector<string> inputs;
  vector<string> outputs;
  vector<BlifNode> nodes;

  void parse(const string &file);
  void print() const;
  vector<BlifNode> getAllNodes() { return nodes; }
};
