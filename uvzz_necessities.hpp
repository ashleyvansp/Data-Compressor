/*
    CSC 485B
    Ashley Van Spankeren
    V00865956
*/
#include <array>
#include <vector>
#include <algorithm>
const unsigned int MAX_BLOCK_SIZE = 2500;
const unsigned short BLOCK_LEN_NUM_BITS = 12;
const unsigned short BWT_INDEX_NUM_BITS = 12;


#define MAGIC1 0x88
#define MAGIC2 0x72

struct BWT_item{
    std::vector<u16> last_col;
    int row;
};

struct Node{ 
    u32 value; 
    struct Node *next; 
}; 
