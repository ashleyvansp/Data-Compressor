/* uvzz_decompress.cpp
   CSC 485B
   Ashley Van Spankeren
   V00865956
*/
/*
    Resources:
    * https://www.geeksforgeeks.org/inverting-burrows-wheeler-transform/ (inverting BWT efficiently)
*/

#include <iostream>
#include "./stream/input_stream.hpp"
#include "./uvzz_necessities.hpp"

std::vector<u32> fixed_literal_code_lengths(256);
std::vector<u32> fixed_literal_codes(256);
std::vector<u32> fixed_run_code_lengths(256);
std::vector<u32> fixed_run_codes(256);

// Creates node with value specified by @value
struct Node *create_node(u32 value){
    struct Node *n = (struct Node *)malloc(sizeof(struct Node));
    n->value = value;
    n->next = NULL;
    return n;
}

// Appends @node to the end of the list pointed to by @head
void append_node(struct Node **head, struct Node *node){
    if (*head == NULL){
        *head = node;
        return;
    }
    struct Node *temp = *head;
    while (temp->next != NULL){
        temp = temp->next;
    }
    temp->next = node;
}

/*
    1. Sort the last column to get the first column
    2. Put a 'copy' of the last column in front of the first column
    3. Now we can "draw arrows" from the first column to the last column
        * Eliminating ambiguity if two i's are followed by a j by recognizing that the alphabetical sort implies that the first i points to the first j and so on
    4. To get the original string:
        a. Start at first_col[solution row]
        b. Follow the arrow, say it goes to row k
        c. Go to first_col[k]
        d. Follow the arrow
        e. Repeat
*/ 
std::vector<u16> invert_BWT(BWT_item bwt){
    u32 n = bwt.last_col.size();
    std::vector<u16> first_col = bwt.last_col;

    // Sort the last column to get the first column
    std::sort(first_col.begin(), first_col.end());
    
    struct Node* lists[256] = {NULL};
    // Each lists[i] is a linked list where the value at each Node is an index where i occurs in the BWT last column
    // Appending the nodes to the back of the list preserves the property that the first occurrence of character i is followed by the first occurrence of character j
    // (in the case that 'ij' occurs multiple times in the original string)
    for (u32 i = 0; i < n; i++){
        struct Node *node = create_node(i);
        append_node(&lists[bwt.last_col.at(i)], node);
    }

    // Now traverse the linked lists in order
    std::vector<u32> list_traversal(n);
    for (u32 i = 0; i < n; i++){
        struct Node **head = &lists[first_col.at(i)];
        list_traversal.at(i) = (*head)->value;
        (*head) = (*head)->next; 
    }

    u32 row = bwt.row;
    std::vector<u16> decoded(n);
    // Store the solution in @decoded
    for (u32 i = 0; i < n; i++){
        row = list_traversal.at(row);
        decoded.at(i) = bwt.last_col.at(row);
    }

    return decoded;
}

//Given a vector of lengths where lengths.at(i) is the code length for symbol i,
//returns a vector V of unsigned int values, such that the lower lengths.at(i) bits of V.at(i)
//comprise the bit encoding for symbol i (using the encoding construction given in RFC 1951). Note that the encoding is in 
//MSB -> LSB order (that is, the first bit of the prefix code is bit number lengths.at(i) - 1 and the last bit is bit number 0).
//The codes for symbols with length zero are undefined.
std::vector<u32> construct_canonical_code(std::vector<u32> const& lengths){

    unsigned int size = lengths.size();
    std::vector< unsigned int > length_counts(16,0); //Lengths must be less than 16 for DEFLATE
    u32 max_length = 0;
    for(auto i: lengths){
        //assert(i <= 15);
        length_counts.at(i)++;
        max_length = std::max(i, max_length);
    }
    length_counts[0] = 0; //Disregard any codes with alleged zero length

    std::vector< u32 > result_codes(size,0);

    //The algorithm below follows the pseudocode in RFC 1951
    std::vector< unsigned int > next_code(size,0);
    {
        //Step 1: Determine the first code for each length
        unsigned int code = 0;
        for(unsigned int i = 1; i <= max_length; i++){
            code = (code+length_counts.at(i-1))<<1;
            next_code.at(i) = code;
        }
    }
    {
        //Step 2: Assign the code for each symbol, with codes of the same length being
        //        consecutive and ordered lexicographically by the symbol to which they are assigned.
        for(unsigned int symbol = 0; symbol < size; symbol++){
            unsigned int length = lengths.at(symbol);
            if (length > 0)
                result_codes.at(symbol) = next_code.at(length)++;
        }  
    } 
    return result_codes;
}

void init(){
    for (int i = 0; i < 256; i++){
        fixed_literal_code_lengths.at(i) = 8;
        fixed_run_code_lengths.at(i) = 8;
    }

    fixed_literal_codes = construct_canonical_code(fixed_literal_code_lengths);
    fixed_run_codes = construct_canonical_code(fixed_run_code_lengths);
}

