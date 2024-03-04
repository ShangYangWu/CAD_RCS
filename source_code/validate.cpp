#include "validate.hpp"
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <sstream>
namespace {

std::vector<std::string> split(const std::string &line) {
  std::vector<std::string> split;
  std::stringstream ss;
  ss << line;
  std::string s;
  while (ss >> s) {
    split.emplace_back(std::move(s));
  }
  return split;
}
} // namespace

const char *getName(LogicGate g) {
  switch (g) {
  case LogicGate::AND:
    return "AND";
  case LogicGate::OR:
    return "OR";
  case LogicGate::INV:
    return "NOT";
  }
  return "";
}
int add_to_mapping(std::unordered_map<std::string, std::pair<size_t, LogicGate>> &mapping,
                   std::vector<std::string> &gate_strs, size_t deep,
                   size_t constraint, LogicGate logic) {
  int err = 0;
  if (gate_strs.size() > constraint) {
    for (const auto &gate : gate_strs) {
      std::cout << gate << " ";
    }

    std::cout << getName(logic) << " constraint is viloated at latency " << deep
              << "\n";
    err = 2;
  }
  for (const std::string &gate : gate_strs) {
    mapping[gate] = std::make_pair(deep, logic);
  }
  return err;
}

int parse_one_line(std::unordered_map<std::string, std::pair<size_t, LogicGate>> &mapping,
                   int and_g, int or_g, int inv_g, const std::string &line) {

  auto colon_pos = line.find(':');
  size_t deep = std::stoi(line.substr(0, colon_pos));
  size_t left = line.find('{', colon_pos);
  size_t right = line.find('}', left + 1);
  auto and_vec = split(line.substr(left + 1, right - left - 1));
  left = line.find('{', right);
  right = line.find('}', left);
  auto or_vec = split(line.substr(left + 1, right - left - 1));
  left = line.find('{', right);
  right = line.find('}', left);
  auto inv_vec = split(line.substr(left + 1, right - left - 1));

  int err = 0;
  err |= add_to_mapping(mapping, and_vec, deep, and_g, LogicGate::AND);
  err |= add_to_mapping(mapping, or_vec, deep, or_g, LogicGate::OR);
  err |= add_to_mapping(mapping, inv_vec, deep, inv_g, LogicGate::INV);
  return err;
}
std::unordered_map<std::string, std::pair<size_t, LogicGate>>
validate_constraint(const std::string &file, int and_g, int or_g, int inv_g,
                    int &err) {
  err = 0;
  std::ifstream fin{file, std::ios::in};
  std::string buffer;
  std::getline(fin, buffer);
  std::unordered_map<std::string, std::pair<size_t, LogicGate>> ret;
  size_t deep = 0;
  while (std::getline(fin, buffer)) {
    if (buffer.find("LATENCY:") != std::string::npos) {
      auto s = split(buffer);
      if (std::stoi(s[1]) != static_cast<int>(deep)) {
        std::cout << "Latency: " << s[1] << " is wrong.\n";
        std::cout << "Latency should be " << deep << "\n";
        err |= 1;
      }
      break;
    }
    err |= parse_one_line(ret, and_g, or_g, inv_g, buffer);
    deep++;
  }
  return ret;
}
int Validate::validate(const std::vector<BlifNode> &nodes) {
  int err = 0;
  auto mapping = validate_constraint(file, and_con, or_con, inv_con, err);
  if (err != 0) {
    return err;
  }

  for (const auto &node : nodes) {
    auto it = mapping.find(node.output);
    if (it == mapping.end()) {
      std::cout << node.output << " is not in the results.\n";
      err = 4;
      continue;
    }
    auto deep = it->second.first;
    auto logic = it->second.second;
    for (const auto &in : node.inputs) {
      if (node.gate != logic) {
        std::cout << it->first << " " << getName(logic) << " Gate is wrong.\n";
        std::cout << "The gate should be " << getName(node.gate) << " for "
                  << it->first << "\n";
        err |= 16;
      }
      auto it2 = mapping.find(in);
      if (it2 != mapping.end()) {
        auto deep2 = it2->second.first;
        if (deep2 >= deep) {
          std::cout << it->first << " should be later than " << it2->first
                    << "\n";
          std::cout << it->first << " has latency " << deep << "\n";
          std::cout << it2->first << " has latency " << deep2 << "\n";
          err |= 8;
        }
      }
    }
  }
  if (err == 0) {
    std::cout << "Congratulation!! Validate Passed.\n";
  }
  return err;
}