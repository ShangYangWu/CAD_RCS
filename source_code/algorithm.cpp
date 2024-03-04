
#include "algorithm.hpp"
#include "heuristic.hpp"
#include "ilp.hpp"
using namespace std;

unique_ptr<Algorithm> makeAlgorithm(const string& mode) {
    if (mode == "-h") {
        return make_unique<Heuristic>();
    } 
    else if(mode == "-e"){
        return make_unique<ilp>();
    }
    return nullptr;
}