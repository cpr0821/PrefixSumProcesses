# PrefixSumProcesses

A concurrent program that computes the prefix sum of a given array using multiple
processes to achieve concurrency. 

It takes the following command line arguments as input:

• n: size of the input array

• <input-file>: name of the text file that contains n integers representing the input array A

• <output-file>: name of the text file that contains n integers representing the output array
B (to be computed by your program)

• m: number of processes that will perform the work (degree of concurrency)

It is written in C++ and it runs on a UNIX/Linux system. It uses shared memory for IPC.
