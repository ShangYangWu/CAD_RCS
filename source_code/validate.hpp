#pragma once

#include "parser.hpp"

struct Validate {
    std::string file;
    int and_con;
    int or_con;
    int inv_con;
    Validate(std::string file, int and_constraint, int or_constraint, int inv_constraint):file{std::move(file)}, and_con{and_constraint}, or_con{or_constraint},inv_con{inv_constraint}{}
    int validate(const std::vector<BlifNode>& nodes);
};

