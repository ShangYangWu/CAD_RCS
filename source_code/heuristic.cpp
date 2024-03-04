#include "heuristic.hpp"
#include <algorithm>
#include <iostream>
#include <list>
#include <queue>
#include <unordered_set>

using namespace std;

// void insert_node_to_mapping(const BlifNode &node,
//                             unordered_map<string, size_t> &mapping,
//                             size_t &max) {
//   // 初始化最大層級為 1
//   size_t max_level = 1;

//   // 獲取節點的輸入名稱列表
//   const auto &inputs = node.inputs;

//   // 遍歷節點的每個輸入
//   for (const auto &input : inputs) {
//     // 查找輸入名稱是否已經存在於映射中
//     auto it = mapping.find(input);
    
//     // 如果找到該輸入，更新最大層級為該輸入的層級 + 1
//     if (it != mapping.end()) {
//       max_level = std::max(max_level, it->second + 1);
//     }
//   }

//   // 更新整體最大層級
//   max = std::max(max, max_level);

//   // 將節點的輸出名稱和計算後的層級插入到映射中
//   mapping.emplace(node.output, max_level);
// }

// 讀入解讀好的資料做成Heuristic的資料結構
void Heuristic::parse(const vector<BlifNode> &all_nodes, const vector<string> &primary_input, const vector<string> &primary_ouput) {

  // 讀入 parse.input ( .inputs 中的元素 )
  PrimaryInputs = primary_input;
  PrimaryOutputs = primary_ouput;

  // {'node.output': node} 之後就可以用 node.output 來存取每個 node (ex. .names a d g => g node )
  for (const auto &node : all_nodes) {
    Mapping[node.output] = node;
  }
}

unordered_map<string, pair<size_t, vector<string>>> prepare_node_inputs_mapping( 
  const unordered_set<string> &PrimaryInputs,
  const unordered_map<string, BlifNode> &mapping) {

  unordered_map<string, pair<size_t, vector<string>>> node_in_out;

  for (const auto &pair : mapping) {
    auto &name = pair.first; // 設定指標變數name 指向 node.name
    auto &node = pair.second; // 設定指標變數node 指向 節點的BlifNode結構

    // 計算該節點的輸入節點數量，篩選掉主要輸入（PrimaryInputs）。
    auto input_nums = count_if(
        node.inputs.begin(), node.inputs.end(), [&](const auto &n) {
          return PrimaryInputs.find(n) == PrimaryInputs.end(); // 沒有 PrimaryInputs 則返回 true
        });

    // 將 扣除PrimaryInputs之後的input數量 存入 size_t的位置。
    node_in_out[name].first = input_nums; 

    // 遍歷該節點的輸入節點，並排除 PrimaryInputs。
    for (const auto &input : node.inputs) {
      if (PrimaryInputs.find(input) == PrimaryInputs.end()) {
        // 如果不是 PrimaryInputs，添加到 vector<string> 中。
        node_in_out[input].second.push_back(name);
      }
    }
  }

  // 返回建立好的節點輸入映射。
  return node_in_out;
}


void try_insert_the_node(
  const string &n, 
  vector<string> &one_level, 
  int &con, 
  unordered_map<string, pair<size_t, vector<string>>> &node_in_out,
  queue<string> &second_queue) {
  if (con == 0) {
    // 如果 constraint 已達到，將節點加到 second_queue 中
    second_queue.push(n);
    return;
  }
  con--; // 減少剩餘的constraint
  one_level.emplace_back(n); // 加節點到當前層級（one_level）
  for (const auto &input : node_in_out[n].second) {
    if (0 == --node_in_out[input].first) {
      // 如果這些輸入節點的未處理輸出數變為0，則將它們添加到下一個隊列
      second_queue.push(input);
    }
  }
}

vector<array<string, 3>> Heuristic::run(int and_con, int or_con, int inv_con) {
  
  // 創建一個集合 Placed，用於跟蹤已經安排的輸入節點
  unordered_set<string> Placed;
  for (const auto &input : PrimaryInputs) {
    Placed.emplace(input);
  }
  
  // 使用 prepare_node_inputs_mapping 函數來獲得節點的輸入數量和輸入關係的映射
  auto input_num_of_the_node = prepare_node_inputs_mapping(Placed, Mapping);

  // 定義一個自定義比較函數 cmp，原則 size 大的排前面 ( lefthand > righthand )
  auto cmp = [&](const string &lhs, const string &rhs) {
    return Mapping[lhs].inputs.size() < Mapping[rhs].inputs.size();
  };


  // 創建一個priority_queue，並使用 cmp 作為排序規則
  priority_queue<string, vector<string>, decltype(cmp)> candidates{cmp};

  // 將那些沒有input的節點添加到候選隊列中
  for (const auto &pair : input_num_of_the_node) {
    if (pair.second.first == 0) {
      candidates.push(pair.first);
    }
  }


  queue<string> second_queue;
  int ac = and_con;
  int oc = or_con;
  int ic = inv_con;
  vector<string> one_level;
  vector<vector<string>> ans;

  // candidates 和 second_queue( 暫存node用的buffer queue ) 皆不為空
  while (!(candidates.empty() && second_queue.empty())) {
    if (candidates.empty() || (ac == 0 && oc == 0 && ic == 0)) {
      ac = and_con;
      oc = or_con;
      ic = inv_con;
      ans.emplace_back(move(one_level));
      decltype(one_level){}.swap(one_level);
      while (!second_queue.empty()) {
        candidates.push(second_queue.front());
        second_queue.pop();
      }
    }

    // n 為 size 最大的 node
    auto n = candidates.top();
    candidates.pop();

    // 把 n 試著插入到 one_level，若放不下就暫存在 second_queue
    const auto &blifnode = Mapping[n];
    if (blifnode.gate == LogicGate::AND) {
      try_insert_the_node(n, one_level, ac, input_num_of_the_node, second_queue);
    } else if (blifnode.gate == LogicGate::OR) {
      try_insert_the_node(n, one_level, oc, input_num_of_the_node, second_queue);
    } else if (blifnode.gate == LogicGate::INV) {
      try_insert_the_node(n, one_level, ic, input_num_of_the_node, second_queue);
    }
  }

  // 把 one_level 中的資料移動到 ans 中
  ans.emplace_back(move(one_level));

  // 讓記憶體預留空間，減少分配與釋放的時間
  vector<array<string, 3>> ret;
  ret.reserve(ans.size());


  for (const auto &a : ans) {
    array<string, 3> one_level_ret;
    for (const auto &node : a) {
      // 根據節點的邏輯閘類型，將節點名稱 "node" 添加到 "one_level_ret" 的適當位置
      one_level_ret[static_cast<size_t>(Mapping[node].gate)] += node;
      one_level_ret[static_cast<size_t>(Mapping[node].gate)] += " ";
    }

    // 去掉每個邏輯閘類型的末尾空格
    for (auto &gate_info : one_level_ret) {
      if (!gate_info.empty()) {
        gate_info.resize(gate_info.size() - 1);
      }
    }

    // 將組合好的 "one_level_ret" 添加到 "ret" 向量中
    ret.emplace_back(move(one_level_ret));
  }
  return ret;
}