// Looks up the given code in the vector of prefix codes and stores the index of the code in &value
// Returns true iff the code was successfully found
// https://thispointer.com/c-how-to-find-an-element-in-vector-and-get-its-index/
bool find_code(const u32& code, u8& value, std::vector<u32>& prefix_code){
    std::vector<u32>::iterator it = std::find(prefix_code.begin(), prefix_code.end(), code);

    if (it == prefix_code.end())
        return false;

    // Get index of element from iterator
    value = std::distance(prefix_code.begin(), it);
    return true;
}

// Decodes the run length encoding in @compressed and outputs the original string
void decode_RLE1(std::vector<u16> compressed){
    u32 block_len = compressed.size();
    u32 run_length = 0;
    u16 run_val = -1;
    for (int i = 0; i < block_len; i++){
        u32 code = compressed.at(i);
        u8 value = 0;
        if (run_length == 4){
            find_code(code, value, fixed_run_codes);
            for (int k = 0; k < value; k++){
                std::cout.put(run_val);
            }
            // Reset the run
            run_length = 0;
        }else{
            find_code(code, value, fixed_literal_codes);

            if (run_length == 0){ // Immediately after a run length
                run_length++;
                run_val = value;
                std::cout.put(value);
            }else{
                if (value == run_val){ // Part of a run
                    run_length++;
                    std::cout.put(value);
                }else{ // Distinct from the previous value
                    run_length = 1;
                    run_val = value;
                    std::cout.put(value);
                }
            }
        }
    }
}

// Reads @block_len characters from the input stream and decodes them as a run length encoding
// Returns a vector of the decompressed data
std::vector<u16> decode_RLE2(InputBitStream& stream, const u32& block_len){
    std::vector<u16> decoded(MAX_BLOCK_SIZE * 2);
    u32 idx_decoded = 0;
    u32 run_length = 0;
    int run_val = -1;
    for (int i = 0; i < block_len; i++){
        if (run_length == 4){
            u32 code = stream.read_bits(8); // Run lengths use 8 bits
            u8 value = 0;
            find_code(code, value, fixed_run_codes);
            for (int k = 0; k < value; k++){
                decoded.at(idx_decoded++) = run_val;
            }
            // Reset the run
            run_length = 0;
            run_val = -1;
        }else{
            u32 code = stream.read_bits(8);
            u8 value = 0;
            find_code(code, value, fixed_literal_codes);

            if (run_length == 0){ // Immediately after a run length
                run_length++;
                run_val = value;
                decoded.at(idx_decoded++) = value;
            }else{
                if (value == run_val){ // Part of a run
                    run_length++;
                    decoded.at(idx_decoded++) = value;
                }else{ // Distinct from the previous value
                    run_length = 1;
                    run_val = value;
                    decoded.at(idx_decoded++) = value;
                }
            }
        }
    }
    decoded.resize(idx_decoded);
    return decoded;
}

// Reads @block_len characters from the stream, finds their code, and outputs them
void decode_literals(InputBitStream& stream, const u32& block_len){
    for (int i = 0; i < block_len; i++){
        u32 code = stream.read_bits(8);
        u8 value = 0;
        find_code(code, value, fixed_literal_codes);
        std::cout.put(value);
    }
}

// If first bit is 0 (not last) and second bit of block is 0, no RLE is used - the compressed block has length of max block_size
// Else there's a 2 byte block length
int main(){

    init();
    InputBitStream stream {std::cin};
    u8 MAG1 = stream.read_byte();
    u8 MAG2 = stream.read_byte();
    if (MAG1 != MAGIC1 || MAG2 != MAGIC2){
        std::cerr << "Incorrect magic number. Aborting.\n";
        return EXIT_FAILURE;
    }

    // TODO: check if the input file was empty (ie. if the only contents of the compressed file are the magic number)
    while (1){
        u32 is_last = stream.read_bit();
            u32 uses_RLE = stream.read_bit();
            u32 block_len = stream.read_bits(BLOCK_LEN_NUM_BITS);
            u32 bwt_row = stream.read_bits(BWT_INDEX_NUM_BITS);
        if (is_last){
            // Decode the block, then exit

            if (uses_RLE){
                BWT_item bwt;
                bwt.row = bwt_row;
                //std::cout <<"OKAY\n";
                bwt.last_col = decode_RLE2(stream, block_len);
                std::vector<u16> inverted_bwt = invert_BWT(bwt);
                decode_RLE1(inverted_bwt);
            }else{
                // Contains only literals
                decode_literals(stream, block_len);
            }
            break;
        }else{
            if (uses_RLE){
                // Decode the fancy stuff
                BWT_item bwt;
                bwt.row = bwt_row;
                bwt.last_col = decode_RLE2(stream, block_len);
                std::vector<u16> inverted_bwt = invert_BWT(bwt);
                decode_RLE1(inverted_bwt);
            }else{
                // Contains MAX_BLOCK_SIZE literals
                decode_literals(stream, MAX_BLOCK_SIZE);
            }
        }
    }

    return 0;
}