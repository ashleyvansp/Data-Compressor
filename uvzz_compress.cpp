/* uvzz_compress.cpp
   CSC 485B
   Ashley Van Spankeren
*/

/* 
    Some code has been repurposed from my Assignment 2 submission
*/
#include <iostream>
#include "./stream/output_stream.hpp"
#include "./uvzz_necessities.hpp"
using Block = std::array<u8, MAX_BLOCK_SIZE>;
std::vector<u32> fixed_literal_code_lengths(256);
std::vector<u32> fixed_literal_codes(256);
std::vector<u32> fixed_run_code_lengths(256);
std::vector<u32> fixed_run_codes(256);

/*
    Given that the BWT index uses 12 bits, the maximum block size can be at most 2^12 = 4096
*/
/*
    Block header format:
    [is_last][uses_RLE][block_length][BWT index]
    [1 bit]   [1 bit]     [12 bits]  [12 bits]
*/


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

// If a run has length greater than 3, the first 4 constants are encoded as literals, 
// followed by a 1-byte number of the remaining run length
// Hence the maximum run length is 259
std::vector<u16> RLE(const std::vector<u16>& block, const u32& block_len){
    std::vector<u16> rle(block_len * 2); // In case of worst case expansion
    u32 rle_ind = 0;
    u32 i = 0;
    u16 run_val = block.at(0);
    u32 run_length = 0;
    while (i < block_len){
        run_val = block.at(i);
        run_length = 0;
        // Matches no matter what for one iteration, so run length > 0 after the loop
        while (i < block_len && run_length < 259 && block.at(i) == run_val){
            run_length++;
            i++;
        }
        if (run_length < 4){
            // No notable run
            for (int k = 0; k < run_length; k++){
                rle.at(rle_ind++) = run_val;
            }
        } else{
            for (int k = 0; k < 4; k++){
                rle.at(rle_ind++) = run_val;
            }
            rle.at(rle_ind++) = run_length - 4;
        }
    }
    rle.resize(rle_ind);

    return rle;
}

// Just converts a block into vector format so we can apply RLE (there's gotta be a better way to do this but I dunno what that way is yet)
std::vector<u16> RLE_wrapper(const Block& block, const u32& block_len){
    std::vector<u16> new_block(block_len);
    for (int i = 0; i < block_len; i++){
        new_block.at(i) = block.at(i);
    }
    std::vector<u16> rle = RLE(new_block, block_len);
    return rle;
}

// Left rotates @block by the number of times specified by @num_rots
std::vector<u16> rotate(const std::vector<u16> block, u32 num_rots){
    std::vector<u16> first_half(block.begin(), block.begin() + num_rots + 1);
    std::vector<u16> second_half(block.begin() + num_rots + 1, block.end());
    second_half.insert(second_half.end(), first_half.begin(), first_half.end());

    return second_half;
}

// Applies the Burrows Wheeler Transform to the block
//  TODO upgrade to use a circular queue
BWT_item BWT(const std::vector<u16>& block, const u32 &n){
    std::vector<std::vector<u16>> rotations(n, std::vector<u16>(n));
    for (u32 i = 0; i < n; i++){
        rotations.at(i) = rotate(block, i);
    }
    std::sort(rotations.begin(), rotations.end());
    std::vector<u16> last_col(n);
    for (u32 i = 0; i < n; i++){
        std::vector<u16> row = rotations.at(i);
        last_col.at(i) = row.at(n - 1);
    }
    BWT_item solution;
    solution.last_col = last_col;
    std::vector<std::vector<u16>>::iterator it = std::find(rotations.begin(), rotations.end(), block);
    solution.row = std::distance(rotations.begin(), it);

    return solution;
}

// Applies the fixed prefix codes to the vector @rle and outputs the codes to @stream
void write_RLE(OutputBitStream& stream, std::vector<u16>& rle){
    u8 run_length = 0;
    int run_val = -1;
    for (int i = 0; i < rle.size(); i++){
        if (run_length == 4){
            stream.push_bits(fixed_run_codes.at(rle.at(i)), fixed_run_code_lengths.at(rle.at(i)));
            run_length = 0;
        }else{
            u32 code = fixed_literal_codes.at(rle.at(i));
            u32 bits = fixed_literal_code_lengths.at(rle.at(i));
            if (run_length == 0){ // Immediately after a run
                run_length++;
                run_val = rle.at(i);
                stream.push_bits(code, bits);
            }else{
                if (rle.at(i) == run_val){ // Part of a run
                    run_length++;
                    stream.push_bits(code, bits);
                }else{ // Distinct from the previous value
                    run_length = 1;
                    run_val = rle.at(i);
                    stream.push_bits(code, bits);
                }
            }
        }
    }
}

// Stage 1 - RLE
// Stage 2 - BWT
// Stage 3 - RLE
// Stage 4 - Huffman code
void write_block(OutputBitStream& stream, Block block, u32 block_size, bool is_last){
    stream.push_bit(is_last ? 1 : 0);
    std::vector<u16> rle = RLE_wrapper(block, block_size);
    BWT_item bwt = BWT(rle, rle.size());
    rle = RLE(bwt.last_col, bwt.last_col.size());

    if (rle.size() >= block_size){
        // Expansion - just output literals
        stream.push_bit(0);
            // Need to specify length
        stream.push_bits(block_size, BLOCK_LEN_NUM_BITS);
        stream.push_bits(0, BWT_INDEX_NUM_BITS);
        for (int i = 0; i < block_size; i++){
            stream.push_bits(fixed_literal_codes.at(block.at(i)), fixed_literal_code_lengths.at(block.at(i)));
        }
    }else{
        // Output the fancy stuff
        stream.push_bit(1);
        stream.push_bits(rle.size(), BLOCK_LEN_NUM_BITS);
        stream.push_bits(bwt.row, BWT_INDEX_NUM_BITS);

        write_RLE(stream, rle);
    }
}

// Set up the fixed prefix codes
void init(){
    for (int i = 0; i < 256; i++){
        fixed_literal_code_lengths.at(i) = 8;
        fixed_run_code_lengths.at(i) = 8;
    }

    fixed_literal_codes = construct_canonical_code(fixed_literal_code_lengths);
    fixed_run_codes = construct_canonical_code(fixed_run_code_lengths);
}

int main(){
    init();
    OutputBitStream stream {std::cout};
    // Push the magic number
    stream.push_bytes(MAGIC1, MAGIC2);

    Block block_contents {};
    u32 block_size {0};
    char next_byte {}; //Note that we have to use a (signed) char here for compatibility with istream::get()

    if (!std::cin.get(next_byte)){
        //Empty input?
    }else{
        //Read through the input
        while(1){
            block_contents.at(block_size++) = next_byte;
            if (!std::cin.get(next_byte))
                break;

            //If we get to this point, we just added a byte to the block AND there is at least one more byte in the input waiting to be written.
            if (block_size == block_contents.size()){
                //The block is full, so write it out.
                //We know that there are more bytes left, so this is not the last block
                write_block(stream, block_contents, block_size, false);
                block_size = 0;
            }
        }
    }

    //At this point, we've finished reading the input (no new characters remain), and we may have an incomplete block to write.
    if (block_size > 0){
        //Write out any leftover data
        write_block(stream, block_contents, block_size, true);

        block_size = 0;
    }

    return 0;
}