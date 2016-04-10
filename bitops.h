#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

/**
 * Bitops library: a collection of useful, bit-twisting functions
 * that are employed in both CRC.c and bitstuff.c. 
 *
 * Author: Olivia Lucca Fraser
 * ID: B00109376
 * Date: October 25, 2015
 **/

#define ON 1
#define OFF 0

#define TRUE 1
#define FALSE 0

#define LEFT  +1
#define RIGHT -1

#define BIG 0x0b
#define LITTLE 0x00

#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define CYAN    "\x1b[36m"
#define MAGENTA "\x1b[35m"
#define COLOR_RESET   "\x1b[0m"

// A handy bitarray structure, used prominently in CRC.c.
typedef struct bitarray {
  uint8_t *array;
  uint32_t end; // bit index of last bit + 1
  uint32_t residue;
  uint32_t size;
} bitarray_t;

// Sometimes, we want to be able to treat an integer as
// an array of bytes. This type helps you do that. 
typedef union chunky_integer {
  uint32_t integer;
  unsigned char bytes[4];
} chunky_integer_t;

/**
 * Returns 1 if operating on a big-endian architectures, and
 * 0 otherwise. 
 **/
int is_big_endian(void){
  chunky_integer_t checker;
  checker.integer = 0x0bffff00;
  return (checker.bytes[0] == BIG);
}

/**
 * Makes a bitarray_t structure, given a byte/char array, and 
 * its length.
 * 
 * @return bitarray_t
 * @param  char *arr : the array of bytes to use 
 * @param  int len   : the length of the initial byte array
 **/
bitarray_t * make_bitarray(char *arr, int len){
  bitarray_t *ba = calloc(1,sizeof(bitarray_t));
  ba->size = len*2;
  ba->array = calloc(ba->size, sizeof(char));
  memcpy(ba->array, arr, len);

  ba->end = len*8;
  ba->residue = 0;
  return ba;
}

/**
 * For cleaning up your heap.
 *
 * @param bitarray_t *ba: A pointer to the bitarray to destroy
 **/
void destroy_bitarray(bitarray_t *ba){
  free(ba->array);
  free(ba);
}

/**
 * Set the bit at the specified bit index to the specified value. 
 * Think of this as allowing subscripting for bits, without the sugar. 
 * 
 * @param unsigned char *byte     : pointer to the byte or byte array
 * @param unsigned long int index : location of the bit to set
 * @param unsigned char bit       : 1 or 0, please. 
 **/
void setbit(unsigned char *byte, unsigned long int index,
            unsigned char bit){
  bit %= 2;
  unsigned long int byte_index = index / 8;
  unsigned char bit_index = index % 8;
  char mask = 1 << bit_index;
  *(byte + byte_index) = bit?
    *(byte + byte_index) | mask :
    *(byte + byte_index) & ~mask;
}

/**
 * Flip the bit, specified by an index, of a given byte or byte
 * array. 
 * 
 * @param unsigned char *byte     : pointer to byte or byte array
 * @param unsigned long int index : index of bit to be flipped
 **/       
void flipbit(unsigned char *byte, unsigned long int index){
  unsigned int byte_index = index / 8;
  unsigned char bit_index = index % 8;
  char mask = 1 << bit_index;
  *(byte + byte_index) ^= mask;
}

/**
 * Return the indexed bit from a byte or array of bytes,
 * in the form of a byte-sized mask, preserving place value. 
 * The relative index of the LSb of each byte is 0. 
 *
 * @param const unsigned char *byte : pointer to byte or byte array
 * @param unsigned long int index   : index of bit to return
 **/
unsigned char getbitasormask(const unsigned char *byte,
                             unsigned long int index){
  unsigned long int byte_index = index / 8;
  unsigned long int bit_index = index % 8;
  unsigned char mask = 1 << bit_index;
  return (*(byte + byte_index) & mask) ;
}

/**
 * Return the indexed bit of a byte or array of bytes, as a 1 or
 * a 0. The relative index of the LSb in each byte is 0.  
 * 
 * @return unsigned char : either 0 or 1, the bit fetched. 
 * @param const unsigned char *byte : pointer to byte or byte array
 * @param unsigned long int index   : index of bit to get
 **/
unsigned char getbit(const unsigned char *byte, unsigned long int index){
  return !!(getbitasormask(byte, index));
}

/**
 * Treat a bitarray_t struct's array field as a bit-stack, and push
 * a new bit on top. The bitarray keeps track of its last bit with the
 * end field, and this function increments that field after pushing. 
 * If the bitarray is running out of space, then this function will 
 * double the size of its array.
 * 
 * @param bitarray_t *ba    : pointer to bitarray
 * @param unsigned char bit : 0 or 1: the bit to push. 
 **/
