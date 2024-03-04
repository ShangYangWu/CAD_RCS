
#include "algorithm.hpp"
#include "arg_parser.hpp"
#include "parser.hpp"
#include "validate.hpp"

using namespace std;

int main(int argc, char **argv) {
  
  // 檢查是不是合法的輸入，並決定是哪個模式(-h/-e)
  auto arg = ParseArgument(argc, argv);

  // 解讀file
  Parser parse;
  parse.parse(arg.file);
  //   parse.print();

  if (arg.mode == "-v") {
    Validate v{arg.target_file, arg.and_constraint, arg.or_constraint, arg.inv_constraint};
    return v.validate(parse.getAllNodes());
  }

  // 選擇模式
  auto algorithm = makeAlgorithm(arg.mode);
  // 智慧指標，借用algorithm的parse
  algorithm->parse(parse.getAllNodes(), parse.inputs, parse.outputs);
  // 智慧指標，result存為借用algorithm的run的結果
  auto result = algorithm->run(arg.and_constraint, arg.or_constraint, arg.inv_constraint);
  

  // dump
  if (arg.mode == "-e") {
    cout << "\nILP-based Scheduling Result\n";
    // 執行方式: 製作 out.mod 的內容
    // 用glpk製作 output.sol
    // algorithm->print_out(arg.and_constraint, arg.or_constraint, arg.inv_constraint);
  } else {
    cout << "Heuristic Scheduling Result\n";
  }
  for (size_t level = 0; level < result.size(); ++level) {
    cout << level + 1 << ": ";
    for (const auto &gate : result[level]) {
      cout << "{" << gate << "} ";
    }
    cout << "\n";
  }
  cout << "LATENCY: " << result.size() << "\n";
  cout << "END\n";

}