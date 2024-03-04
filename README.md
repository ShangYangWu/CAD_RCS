README<BR/>

# CAD_RCS
CAD PROJECT 2<BR/>

## Generate Design:
cd source_code<BR/>
make<BR/>
./M11215084 -h/-e ../aoi_benchmark/filename.blif and or not<BR/>

### -h: using heuristic solver
./M11215084 -h ../aoi_benchmark/test.blif 2 1 1<BR/>

### -e: using heuristic solver
./M11215084 -e ../aoi_benchmark/test.blif 2 1 1<BR/>

## Validate Design:
./M11215084 -v \filename.blif 2 1 1 \result_to_validate <BR/>

## Test case: Result with different latencies
### 1. using heuristic solver: latency=10
./M11215084 -h ../aoi_benchmark/test.blif 2 1 1<BR/>

Heuristic Scheduling Result<BR/>
1: {b1 b2} {} {}<BR/>
2: {b3 b4} {} {}<BR/>
3: {c1 b5} {} {}<BR/>
4: {d1} {c2} {}<BR/>
5: {} {c5} {e1}<BR/>
6: {} {c3} {}<BR/>
7: {} {d2} {}<BR/>
8: {} {c4} {}<BR/>
9: {} {d3} {}<BR/>
10: {} {e2} {}<BR/>
LATENCY: 10<BR/>
END<BR/>

### 2. using ILP solver: latency=8<BR/>
./M11215084 -e ../aoi_benchmark/test.blif 2 1 1<BR/>

ILP-based Scheduling Result<BR/>
1: {b3 b5} {} {}<BR/>
2: {b1 b2} {c3} {}<BR/>
3: {b4 c1} {c4} {}<BR/>
4: {d1} {c5} {}<BR/>
5: {} {c2} {}<BR/>
6: {} {d3} {e1}<BR/>
7: {} {d2} {}<BR/>
8: {} {e2} {}<BR/>
LATENCY: 8<BR/>
END<BR/>

