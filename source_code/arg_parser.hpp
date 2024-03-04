
#pragma once

#include <iostream>
#include <string>

struct ArgParser {
  std::string mode;
  int and_constraint;
  int or_constraint;
  int inv_constraint;
  std::string file;
  std::string target_file;
};

inline void PrintError(int argc, char **argv) {
  std::cout << "Command: ";
  for (int i = 0; i < argc; ++i) {
    std::cout << argv[i] << " ";
  }
  std::cout << "is not a legal command.\n";
  std::cout << "Please use: \n";
  std::cout << "  " << argv[0]
            << " -h/-e <blif> AND_CONSTRAINT OR_CONSTRAINT INV_CONSTRAINT\n";
  std::cout
      << "  " << argv[0]
      << " -v    <blif> AND_CONSTRAINT OR_CONSTRAINT INV_CONSTRAINT <ans_file>\n";
  exit(1);
}

inline ArgParser ParseArgument(int argc, char **argv) {
  if (argc < 6 || argc > 7) {
    PrintError(argc, argv);
  }
  ArgParser p;
  p.mode = argv[1];
  if (argc == 6 && p.mode != "-h" && p.mode != "-e") {
    PrintError(argc, argv);
  } else if (argc == 7 && p.mode != "-v") {
    PrintError(argc, argv);
  }

  p.file = argv[2];
  p.and_constraint = std::stoi(argv[3]);
  p.or_constraint = std::stoi(argv[4]);
  p.inv_constraint = std::stoi(argv[5]);
  if (argc == 7) {
    p.target_file = argv[6];
  }
  return p;
}