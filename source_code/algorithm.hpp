
#pragma once // 避免多次包含同一頭文件

#include "parser.hpp" // 包含用於解析 BLIF 文件的頭文件
#include <array> // 包含 STL 的 array 容器
#include <memory> // 包含 STL 的智能指針

using namespace std;

// 定義一個抽象的算法界面，用於解析 BLIF 文件並執行算法
struct Algorithm {
    // 解析 BLIF 文件的純虛擬函數，接受所有節點和主要輸入
    virtual void parse(const vector<BlifNode>& all_nodes, const vector<string>& primary_input, const vector<string>& primary_outputs) = 0;
    
    // 執行算法的純虛擬函數，接受 AND、OR 和 INV 閘的參數，返回結果
    virtual vector<array<string, 3>> run(int and_con, int or_con, int inv_con) = 0;
       
    // 虛擬析構函數，用於釋放資源
    virtual ~Algorithm() = default;
};

// 函數原型：創建一個特定模式（mode）的算法實例
unique_ptr<Algorithm> makeAlgorithm(const string& mode);
