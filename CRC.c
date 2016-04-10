#include "bitops.h"
#include "unistd.h"

/**
 * Author: Olivia Lucca Fraser
 * ID: B00109376
 * Date: October 25, 2015
 **/

#define MINARGS 2
#define DEFAULT_GENERATOR 0x04C11DB7

#define LOG stdout

#define SEND_RECV 2
#define SEND 1
#define RECV 0

bitarray_t * CRC(bitarray_t *message,
                 uint32_t generator,
                 unsigned char mode);

int verbose = 1;

int main(int argc, char **argv){

  FILE *fd = stdin;
  char fmt[8];
  int inputformat = 0;
  uint32_t input;
  char opt;
  char direction = SEND_RECV;
  char input_as_binary = FALSE;
  char output_binary_only = FALSE;
  int burst_length = 0;
  char random_msg = TRUE;
  int random_msg_size = 1520;
  uint32_t generator = DEFAULT_GENERATOR;

  if (is_big_endian()){
    fprintf(stderr, "*****************************************\n"
                    "Warning: this is a big-endian system.\n"
                    "The programme may not work as expected,\n"
                    "and is limited to using default settings,\n"
                    "due to compatibility issues with unistd.h\n"
                    "\nThis utility reads from stdin. If it is\n"
                    "hanging, hit Ctrl-C to exit, and then\n"
                    "relaunch it as the target of a pipe, i.e.\n"
                    "echo Hello World | ./CRC\n"
                    "*****************************************\n");
    goto skipopts;
  }
  // Parse the command line arguments. 
  if (argc < MINARGS)
    goto help;
  while ((opt = getopt(argc, argv, "srbvf:qg:ce:ho")) != -1){
    switch(opt) {
    case 'b':
      input_as_binary = TRUE;
      break;
    case 'c':
      input_as_binary = FALSE;
      break;
    case 's':
      direction = SEND;
      break;
    case 'o':
      output_binary_only = TRUE;
      verbose = FALSE;
      break;
    case 'r':
      direction = RECV;
      break;
    case 'g':
      if (optarg[0] == '0' && optarg[1] == 'x')
        sscanf(optarg,"0x%x",&generator);
      else
        sscanf(optarg, "%d",&generator);
      break;
    
    case 'v':
      verbose = TRUE;
      break;
    case 'f':
      if (optarg[0] == '-')
        break;
      fd = fopen(optarg,"r");
      if (fd == NULL){
        fprintf(stderr, "Error opening %s. Exiting.\n",optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'q':
      verbose = FALSE;
      break;
    case 'e':
      burst_length = atoi(optarg);
      break;
    case 'h':
    default:
    help:
      printf(GREEN"CRC UTILITY\n"
             YELLOW"=-=-=-=-=-=\n"COLOR_RESET
             "Usage: %s [OPTIONS] \n"
             "-v: verbose\n"
             "-q: quiet (diable verbose output)\n"
             "-f <filename>: supply file as input, instead of stdin\n"
             "-f - : read from stdin (default)\n"
             "-s: execute send phase only; append remainder, but do not check\n"
             "-r: execute receive phase only; check for remainder, but don't append\n"
             "-b: read input as binary string of ASCII '0's and '1's\n"
             "-c: read input as raw characters [default]\n"
             "-o: output bitstring only: use with -b to chain CRC pipes together\n"
             "-e <burst length>: introduce burst error of <burst length> bits\n"
             "-g <generator>: supply alternate CRC polynomial in hex or decimal\n"
             "-h: display this help menu.\n"
             , argv[0]);
      exit(EXIT_FAILURE);
      break;
    }
  }
 skipopts:
  opt=0;
  bitarray_t *orig_msg;

  // Read the input as ASCII '0's and '1's if requested, and
  // convert to an actual bitarray
  if (input_as_binary == FALSE){ 
    char *txt_msg = read_characters(fd, EOF);
    orig_msg = make_bitarray(txt_msg, strlen(txt_msg));
    free(txt_msg);
  } else {
    orig_msg = read_binary(fd);
  }
  
  if (verbose){
    fprintf(LOG,"MESSAGE READ: %s\n",orig_msg->array);
    fprintf(LOG,"IN BINARY:    ");
    print_bitarray(LOG, orig_msg);
    fprintf(LOG,"\n");
  }

  // Calculate CRC remainder, and append it to the message.
  // If not sending, just alias prep_msg to orig_msg
  bitarray_t *prep_msg = (direction >= SEND)?
    CRC(orig_msg, generator, SEND) : orig_msg;
  
  // Introduce a burst error, if requested (by command-line option
  // -e <length>). This may be either a burst of 1s or a burst of 0s. 
  if (burst_length){
    burst_error(prep_msg->array, prep_msg->end/8,
                burst_length, (char) clock()%2);
  }

  // Check the resulting message for bit errors. If no burst errors
  // have been introduced, then no errors should be reported.
  // If not receiving, then just alias recv_msg to prep_msg
  bitarray_t *recv_msg = (direction % SEND_RECV == RECV)?
    CRC(prep_msg, generator, RECV) : prep_msg;

  // Return a 1 if there is a residue, 0 otherwise. To see the
  // actual residue, verbose should be enabled. Residue is stored
  // in bitarray_t field named 'residue'.
  unsigned char retval = (unsigned char) !!recv_msg->residue;

  if (verbose) {
    if (!recv_msg->residue) {
      fprintf(LOG, "%s\n", (direction != SEND)?
              "NO CORRUPTION DETECTED." : "NO RESIDUE GENERATED.");
    } else {
      if (direction != SEND)
        fprintf(LOG, "*** CORRUPTION DETECTED ***\n");
      fprintf(LOG, "*** RESIDUE: 0x%lx\n",
              (unsigned long int) recv_msg->residue);
    }
  }
  
  if (output_binary_only){
    print_bitarray(stdout, recv_msg);
    printf("\n");
 }
  
  // Clean up the heap
  
  if (direction == SEND_RECV) destroy_bitarray(orig_msg);
  if (direction == SEND || direction == SEND_RECV) destroy_bitarray(prep_msg);
  if (direction == RECV || direction == SEND_RECV) destroy_bitarray(recv_msg);
  
  return retval;
}


bitarray_t * CRC(bitarray_t *message,
                 uint32_t generator,
                 unsigned char mode){

  int shiftbitlen = 0;
  uint32_t g = generator;
  while ((g >>= 1) != 0)
    shiftbitlen ++;

  // It will be helpful to have a mask for grabbing the high bit of the reg.
  uint32_t shiftreg_highmask = (0x1 << (shiftbitlen-1));
  uint32_t shiftreg_cropmask = ~(0xffffffff << (shiftbitlen)); // mask was too small
  // We drop the MSB of the generator when determining our xor gates.
  uint32_t xorplate = (generator) & shiftreg_cropmask;
  
  /////////
  if (verbose){
    fprintf(LOG, "XORPLATE:     ");
    fprint_lint_bits(LOG, xorplate);
    fprintf(LOG,"\n");
  }
  //////////
  
  bitarray_t *bitmsg_out;

  bitmsg_out = calloc(1,sizeof(bitarray_t));
  bitmsg_out->array = calloc((message->end/8) + shiftbitlen/8 + 2, sizeof(char));
  bitmsg_out->end = message->end; // + ((send == SEND)? shiftbitlen : 0); //??
  bitmsg_out->residue = 0;
  bitmsg_out->size = message->end/4; // todo: tidy up and put inside a builder function
  
  uint32_t bit_index = 0;

  chunky_integer_t shiftreg;
  memset(&shiftreg,0,sizeof(uint32_t));
  
  char *shiftreg_string;
  char *msg_bitstring;
  char xored = 0;
  uint32_t topbit = 0, bit = 0;
  /*
  fprintf(stderr,"IN:  ");
  print_bitarray(stderr, message);
  fprintf(stderr,"\n");
  */
  //////////////////////////////////////////////////////////////
  while (bit_index < bitmsg_out->end+shiftbitlen){
    
    bit = getbit(message->array, bit_index);
    bit_index ++;
    shiftreg.integer = (shiftreg.integer << 1) & shiftreg_cropmask;
    shiftreg.integer |= bit;
    // When the MSB of the shift register is 1, perform XOR operation
    if (topbit){
      shiftreg.integer ^= xorplate; ///xorplate;
      if (verbose) xored = 1;
    } 
  
    topbit = shiftreg.integer & shiftreg_highmask;

    if (verbose){ 
      shiftreg_string = stringify_chunky(&shiftreg, shiftbitlen);
      fprintf(LOG, "[%2.2d] SHIFTREG: %s  FED: %d  %s\n", bit_index,
              shiftreg_string, bit,
              xored? "XOR EVENT" : "");
      free(shiftreg_string);
      xored = 0;
    }
  }

  // Some special attention is needed for porting this to
  // big-endian architectures, like PowerPC (experimental)
  if (is_big_endian())
    shiftreg.integer = end_reverse(shiftreg.integer);

  int j = 0;
  while (j < (message->end/8)+1)
    bitmsg_out->array[j] = message->array[j++];
  
  if (is_big_endian())
    shiftreg.integer = end_reverse(shiftreg.integer);

  // Whether or not we append it to the message, we always store the
  // residue in the "residue" field of the struct, so that it can be
  // treated as a return value of the function, along with the msg.
  // This is just a convenience. We could extract it by counting back
  // from bitmsg_out->end, if we know the generator in advance. 
  bitmsg_out->residue = shiftreg.integer;

  // When in "SEND" mode, append the remainder to the end of the msg 
  int i;
  if (mode == SEND) {
    for (i = shiftbitlen-1; i >= 0; i --){
      bitarray_push(bitmsg_out, getbit(shiftreg.bytes,i));
      // More verbose output:
      if (verbose)
        fprintf(LOG, "(%d) copying %d from shiftreg to"
                " bitmsg_out bit #%d\n", i,getbit(shiftreg.bytes,i),
                bitmsg_out->end-1);
     }
  }

  // More verbose output:
  if (verbose){
    fprintf(LOG,"IN:  ");
    print_bitarray(LOG, message);
    fprintf(LOG,"\n");
    fprintf(LOG,"OUT: ");
    print_bitarray(LOG, bitmsg_out);
    fprintf(LOG,"\n\n");
  }
  

  return bitmsg_out;
}