void bitarray_push(bitarray_t *ba, unsigned char bit){
  setbit(ba->array, (ba->end)++, bit%2);
  if ((ba->end * 8) >= ((ba->size * 4) / 3)){
    ba->size *= 2;
    uint8_t *newarray = calloc (ba->size, sizeof(uint8_t));
    memcpy(newarray, ba->array, ba->size/2);
    free(ba->array);
    ba->array = newarray;
  }
}

/**
 * Treat a bitarray_t struct's array field as a bit-stack, and pop
 * the topmost bit, returning it. The bitarray keeps track of its 
 * 'top' with the end field, which this function decrements. 
 * 
 * @return unsigned char
 **/
unsigned char bitarray_pop(bitarray_t *ba){
  if (!ba->end){
    fprintf(stderr,"ERROR: Attempt to pop an empty bitarray.\n");
    exit(EXIT_FAILURE);
  }
    
  return getbit(ba->array, --(ba->end));
}

/**
 * Convert a byte-array to a human-readable string of '1' and 
 * '0' chars.
 *
 * @return pointer to bitstring, on the heap. Free after using. 
 * @param unsigned char byte: the byte to convert
 *        int len: length of the byte array to convert, in bytes
 **/
char * bytes2bitstring(const unsigned char *byte, int len){
  int i;
  unsigned char *bytearray;
  bytearray = malloc (sizeof(char)*len);
  memcpy(bytearray,byte, len);
  int byte_index = 0;
  int spaces = 0;
  char *arr = malloc((len*9 + 1) * sizeof(bytearray));
  int octets = 0;
  while (byte_index < len){
    for (i=7; i >= 0; i--){
      *(arr + spaces + byte_index*8 + i) = '0'+(*(bytearray + byte_index) & 1);
      *(bytearray + byte_index) >>= 1;
    }
    if (byte_index < len-1)
      *(arr + spaces++ + byte_index*8 + 8)
        = (octets % 8 == 0)? '\n' : ' ';
    byte_index ++;
  }
  *(arr + spaces + byte_index*8 + 8) = '\0';
  free(bytearray);
  return arr;
}

/**
 * Convert a single byte to a human-readable string of '1' and 
 * '0' chars.
 *
 * @return pointer to bitstring, on the heap. Free after using. 
 * @param unsigned char byte: the byte to convert
 *        int len: length of the byte array to convert, in bytes
 **/
void byte2bitstring(unsigned char byte, unsigned char *arr){
  int i;
  for (i=7; i >= 0; i--){
    *(arr + i) = '0'+(byte & 1);
    byte >>= 1;
  }
}

/**
 * Introduce a burst error of errbitlen bits into an 
 * array of bytes (such as a string, or the array field
 * of a bitarray), at a random location. 
 *
 * @param unsigned char *message : the array to spoil
 * @param int msglen : the length of the message array
 * @param int errbitlen : the length of the burst error
 * @param int highlow : 0 or 1, depending on whether you 
 *        want a burst of low noise or high noise. 
 **/
void burst_error(unsigned char *message, int msglen,
                 int errbitlen, int highlow){
  int errbytes = errbitlen / 8;
  unsigned char errbytemask = 0xffff << (errbitlen % 8);
  int index = (msglen-errbytes-1 > 0)? rand() % (msglen-errbytes-1)
    : 0;
  if (errbytes) memset(message+index, highlow, errbytes);
  *(message + index + errbytes) &= errbytemask;
  return;
}

/**
 * Reverse the endianness of a 32-bit wide integer.
 *
 * @return uint32_t : the result of the operation
 * @param uint32_t l: the integer to be reversed
 **/ 
uint32_t end_reverse (uint32_t l){
  uint32_t r = 0;
  uint32_t mask = 0xff;
  uint8_t i = 0;
  for (i = 0; i < 4; i++){
    r <<= 8;
    r |= (mask & l) >> (i*8);
    mask <<= 8;    
  }
  return r;
}

/**
 * Print the bytes contained in a bitarray's array field
 * as a series of binary octets, each representing a byte. 
 * 
 * @param FILE *channel : the file decriptor to print to
 * @param bitarray_t *ba: a pointer to the bitarray to print
 **/
void print_bitarray_bytes (FILE *channel, bitarray_t *ba){
  char * s = bytes2bitstring(ba->array, ba->end/8 + 1);
  fprintf(channel, "%s", s);
  free(s);
}

/**
 * Print the bits contained in a bitarray's array field as 
 * an uninterrupted series of ASCII '0's and '1's. 
 *
 * @param FILE *channel : the file descriptor to print to
 * @param bitarray_t *ba: a pointer to the bitarray to print
 **/
void print_bitarray (FILE *channel, bitarray_t *ba){
  int i = 0;
  while (i < ba->end)
    fprintf(channel, "%d", getbit(ba->array, i++));
}

/**
 * Convert a bitarray's array field into a string of ASCII
 * '0's and '1's, without breaking it into octets.
 * 
 * @return char * : the resulting string
 * @param const bitarray_t *ba: pointer to the bitarray
 **/ 
