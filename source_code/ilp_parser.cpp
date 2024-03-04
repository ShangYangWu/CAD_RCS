#include "ilp_parser.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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
      // move(s) 的使用表示將 s 的內容移動到向量中
      split.emplace_back(move(s));
    }
    return split;
  }
} // namespace



void IlpParser::ilpParse(const string file) {
  ifstream fin(file, ios::in); // 打開指定文件以讀取
  string buffer; // 用於存儲讀取的每一行文本
  IlpNode node; // 用於臨時存儲正在解析的Ilp節點信息
  // size_t flag = 0;

  // 遍歷文件的每一行
  while (getline(fin, buffer)) {
    auto split_string = split(buffer); // 使用 split 函數將當前行分割成單詞

    if (!split_string.empty()) {  // 確保 split_string 非空
      auto &title = split_string[0];
      auto &find_node = split_string[1]; // 提取當前行的標題（以第一個單詞表示）
      string max, level;

      // // 找max_level  
      // if(title == "Objective:"){
      //   max = split_string[3];
      //   max_level = stoi(max)-1;
      // }

      // // 遇到end[1]就停止
      // if( find_node == "end[1]"){
      //   break;
      // }

      // // 開始遍歷
      // if( title == "No." && find_node == "Column"){
      //   flag = 1;
      // }

      // 找max_level  
      if(title == "#"){
        max = split_string[4];
        max_level = stoi(max)-1;
      }

      if(find_node == to_string(1)){

        // istringstream iss(title);
        // string token;

        // while (iss >> token) {
        //     string letters;
        //     string numbers;

        //     for (char c : token) {
        //         if (isalpha(c)) {
        //             letters += c;
        //         } else if (isdigit(c)) {
        //             numbers += c;
        //         }
        //     }

        //     node.ilp_node = letters;
        //     node.level = stoi(numbers);

        //     ilp_nodes.push_back(node);        

        string input = title;
        // 找到 '[' 和 ']' 的位置
        size_t leftBracketPos = input.find('(');
        size_t rightBracketPos = input.find(')');

        if (leftBracketPos != string::npos && rightBracketPos != string::npos) {
          // 提取變數名
          node.ilp_node = input.substr(0, leftBracketPos);
          // 提取數字部分，並轉換為整數
          level = input.substr(leftBracketPos + 1, rightBracketPos - leftBracketPos - 1);
          node.level = stoi(level);

          ilp_nodes.push_back(node);

        // }
        }       
      }
    }
  }
}