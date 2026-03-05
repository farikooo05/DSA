
# DSA2 Project 2526

The following README is to be completed by you.
You MUST keep the following statement in the document and sign it by writting your full name.

##  Declaration of Authorship
> I hereby certify that the work submitted is my own. While I may have occasionally used artificial intelligence tools for assistance, I have carefully reviewed and verified the submitted source code and written report. I fully understand their content and take complete responsibility for the work presented.

* Name1:
* Name2:
* Name3:


## Subject

The subject is described on the Moodle page of the course.

## Tests

### Test Files Description

Let us assume your project's structure is as follows:

```
dsa2-project
├── src
│   ├── Makefile
│   ├── sched.c
│   ├── ...
│   └── sched
└── tests
     ├── out
     │   ├── test-0.out
     │   ├── test-0.res
     │   ├── ...
     ├── in
     |   ├── test-0.in
     |   ├── ...
     ├── ref
     │   ├── test-0.ref
     │   ├── ...
     │   └── test-10.ref
     ├── src
     │   ├── Makefile
     │   ├── read_trace.c
     │   ├── read_trace.h
     │   ├── test_main.c
     │   ├── workload.c
     │   └── workload.h
     └── test-sched.sh
```

Your source files must be in the top `src/` directory.
Complete the existing `Makefile` do not write a new one from scratch.
You will find test a test program to build in `tests/src` with `make`.
Once build, you will be able to test with the `tests/test-sched.sh` script.


Description of the data directories:
- `in` : the input data files
- `ref` : the expected output.
- `out` :  initially contains  the full output  of my  scheduler (with
  explained  decisions). Once  you run  the  test script,  it will  be
  overwritten by your program outputs.


## Test Principle
The principle is to analyze an output such as:
```
* Parsed trace (from out/test-8.res):
 0: RRRRRR.......RRRRR___________
 1: ..RRRRRRRRRRR________________
 2: ....RR.R.R.R.RRRRRR__________
 3: ......RRRRRRRRRRR____________
```
and check that:
1. at any timestep, the cumulative load of scheduled processes does not exceed the CPU capacity. In the example above, at timestep=4 (processes 0,1 and 2 are running), we can get their priorities in the `in/test-8.in` data, and sum them.
2. the makespan of the execution (i.e the walltime) does not last more than a given limit, specified per test.
3. the number of execution quantums match the total sum of processes' durations. An execution quantum is an 'R' in the trace.
4. the property "At a given timestep, the running processes that can fit in the CPU capacity all have a greater of equal priority than the ones in pending queue" holds.

Note that `test-sched.sh` uses the auxiliary C program specified in the script as: `BIN_TEST=src/test_main`. This program is passed the maximum CPU capacity and walltime limit and make the above verifications.
Note that the expected maximum walltimes for the produced schedule are hard-coded in the script as:
```shell
MAX_DURATIONS=(15 20 24 16 29 19 21 25 19 24 29)
```
for respectively `in/test-[0,1,2,...,10]` and are passed as parameter to the `BIN_TEST` program.


## How to use `test-sched.sh`

From the top directory of your project:
1. `cd tests/src`
2. `make`   # This compiles the `test_main` C program
3. `cd ..`
4. Run `./test-sched.sh`



## Report

-- Insert your report here ---