char * stringify_bitarray (const bitarray_t *ba){
  char * s;
  s = calloc (1, sizeof(ba->array));
  int i = 0;
  while (i < ba->end)
    *(s + i) = getbit(ba->array, i++) + '0';
  return s;
}

/**
 * Convert a "chunky_integer" -- a uint32_t integer segmented into
 * byte-sized chunks -- into a string of ASCII '0's and '1's. This
 * function detects and responds to the system's endianness. 
 * 
 * @return char * : the resulting string
 * @param const chunky_integer_t *ci : pointer to the chunky int
 * @param int bitlen : the number of bits to stringify, not 
 *        necessarily all 32. 
 **/
char * stringify_chunky (const chunky_integer_t *ci, int bitlen){
  char * s;
  s = calloc (bitlen, sizeof(char));
  int i = 0;
  chunky_integer_t *standin = calloc (1, sizeof(chunky_integer_t));
  standin->integer = is_big_endian()? end_reverse(ci->integer) : ci->integer; 
  while (i < bitlen)
    *(s + ((bitlen-1)-i)) = getbit(standin->bytes, i++) + '0';
  free(standin);
  return s;                     
}

/**
 * Reads a series of ASCII '0's and '1's as a binary stream, and flexibly
 * allocate a bitarray struct on the heap to store them in. Remember to 
 * call free() after finishing with the bitarray. Stops reading at the 
 * first newline ('\n') character encountered.
 * 
 * @return bitarray_t * : pointer to the bitarray storing the bits read
 * @param FILE *channel : the channel to read from.
 **/
bitarray_t * read_binary (FILE *channel){
  int size = 0x100;
  char endsig = '\n';
  bitarray_t *ba = calloc(1,sizeof(bitarray_t));
  ba->array = calloc(size,sizeof(uint8_t));
  ba->end = 0;
  ba->size = size;
  ba->residue = 0;
  int b = 0, i = 0;
  char glyph;
  while (((glyph = fgetc(channel)) != endsig) && (!feof(channel))){
    if (glyph != '0' && glyph != '1')
      break;
    bitarray_push(ba,(glyph - '0'));
  }
  return ba;  
}

/**
 * Read a stream of characters from a file descriptor, and 
 * flexibly allocate an array to store them in. Remember to 
 * call free() when finished with the string. 
 * 
 * @return char * : a string containing the characters read
 * @param FILE *channel : the file descriptor to read from
 * @char endsig : the character at which to stop reading -- EOF
 *       or '\n' are potential candidates, but any may be used.
 **/
char * read_characters (FILE *channel, char endsig){
  long int size = 0x100;
  char * string = calloc(size,sizeof(uint8_t));
  int i = 0;
  char glyph;
  do {
    glyph = fgetc(channel);
    string[i++] = glyph;
    if (i >= (size*3)/4){
      size *= 2;
      uint8_t *copy = calloc(size,sizeof(uint8_t));
      memcpy(copy, string, i);
      free(string);
      string = copy;
    }
  } while (!feof(channel) && glyph != endsig);

  string[i] = '\0';
  if (glyph == endsig) string[i-1] = '\0';
  return string;
}

/**
 * Read up to a determinate number of characters from a given
 * file descriptor, and flexibly allocate an array to store 
 * them in, on the heap. Will also stop reading when an EOF
 * signal is enountered (via feof()). Remember to call free()
 * when finished with the string. 
 * 
 * @return char * : the string containing the chars read
 * @param FILE *channel : the file descriptor to read from
 * @param int n : the maximum number of characters to read.
 **/
char * read_n_characters (FILE *channel, int n){
  long int size = 0x100;
  char * string = calloc(size,sizeof(uint8_t));
  int i = 0;
  char glyph;
  do {
    glyph = fgetc(channel);
    string[i++] = glyph;
    if (i >= (size*3)/4){
      size *= 2;
      uint8_t *copy = calloc(size,sizeof(uint8_t));
      memcpy(copy, string, i);
      free(string);
      string = copy;
    }
  } while (!feof(channel) && i < n);

  string[i] = '\0';
  return string;
}

/**
 * Read n random bytes from /dev/urandom, and flexibly
 * allocate an array on the heap to store them in. Remember
 * to call free() when finished with the string.  
 * 
 * @return char * : the resulting string
 * @param int len : the number of bytes to read.
 **/ 
char * get_random_bytes(int len){
  FILE *urandom = fopen("/dev/urandom","r");
  return read_n_characters(urandom, len);
}

/**
 * Print the bits of a long integer to the specified channel,
 * as a sequence of ASCII '0's and '1's.
 * 
 * @param FILE *channel : the file descriptor to print to
 * @param long int l : the long integer to print
 **/
void fprint_lint_bits(FILE *channel, long int l){
  while (l != 0){
    int bit = l & 1;
    l >>= 1;
    fprintf(channel, "%d",bit);
  }
}


