#include "parser.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

namespace {

// 將一個包含空格分隔單詞的字串 line 分割成一個字串向量 split，並返回該向量。
vector<string> split(const string &line) {
  vector<string> split;
  stringstream ss;
  ss << line;
  string s;
  while (ss >> s) {
    // 在每次從流中讀取一個單詞後，使用 emplace_back 函數將該單詞添加到 split 向量中。
    // std::move(s) 的使用表示將 s 的內容移動到向量中
    if(s != "\\"){
      split.emplace_back(move(s));
    }
  }
  return split;
}
} // namespace

LogicGate getLogic(size_t condition, size_t input_size) {
  // 如果輸入大小為1，返回邏輯閘類型 INV（反相器）
  if (input_size == 1) {
    return LogicGate::INV;
  }
  // 如果條件等於輸入大小，表示所有輸入都需要參與計算，返回邏輯閘類型 OR（或閘）
  if (condition == input_size) {
    return LogicGate::OR;
  }
  // 默認情況下，返回邏輯閘類型 AND（與閘）
  return LogicGate::AND;
}


void Parser::parse(const string &file) {
  ifstream fin(file, ios::in); // 打開指定文件以讀取
  string buffer; // 用於存儲讀取的每一行文本
  BlifNode node; // 用於臨時存儲正在解析的BLIF節點信息
  size_t num = 0; // 用於計算節點的輸入數量
  bool input_flag = 0;
  bool output_flag = 0;

  // 遍歷文件的每一行
  while (getline(fin, buffer)) {
    auto split_string = split(buffer); // 使用 split 函數將當前行分割成單詞
    auto &title = split_string[0]; // 提取當前行的標題（以第一個單詞表示）

    if(title == ".names"){
      output_flag = 0;
    }

    if (title == ".outputs"){
      input_flag = 0;
      move(split_string.begin() + 1, split_string.end(), back_inserter(outputs));
      output_flag = 1;
      continue;
    }

    if (title == ".inputs"){
      input_flag = 1;
      move(split_string.begin() + 1, split_string.end(), back_inserter(inputs));
      continue;
    }



    if (title == ".names" || title == ".end") { // 如果標題是 ".names" 或 ".end"
      if (!node.output.empty()) { // 如果正在解析的節點的輸出不為空
        node.gate = getLogic(num, node.inputs.size()); // 設置節點的邏輯閘類型
        nodes.emplace_back(move(node)); // 將節點添加到節點向量中
        num = 0; // 重置輸入數量計數，判斷下一個condition
        node.output.clear(); // 清空節點的輸出
        node.inputs.clear(); // 清空節點的輸入
      }

      move(split_string.begin() + 1, split_string.end() - 1, back_inserter(node.inputs)); // 將分割的單詞作為節點的輸入添加
      node.output = split_string.back(); // 設置節點的輸出
      if (title == ".end") { // 如果標題是 ".end"，結束解析
        return;
      }
    } else if (title == ".model") { // 如果標題是 ".model"，提取模型名稱
        ModelName = split_string[1];
    } else if(input_flag == 1) { // 如果標題是 ".inputs"，提取輸入變數
        move(split_string.begin(), split_string.end(), back_inserter(inputs));
    } else if(output_flag == 1) { // 如果標題是 ".outputs"，提取輸出變數
        move(split_string.begin(), split_string.end(), back_inserter(outputs));
    } else { // 如果標題不符合以上條件，則可能是邏輯節點的定義
      if (split_string.size() == 2 &&
          (split_string[1] == "1" || split_string[1] == "0")) {
        num++; // 增加輸入數量計數，用來判斷condition
      }
    }
  }
}


void BlifNode::print() const {
  switch (gate) {
  case LogicGate::AND:
    cout << "AND ";
    break;
  case LogicGate::OR:
    cout << "OR ";
    break;
  case LogicGate::INV:
    cout << "INV ";
    break;
  }

  for (const auto &input : inputs) {
    cout << input << " ";
  }
  cout << " -> ";
  cout << output << "\n";
}

void Parser::print() const {
  cout << "Model: " << ModelName << "\n";
  cout << "Inputs: ";
  for (const auto &input : inputs) {
    cout << input << " ";
  }
  cout << "\n";
  cout << "Outputs: ";
  for (const auto &output : outputs) {
    cout << output << " ";
  }
  cout << "\n";
  cout << "ALL Logic:\n";
  for (const auto &logic : nodes) {
    logic.print();
  }
}