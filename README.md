# Data Compression Program
CSC 485B
Ashley Van Spankeren

## The Design
---------

There are 4 main steps to the compression scheme, excluding the block header (which will be covered in the Bitstream Format section):
1. Run Length Encoding
2. Burrows-Wheeler Transform
3. Run Length Encoding
4. Prefix Coding

The run length encoding only outputs runs of length 4 or more. The data is stored in a vector of u16 values to simplify the prefix coding. This has the consequence that if a run exceeds length 259, it will be stored in separate runs.

The Burrows Wheeler Transform makes a matrix of rotations of the input vector, applies the standard sorting algorithm, and returns a data type containing the last column of the matrix and the row of the original string.

The prefix coding step uses fixed length codes. I took advantage of the property that run length codes will be output if and only if there has been a run of length 4 and hence the run length codes need not be distinct from the literal codes. There is one 8-bit prefix code representing 256 possible literal values and another 8-bit prefix code representing run lengths 4 - 259. Using dynamic codes would have greatly improved the compression ratio but unfortunately correctness and completeness were prioritized before spectacular compression ratios so here we are.

## The Architecture
---------

The compression side of things is pretty vanilla as far as algorithms and data structures go. The algorithm to construct canonical codes is the same as that used in Assignment 2. There is definitely opportunity to improve the runtime by using more clever algorithms to compute the BWT but again, correctness and completeness were the queens here.

Interestingly enough, using clever algorithms to invert the BWT was absolutely necessary to meet the requirement of completeness. My first attempt at inversion was brute force in its purest definition and sorted an n x n matrix n times, skyrocketing the runtime to 15+ minutes to validate the test data. So in came the pointers...
* There are two vectors representing the first and last columns of the theoretical matrix of rotations
* There is also an array (creatively named @lists) of linked lists, where lists[i] is an ascending list of indices that element i occurs in the last column of the theoretical matrix
* The decompressor then traverses through each list in order, storing the values in the vector @list_traversal
* At this point we effectively have the diagram where each element in the first column points to a distinct element in the last column
* So finding the original string becomes as easy as:
    - Starting in the first column at the row specified by the input @bwt.row
    - "Following the arrow" to some element in row k in the last column 
    - Sliding back to the first column in row k
    - Following that arrow 
    - Continuing until we're back to the starting spot

## The Bitstream
---------

Each compressed file begins with a 2-byte magic number (fraudsters beware!) which gets error-checked by the decompressor.
The data then gets processed block by block. The block header specifies 4 properties:
    [is_last][uses_RLE][block_length][BWT index]
    [1 bit]   [1 bit]     [12 bits]  [12 bits]

* is_last indicates whether or not that block will be the last in the file. 
* uses_RLE exists to acknowledge the fact that blocks with many runs of length 4 may be expanded by RLE. After the BWT and both rounds of RLE are applied, if it's determined that these fancy tricks actually expanded the data, then the original literal values are just output instead.
* block_length indicates how many 8-bit codes the decompressor should read in for this block.
* BWT_index indicates which row in the theoretical matrix of sorted rotations contains the original string

Due to block_length being encoded in 12 bits, @MAX_BLOCK_SIZE can be at most 2^12 = 4096.

## Sources
---------
* https://www.geeksforgeeks.org/inverting-burrows-wheeler-transform/ 
* https://thispointer.com/c-how-to-find-an-element-in-vector-and-get-its-index/
* My submission for Assignment 2
* The input stream helper for the arithmetic coder provided by Bill