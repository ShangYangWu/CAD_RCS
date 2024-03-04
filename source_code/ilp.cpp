// 先用ALAP找到每個node最晚要什麼時候開始
// 把ilp得到的的結果轉成ilp可以讀的數學模型，輸出檔為.mod
// glpsol --model {filename}.mod --output {filename}.sol
// 把 .sol解讀出來
// 生成指定格式
// gurobi
// gurobi_cl ResultFile=sol_file lp_file

#include "ilp.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <list>
#include <queue>
#include <unordered_set>


using namespace std;

namespace {
  // // 將一個包含空格分隔單詞的字串 line 分割成一個字串向量 split，並返回該向量。
  // vector<string> split(const string &line) {
  //   vector<string> split;
  //   stringstream ss;
  //   ss << line;
  //   string s;
  //   while (ss >> s) {
  //     // 在每次從流中讀取一個單詞後，使用 emplace_back 函數將該單詞添加到 split 向量中。
  //     // std::move(s) 的使用表示將 s 的內容移動到向量中
  //     split.emplace_back(move(s));
  //   }
  //   return split;
  // }
} // namespace

void insert_node__mapping(const BlifNode &node, unordered_map<string, size_t> &mapping, size_t &max) {
  // 初始化最大層級為 1
  size_t max_level = 1;

  // 獲取節點的輸入名稱列表
  const auto &inputs = node.inputs;

  // 遍歷節點的每個輸入
  for (const auto &input : inputs) {
    // 查找輸入名稱是否已經存在於映射中
    auto it = mapping.find(input);
    
    // 如果找到該輸入，更新最大層級為該輸入的層級 + 1
    if (it != mapping.end()) {
      max_level = std::max(max_level, it->second + 1);
    }
  }

  // 更新整體最大層級
  max = std::max(max, max_level);

  // 將節點的輸出名稱和計算後的層級插入到映射中
  mapping.emplace(node.output, max_level);
}

// 讀入解讀好的資料做成ilp的資料結構
void ilp::parse(const vector<BlifNode> &all_nodes, const vector<string> &primary_input, const vector<string> &primary_ouput) {

  // 讀入 parse.input ( .inputs 中的元素 )
  PrimaryInputs = primary_input;
  PrimaryOutputs = primary_ouput;

  // {'node.output': node} 之後就可以用 node.output 來存取每個 node (ex. .names a d g => g node )
  for (const auto &node : all_nodes) {
    Mapping[node.output] = node;
  }
}

