#pragma once

#include "algorithm.hpp"
#include "parser.hpp"
#include "ilp_parser.hpp"
#include <unordered_map>
using namespace std;


struct ilp : public Algorithm {
  unordered_map<string, BlifNode> Mapping; // 用於映射 BlifNode 的無序映射容器
  vector<string> PrimaryInputs; // 存儲主要輸入的向量
  vector<string> PrimaryOutputs;

  // 覆寫 Algorithm 結構中的 parse 函數
  void parse(const vector<BlifNode> &all_nodes, const vector<string> &primary_input, const vector<string> &primary_output) override;

  // 覆寫 Algorithm 結構中的 run 函數
  vector<array<string, 3>> run(int and_con, int or_con, int inv_con) override;

};