unordered_map<string, pair<size_t, vector<string>>> prepare_node_input_mapping( 
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


void try_insert_node(
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

vector<array<string, 3>> ilp::run(int and_con, int or_con, int inv_con) {
  // 創建一個集合 Placed，用於跟蹤已經安排的輸入節點
  unordered_set<string> Placed;
  for (const auto &input : PrimaryInputs) {
    Placed.emplace(input);
  }
  
  // 使用 prepare_node_input_mapping 函數來獲得節點的輸入數量和輸入關係的映射
  auto input_num_of_the_node = prepare_node_input_mapping(Placed, Mapping);

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
      try_insert_node(n, one_level, ac, input_num_of_the_node, second_queue);
    } else if (blifnode.gate == LogicGate::OR) {
      try_insert_node(n, one_level, oc, input_num_of_the_node, second_queue);
    } else if (blifnode.gate == LogicGate::INV) {
      try_insert_node(n, one_level, ic, input_num_of_the_node, second_queue);
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

// 數學式 => 寫成output.lp檔(gurobi)
  ofstream file("output.lp"); // 打開一個名為 "output.lp" 的文件
  if (file.is_open()) {
    streambuf *coutbuf = cout.rdbuf();
    cout.rdbuf(file.rdbuf());

    // min = 1*lastgate_1 + 2*lastgate_2 + ... + maxlevel*lastgate_maxlevel
    cout << "Minimize\n";
    for (size_t i=1; i<=ret.size()+1; i++){
      if(i == ret.size()+1){
        cout << i << " end" << "(" << i << ")";
      }
      else{
        cout << i << " end" << "(" << i << ") + ";
      }
    }
    cout << "\n\nSubject To" << endl;

    // // 限制一: ALAP限制上限
    size_t condition_count = 0;
    const size_t m_level = ret.size();

    // ALAP 先放 max_level
    vector<vector<string>> alap_matrix;
    vector<string> alap_max_layer;
    vector<string> record; 
    for( auto &n : PrimaryOutputs){
      alap_max_layer.emplace_back(n);
      record.emplace_back(n);
    }     
    alap_matrix.emplace_back(alap_max_layer);
    
    // ALAP 限縮上界
    for(size_t i=0; i<m_level; i++){
      vector<string> alap_layer = {};
      for(auto &element: alap_matrix[i]){
        // cout << "element: " << element << endl;
        for(auto &input : Mapping[element].inputs){
          if(find(record.begin(), record.end(), input) == record.end()){ // 已放過不放
            if(find(PrimaryInputs.begin(), PrimaryInputs.end(), input) == PrimaryInputs.end()){ // 初始輸入點不放
              alap_layer.emplace_back(input);
              record.emplace_back(input);
            }
          }
        }
      }
      alap_matrix.emplace_back(alap_layer);
    }

    // for(auto &m: alap_matrix){
    // //   cout << "m: " <<  m  << endl;
    //   for(auto &n : m){
    //     cout << "n: " << n  << endl;
    //   }  
    // }

    for (size_t i=0; i<m_level; ++i) {
      for( auto &element : alap_matrix[i]){
        string node = element;
        cout << "c" << condition_count << ": ";
        for(size_t j=1; j<=m_level-i; j++ ){
          if(j == m_level-i){
            cout << node << "(" << j << ")";
          }else{
            cout << node << "(" << j << ") + ";
          }
        }
        cout << " = 1" <<endl;
        condition_count++;
      

        if(i<m_level+1){
          string node = element;
          cout << "c" << condition_count << ": ";
          for(size_t j=m_level+1-i; j<=m_level+1; j++ ){
            if(j == m_level+1){
              cout << node << "(" << j << ")";
            }else{
              cout << node << "(" << j << ") + ";
            }
          } 
          cout << " = 0" <<endl;
          condition_count++;               
        }     
      }
    }
    cout << "c"<< condition_count <<": ";
    for(size_t i=1; i<=m_level+1; ++i){
      if(i==m_level+1){
        cout << "end" << "(" << i << ")"; 
      }else{
        cout << "end" << "(" << i << ") + ";
      }
    }
    cout <<" = 1" << endl;
    condition_count++;

    // 限制二: input output順序
    // 後者要大於前者+1
    for (size_t i=0; i<m_level; ++i) {
      for( auto &element : alap_matrix[i]){
        vector<string> input_vec;
        string node = element;
        for (auto &input : Mapping[node].inputs){
          if(find(PrimaryInputs.begin(), PrimaryInputs.end(), input) == PrimaryInputs.end()){ // 初始輸入點不放
            input_vec.emplace_back(input);
          }
        }
        for(auto &element : input_vec){
          string input = element;
          cout << "c" << condition_count << ": ";
          for(size_t j=1; j<=m_level-i; j++ ){
            if(j == m_level-i){
              cout << j << " " << node << "(" << j << ")";
            }else{
              cout << j << " " << node << "(" << j << ") + ";                  
            }
          }
          cout << " - ";
          for(size_t j=1; j<=m_level-i; j++ ){
            if(j == m_level-i){
              cout << j << " " << input << "(" << j << ")" ;
            }else{
              cout << j << " " << input << "(" << j << ") - ";                  
            }
          }
          cout << " >= 1" << endl;
          condition_count++;
        }
      }
    } 
    // end 要在最後
    for(auto &element : alap_max_layer){
      cout << "c"<< condition_count <<": ";
      for(size_t i=1; i<=m_level+1; ++i){
        if(i == m_level+1){
          cout << i << " end" << "(" << i << ")" ;
        }else{
          cout << i << " end" << "(" << i << ") + ";
        }
      }
      cout << " - ";
      for(size_t i=1; i<=m_level+1; ++i){
        if(i == m_level+1){
          cout << i << " " << element << "(" << i << ")" ;
        }else{
          cout << i << " " << element << "(" << i << ") - ";
        }
      } 
      cout << " >= 1" << endl ;
      condition_count++;
    }

    // 限制三: constraints
    vector<string> and_gate, or_gate, inv_gate;
    for(auto &blifnode : Mapping){
      if (blifnode.second.gate == LogicGate::AND){
        string gate = blifnode.first;
        and_gate.emplace_back(gate);
      } else if (blifnode.second.gate == LogicGate::OR){
        string gate = blifnode.first;
        or_gate.emplace_back(gate);
      } else if(blifnode.second.gate == LogicGate::INV){
        string gate = blifnode.first;
        inv_gate.emplace_back(gate);
      }
    }

    // sum (and_gate_var) <= and_con
    LogicGate l;
    l = LogicGate::AND;
    size_t flag=0;
    for( auto &n : Mapping){
      if( n.second.gate == l){
        flag = 1;
      }
    }
    if(flag == 1){
      for(size_t level = 1; level <= m_level; level++){
        cout << "c"<< condition_count <<": ";
        for(size_t i=0; i<and_gate.size(); ++i){
          if (i == and_gate.size()-1){
            cout << and_gate[i] << "(" << level << ")";
          }else{
            cout << and_gate[i] << "(" << level << ") + ";
          }
        }
        cout << " <= " << and_con << endl;
        condition_count++;
      }
    }
    // sum (or_gate_var) <= or_con
    l = LogicGate::OR;
    flag=0;
    for( auto &n : Mapping){
      if( n.second.gate == l){
        flag = 1;
      }
    }
    if(flag == 1){
      for(size_t level = 1; level <= m_level; level++){
        cout << "c"<< condition_count <<": ";
        for(size_t i=0; i<or_gate.size(); ++i){
          if (i == or_gate.size()-1){
            cout << or_gate[i] << "(" << level << ")";
          }else{
            cout << or_gate[i] << "(" << level << ") + ";
          }
        }
        cout << " <= " << or_con << endl;
        condition_count++;
      }
    }
    
    // sum (inv_gate_var) <= inv_con
    l = LogicGate::INV;
    flag=0;
    for( auto &n : Mapping){
      if( n.second.gate == l){
        flag = 1;
      }
    }
    if(flag == 1){
      for(size_t level = 1; level <= m_level; level++){
        cout << "c"<< condition_count <<": ";
        for(size_t i=0; i<inv_gate.size(); ++i){
          if (i == inv_gate.size()-1){
            cout << inv_gate[i] << "(" << level << ")" ;
          }else{
            cout << inv_gate[i] << "(" << level << ") + ";
          }
        }
        cout << " <= " << inv_con << endl;
        condition_count++;
      }
    }
    cout << endl;


    //  所有變數都是二元變數
    cout << "Binary\n";
    for( auto &n : Mapping ){
      string node_name = n.first;
      for(size_t i=1; i<=m_level+1; ++i){
        cout << node_name << "(" << i << ") ";
      }
    }
    for(size_t i=1; i<=m_level+1; ++i){
      cout << "end" << "(" << i << ") ";
    }
    cout << "\n\nEnd" << endl;
    
    // 還原 cout 到原始狀態
    cout.rdbuf(coutbuf);

    file.close(); // 關閉文件
    } else {
        cerr << "無法打開文件!" << endl;
    }

  // 執行系統指令 glpsol —m output.mod -o output.sol 產生 output.sol
  auto gurobi_file = system("gurobi_cl ResultFile=output.sol output.lp");

  // 若下指令成功
  if(gurobi_file == 0){
    // 解讀 output.sol
    IlpParser gurobi;
    gurobi.ilpParse("output.sol");

    // 拿出 max_level 的值
    size_t ilpmaxlevel = gurobi.max_level;
    // // 配對成 {gate, <node_name, level>}
    auto ilpnodes = gurobi.getAllIlpNodes();

    // for(auto n : ilpnodes){
    //   cout <<"get_all_nodes: "<< n.ilp_node << " " << n.level << endl;
    // }

    // 宣告結果資料結構
    vector<array<string, 3>> res;
    // 按照level結果 一層一層加入res
    for(size_t level=1; level<=ilpmaxlevel; ++level){
      array<string, 3> one_level_res;
      for(auto &a : ilpnodes){
        if(a.level == level){
          string node = a.ilp_node;
          one_level_res[static_cast<size_t>(Mapping[node].gate)] += node;
          one_level_res[static_cast<size_t>(Mapping[node].gate)] += " ";
        }
      }
      // 去掉每個邏輯閘類型的末尾空格
      for (auto &gate_info : one_level_res) {
        if (!gate_info.empty()) {
          gate_info.resize(gate_info.size() - 1);
        }
      }
      // 將組合好的 "one_level_ret" 添加到 "ret" 向量中
      res.emplace_back(move(one_level_res));

    }
    return res;
  } else{
    cout << "gurobi_cl 失敗\n";
  } 
}


//   // 數學式 => 寫成output.mod檔(glpk)
//   ofstream file("output.mod"); // 打開一個名為 "output.mod" 的文件

//     if (file.is_open()) { // 確保文件成功打開
//         streambuf *coutbuf = cout.rdbuf(); // 保存原始的 cout 流緩衝區

//         // 重定向 cout 到文件流
//         cout.rdbuf(file.rdbuf());

//         cout << "set VAR ;\n" << endl;
//         for(auto &n : Mapping){
//           string node_name = n.first;
//           cout << "param " << node_name << "_coef {a in VAR} ;\n" ;
//         }
//         cout << "param demand_coef {a in VAR} ;\n" << endl; 

//         // node是否在level被執行只能是 0 or 1
//         // x[1],...,x[n] ∈{0,1}; 其中x為所有node_gate，n為max_level
//         // => var x {i in VAR},binary;
//         for( auto &n : Mapping ){
//           string node_name = n.first;
//           cout << "var " << node_name << " {a in VAR},binary;" << endl;
//         }
//         cout << "var end {a in VAR},binary;" << endl;
//         cout << endl;
        
//         // 目標: 求last_node所執行level為最小
//         // 最後一個node存在 ret.size()
//         // min = 1*lastgate_1 + 2*lastgate_2 + ... + maxlevel*lastgate_maxlevel
//         // MIN ∑demand[i]coeff[i]z[i] 其中z是last_node，i是level
//         // maximize Z: sum{i in VAR} level_coef[i]*z[i];
//         cout << "minimize Z: sum{a in VAR} demand_coef[a]*end[a];\n" << endl;

//         // 限制一: level: 1表示在該level執行，一個gate只能執行一次
//         // gate_1 + gate_2 + ... + gate_maxlevel = 1
//         // ∑x[i]  = 1; 
//         // => condition1: sum{i in VAR} x[i] = 1;
//         // ret[0] => level 1
//         // ret[1] => level 2
//         // ret.size() => level max
//         cout << "s.t." << endl;
//         size_t condition_count = 1;
//         size_t m_level = ret.size();

//         // ALAP 先放 max_level
//         vector<vector<string>> alap_matrix;
//         vector<string> alap_max_layer;
//         vector<string> record; 
//         for( auto &n : PrimaryOutputs){
//           alap_max_layer.emplace_back(n);
//           record.emplace_back(n);
//         }     
//         alap_matrix.emplace_back(alap_max_layer);

//         // // ASAP 先放 PrimaryInputs
//         // vector<vector<string>> asap_matrix;
//         // vector<string> asap_done_layer;
//         // vector<string> asap_undone_layer;
//         // for(auto &n : PrimaryInputs){
//         //   asap_done_layer.emplace_back(n)
//         // }
//         // for(auto &elements : Mapping){
//         //   asap_undone_layer.emplace_back(elements);
//         // }

//         // // ASAP 限縮下界
//         // for(auto &element : asap_undone_layer){
//         //   vector<string> asap_one_layer;
//         //   vector<string> input_record;
//         //   for( auto &input : element.second.inputs){
//         //     input_record.emplace_back(input);
//         //   }
//         //   for( auto &input : input_record){
//         //     if(find(asap_done_layer.begin(), asap_done_layer.end(), input) == asap_done_layer.end()){ // input已被完成
//         //       input_record.pop(input);
//         //     }
//         //     if(input_record.empty()){
//         //       asap_one_layer.push_back(element);
//         //       asap_undone_layer.pop(element);
//         //     }
//         //   }
//         //   asap_matrix.push_back(asap_one_layer);
//         // }

//         // for (size_t i=0; i<asap_matrix.size(); ++i) {
//         //   for( auto &element : asap_matrix[i]){
//         //       string node = element;
//         //       cout << "condition" << condition_count << ": (";
//         //       for(size_t j=1; j<=i+1; j++ ){
//         //         if(j == i+1){
//         //           cout << node << "[" << j << "]";
//         //         }else{
//         //           cout << node << "[" << j << "] + ";
//         //         }
//         //       }
//         //       cout << ") = 1;" <<endl;
//         //       condition_count++;   
//         //   }
//         // }
        

//         // for(auto &m: alap_matrix){
//         //   for(auto &n : m){
//         //     cout << n << " " << endl;
//         //   }  
//         // }


//         // ALAP 限縮上界
//         for(size_t i=0; i<m_level; i++){
//           vector<string> alap_layer = {};
//           for(auto &element: alap_matrix[i]){
//             // cout << "element: " << element << endl;
//             for(auto &input : Mapping[element].inputs){
//               if(find(record.begin(), record.end(), input) == record.end()){ // 已放過不放
//                 if(find(PrimaryInputs.begin(), PrimaryInputs.end(), input) == PrimaryInputs.end()){ // 初始輸入點不放
//                   alap_layer.emplace_back(input);
//                   record.emplace_back(input);
//                 }
//               }
//             }
//           }
//           alap_matrix.emplace_back(alap_layer);
//         }

//         // for(auto &m: alap_matrix){
//         // //   cout << "m: " <<  m  << endl;
//         //   for(auto &n : m){
//         //     cout << "n: " << n  << endl;
//         //   }  
//         // }

//         for (size_t i=0; i<m_level; ++i) {
//           for( auto &element : alap_matrix[i]){
//               string node = element;
//               cout << "condition" << condition_count << ": (";
//               for(size_t j=1; j<=m_level-i; j++ ){
//                 if(j == m_level-i){
//                   cout << node << "[" << j << "]";
//                 }else{
//                   cout << node << "[" << j << "] + ";
//                 }
//               }
//               cout << ") = 1;" <<endl;
//               condition_count++;
            

//               if(i<m_level+1){
//                 string node = element;
//                 cout << "condition" << condition_count << ": (";
//                 for(size_t j=m_level+1-i; j<=m_level+1; j++ ){
//                   if(j == m_level+1){
//                     cout << node << "[" << j << "]";
//                   }else{
//                     cout << node << "[" << j << "] + ";
//                   }
//                 } 
//                 cout << ") = 0;" <<endl;
//                 condition_count++;               
//               }     
     
//           }
//         }

//         // 限縮太緊
//         // for( auto &n : Mapping ){
//         //   string node_name = n.first;
//         //   for(size_t level=0; level<ret.size(); ++level){
//         //     for (const auto &gate : ret[level]) {
//         //       auto split_string = split(gate); // 使用 split 函數將當前行分割成單詞
//         //       for(size_t j=0; j<split_string.size(); ++j){
//         //         if( node_name == split_string[j]){
//         //           cout << "condition"<< condition_count <<": ";
//         //           for(size_t lv=1; lv<=level+1; ++lv){
//         //             if(lv == level+1){
//         //               cout << node_name << "[" << lv << "]";
//         //             } else {
//         //               cout << node_name << "[" << lv << "] +";
//         //             }
//         //           }
//         //           cout << " = 1;" << endl;
//         //           condition_count++;
//         //           if(level+1<m_level){
//         //             cout << "condition"<< condition_count <<": ";
//         //             for(size_t lv=level+2; lv<=m_level; ++lv){
//         //               if(lv == m_level){
//         //                 cout << node_name << "[" << lv << "]";
//         //               } else {
//         //                 cout << node_name << "[" << lv << "] +";
//         //               }
//         //             }
//         //             cout << " = 0;" << endl;
//         //             condition_count++; 
//         //           }                  
//         //         }
//         //       }

//         //     }            
//         //   }
//         //   // cout << "condition"<< condition_count <<": sum{a in VAR} " << node_name << "[a] = 1;" << endl;
//         //   // condition_count++;
//         // }
//         cout << "condition"<< condition_count <<": sum{a in VAR} end[a] = 1;" << endl;
//         condition_count++;

//         // 限制二: priority => 誰在誰後面執行
//         // gate_pior >= gate_inferior + 1
//         // 取得每一個 BlifNode.input BlifNode.output
//         // ∑level_coef[l]*x[l] >= ∑level_coef[l]*y[l] + 1 其中y為node的input，x為node的output
//         // for( auto &n : Mapping ){
//         //   string node_name = n.first;
//         //   for (auto &i : Mapping[node_name].inputs)
//         //   {
//         //     auto it = find(PrimaryInputs.begin(), PrimaryInputs.end(), i);
//         //     if (it == PrimaryInputs.end()) {    
//         //           cout << "condition"<< condition_count <<": (sum{a in VAR} " << Mapping[node_name].output << "_coef[a]*" << Mapping[node_name].output << "[a]) >= " 
//         //           << "((sum{a in VAR} " << i << "_coef[a]*"<< i << "[a]) + 1);" << endl ;
//         //           condition_count++;
//         //     }
//         //   } // 改成 coef * Mapping[node_name].output的[start_level]~[end_level] >= coef * input[start_level]~output[end_level] + 1
//         // }



//         // 後者要大於前者+1
//         for (size_t i=0; i<m_level; ++i) {
//           for( auto &element : alap_matrix[i]){

//               vector<string> input_vec;
//               string node = element;
//               for (auto &input : Mapping[node].inputs){
//                 if(find(PrimaryInputs.begin(), PrimaryInputs.end(), input) == PrimaryInputs.end()){ // 初始輸入點不放
//                   input_vec.emplace_back(input);
//                 }
//               }
//               for(auto &element : input_vec){

//                   string input = element;
//                   cout << "condition" << condition_count << ": (";
//                   for(size_t j=1; j<=m_level-i; j++ ){
//                     if(j == m_level-i){
//                       cout << j << "*" << node << "[" << j << "]";
//                     }else{
//                       cout << j << "*" << node << "[" << j << "] + ";                  
//                     }
//                   }
//                   cout << ") >= ((";
//                   for(size_t j=1; j<=m_level-i; j++ ){
//                     if(j == m_level-i){
//                       cout << j << "*" << input << "[" << j << "]";
//                     }else{
//                       cout << j << "*" << input << "[" << j << "] + ";                  
//                     }
//                   }
//                   cout << ") + 1);" << endl;
//                   condition_count++;

//               } 
            
//           }
//         } 

//         // end 要在最後
//         for(auto &element : alap_max_layer){
//             cout << "condition"<< condition_count <<": (sum{a in VAR} demand_coef[a]*end[a]) >= " 
//             << "((sum{a in VAR} "<< element <<"_coef[a]*"<< element << "[a]) + 1);" << endl ;
//             condition_count++;
//         }






//         // //  限縮太緊
//         // for( auto &n : Mapping ){
//         //   string node_name = n.first;
//         //   vector<string> inferior_list = {};
//         //   for (auto &i : Mapping[node_name].inputs){
//         //     auto it = find(PrimaryInputs.begin(), PrimaryInputs.end(), i);
//         //     if (it == PrimaryInputs.end()) { 
//         //       inferior_list.push_back(i);
//         //     }
//         //   }
//         //   while(!inferior_list.empty()){
//         //     for(size_t level=0; level<ret.size(); ++level){
//         //       for (const auto &gate : ret[level]) {
//         //         auto split_string = split(gate); // 使用 split 函數將當前行分割成單詞
//         //         for(size_t j=0; j<split_string.size(); ++j){
//         //           if( node_name == split_string[j]){
//         //             cout << "condition"<< condition_count <<": (";
//         //             for(size_t lv=1; lv<=level+1; ++lv){
//         //               if(lv == level+1){
//         //                 cout << lv << "*" << node_name << "[" << lv << "]";
//         //               } else {
//         //                 cout << lv << "*" << node_name << "[" << lv << "] + ";
//         //               }
//         //             }
//         //             cout << ") >= ((" ;
//         //             for(size_t lv=1; lv<=level; ++lv){
//         //               if(lv == level){
//         //                 cout << lv << "*" << inferior_list.back() << "[" << lv << "])";
//         //               } else {
//         //                 cout << lv << "*" << inferior_list.back() << "[" << lv << "] + ";
//         //               }
//         //             }
//         //             cout << " +1);\n";
//         //             condition_count++;
//         //             inferior_list.pop_back();
//         //           }                 
//         //         }
//         //       }
//         //     }            
//         //   }
//         // }

//         // for (const auto &last_node : ret[m_level]){
//         //   if(!last_node.empty()){
//         //     auto split_string = split(last_node); // 使用 split 函數將當前行分割成單詞
//         //     for(size_t j=0; j<split_string.size(); j++){
//         //       cout << "condition"<< condition_count <<": (sum{a in VAR} demand_coef[a]*end[a]) >= " 
//         //       << "((sum{a in VAR} "<< split_string[j] <<"_coef[a]*"<< split_string[j] << "[a]) + 1);" << endl ;
//         //       condition_count++;
//         //     }
//         //   }
//         // }
      


//         // 限制三: contraint: 同一level邏輯閘限制
//         // ret[0] => level 1 裡面的 and / or / not gate
//         vector<string> and_gate, or_gate, inv_gate;

//         for(auto &blifnode : Mapping){
//           if (blifnode.second.gate == LogicGate::AND){
//             string gate = blifnode.first;
//             and_gate.emplace_back(gate);
//           } else if (blifnode.second.gate == LogicGate::OR){
//             string gate = blifnode.first;
//             or_gate.emplace_back(gate);
//           } else if(blifnode.second.gate == LogicGate::INV){
//             string gate = blifnode.first;
//             inv_gate.emplace_back(gate);
//           }
//         }

//         // sum (and_gate_var) <= and_con
//         LogicGate l;
//         l = LogicGate::AND;
//         size_t flag=0;
//         for( auto &n : Mapping){
//           if( n.second.gate == l){
//             flag = 1;
//           }
//         }
//         if(flag == 1){
//           for(size_t level = 1; level <= ret.size(); level++){
//             cout << "condition"<< condition_count <<": (";
//             for(size_t i=0; i<and_gate.size(); ++i){
//               if (i == and_gate.size()-1){
//                 cout << and_gate[i] << "[" << level << "]";
//               }else{
//                 cout << and_gate[i] << "[" << level << "] + ";
//               }
//             }
//             cout << ") <= " << and_con << ";" << endl;
//             condition_count++;
//           }
//         }
//         // sum (or_gate_var) <= or_con
//         l = LogicGate::OR;
//         flag=0;
//         for( auto &n : Mapping){
//           if( n.second.gate == l){
//             flag = 1;
//           }
//         }
//         if(flag == 1){
//           for(size_t level = 1; level <= ret.size(); level++){
//             cout << "condition"<< condition_count <<": (";
//             for(size_t i=0; i<or_gate.size(); ++i){
//               if (i == or_gate.size()-1){
//                 cout << or_gate[i] << "[" << level << "]";
//               }else{
//                 cout << or_gate[i] << "[" << level << "] + ";
//               }
//             }
//             cout << ") <= " << or_con << ";" << endl;
//             condition_count++;
//           }
//         }
        
//         // sum (inv_gate_var) <= inv_con
//         l = LogicGate::INV;
//         flag=0;
//         for( auto &n : Mapping){
//           if( n.second.gate == l){
//             flag = 1;
//           }
//         }
//         if(flag == 1){
//           for(size_t level = 1; level <= ret.size(); level++){
//             cout << "condition"<< condition_count <<": (";
//             for(size_t i=0; i<inv_gate.size(); ++i){
//               if (i == inv_gate.size()-1){
//                 cout << inv_gate[i] << "[" << level << "]";
//               }else{
//                 cout << inv_gate[i] << "[" << level << "] + ";
//               }
//             }
//             cout << ") <= " << inv_con << ";" << endl;
//             condition_count++;
//           }
//         }
//         cout << endl;

//         // 印出data; set VAR :=
//         cout << "data;" << endl;
//         cout << "set VAR := ";
//         for(size_t level = 1; level <= ret.size()+1; ++level){
//           cout << level << " ";
//         }
//         cout << ";\n" << endl;

//         // 印出 param demand :=
//         for(auto &n : Mapping){
//           string node_name = n.first;
//           cout << "param "<< node_name <<"_coef :=" << endl;
//           for(size_t i = 1; i<=ret.size()+1; i++){
//               if(i == ret.size()+1){
//                 cout << i << "    " << i<< ";\n" << endl;
//               } else{
//                 cout << i << "    " << i << endl;
//               }
//           }
//         }


//         cout << "param demand_coef :=" << endl;
//         for(size_t i = 1; i<=ret.size()+1; i++){
//           if(i == ret.size()+1){
//             cout << i << "    " << i;
//           }else{
//             cout << i << "    " << i << endl;
//           }
//         }
//         cout << ";\n" << endl;
//         cout << "end;\n";

//         // 還原 cout 到原始狀態
//         cout.rdbuf(coutbuf);

//         file.close(); // 關閉文件
//     } else {
//         cerr << "無法打開文件!" << endl;
//     }

//   // 執行系統指令 glpsol —m output.mod -o output.sol 產生 output.sol
//   auto glpk_file = system("glpsol --model output.mod --output output.sol");

//   // 若下指令成功
//   if(glpk_file == 0){
//     // 解讀 output.sol
//     IlpParser glpk;
//     glpk.ilpParse("output.sol");

//     // 拿出 max_level 的值
//     size_t ilpmaxlevel = glpk.max_level;
//     // // 配對成 {gate, <node_name, level>}
//     auto ilpnodes = glpk.getAllIlpNodes();

//     // 宣告結果資料結構
//     vector<array<string, 3>> res;
//     // 按照level結果 一層一層加入res
//     for(size_t level=1; level<=ilpmaxlevel; ++level){
//       array<string, 3> one_level_res;
//       for(auto &a : ilpnodes){
//         if(a.level == level){
//           string node = a.ilp_node;
//           one_level_res[static_cast<size_t>(Mapping[node].gate)] += node;
//           one_level_res[static_cast<size_t>(Mapping[node].gate)] += " ";
//         }
//       }
//       // 去掉每個邏輯閘類型的末尾空格
//       for (auto &gate_info : one_level_res) {
//         if (!gate_info.empty()) {
//           gate_info.resize(gate_info.size() - 1);
//         }
//       }
//       // 將組合好的 "one_level_ret" 添加到 "ret" 向量中
//       res.emplace_back(move(one_level_res));

//     }
//     return res;
//   } else{
//     cout << "glpsol 失敗\n";
//   } 
// }

