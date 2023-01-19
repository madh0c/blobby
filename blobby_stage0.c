// blobby.c
// blob file archiver
// COMP1521 20T3 Assignment 2
// Written by Jeffery Pan (z5310210)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// the first byte of every blobette has this value
#define BLOBETTE_MAGIC_NUMBER          0x42

// number of bytes in fixed-length blobette fields
#define BLOBETTE_MAGIC_NUMBER_BYTES    1
#define BLOBETTE_MODE_LENGTH_BYTES     3
#define BLOBETTE_PATHNAME_LENGTH_BYTES 2
#define BLOBETTE_CONTENT_LENGTH_BYTES  6
#define BLOBETTE_HASH_BYTES            1

// mode byte shift increments to construct "mode integer"
#define MODE_BYTE_1_SHIFT 16
#define MODE_BYTE_2_SHIFT 8


// maximum number of bytes in variable-length blobette fields
#define BLOBETTE_MAX_PATHNAME_LENGTH   65535
#define BLOBETTE_MAX_CONTENT_LENGTH    281474976710655


// misc
#define BITS_IN_BYTE 8



typedef enum action {
    a_invalid,
    a_list,
    a_extract,
    a_create
} action_t;


void usage(char *myname);
action_t process_arguments(int argc, char *argv[], char **blob_pathname,
                           char ***pathnames, int *compress_blob);

void list_blob(char *blob_pathname);
void extract_blob(char *blob_pathname);
void create_blob(char *blob_pathname, char *pathnames[], int compress_blob);

uint8_t blobby_hash(uint8_t hash, uint8_t byte);


// ADD YOUR FUNCTION PROTOTYPES HERE
long blobbete_mode(FILE *fp, long curr_byte);
unsigned long blobbete_name_content_len(FILE *fp, long curr_byte, 
                        char pathname[BLOBETTE_MAX_PATHNAME_LENGTH]);


// YOU SHOULD NOT NEED TO CHANGE main, usage or process_arguments

int main(int argc, char *argv[]) {
    char *blob_pathname = NULL;
    char **pathnames = NULL;
    int compress_blob = 0;
    action_t action = process_arguments(argc, argv, &blob_pathname, &pathnames,
                                        &compress_blob);

    switch (action) {
    case a_list:
        list_blob(blob_pathname);
        break;

    case a_extract:
        extract_blob(blob_pathname);
        break;

    case a_create:
        create_blob(blob_pathname, pathnames, compress_blob);
        break;

    default:
        usage(argv[0]);
    }

    return 0;
}

// print a usage message and exit

void usage(char *myname) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\t%s -l <blob-file>\n", myname);
    fprintf(stderr, "\t%s -x <blob-file>\n", myname);
    fprintf(stderr, "\t%s [-z] -c <blob-file> pathnames [...]\n", myname);
    exit(1);
}

// process command-line arguments
// check we have a valid set of arguments
// and return appropriate action
// **blob_pathname set to pathname for blobfile
// ***pathname set to a list of pathnames for the create action
// *compress_blob set to an integer for create action

action_t process_arguments(int argc, char *argv[], char **blob_pathname,
                           char ***pathnames, int *compress_blob) {
    extern char *optarg;
    extern int optind, optopt;
    int create_blob_flag = 0;
    int extract_blob_flag = 0;
    int list_blob_flag = 0;
    int opt;
    while ((opt = getopt(argc, argv, ":l:c:x:z")) != -1) {
        switch (opt) {
        case 'c':
            create_blob_flag++;
            *blob_pathname = optarg;
            break;

        case 'x':
            extract_blob_flag++;
            *blob_pathname = optarg;
            break;

        case 'l':
            list_blob_flag++;
            *blob_pathname = optarg;
            break;

        case 'z':
            (*compress_blob)++;
            break;

        default:
            return a_invalid;
        }
    }

    if (create_blob_flag + extract_blob_flag + list_blob_flag != 1) {
        return a_invalid;
    }

    if (list_blob_flag && argv[optind] == NULL) {
        return a_list;
    } else if (extract_blob_flag && argv[optind] == NULL) {
        return a_extract;
    } else if (create_blob_flag && argv[optind] != NULL) {
        *pathnames = &argv[optind];
        return a_create;
    }

    return a_invalid;
}


// list the contents of blob_pathname

void list_blob(char *blob_pathname) {   
    FILE *fp = fopen(blob_pathname, "r");
    long curr_byte = fgetc(fp);

    while (curr_byte != EOF) {     
        // check magic number of current blobette
        if (curr_byte != BLOBETTE_MAGIC_NUMBER) {
            fprintf(stderr, "ERROR: Magic byte of blobette incorrect\n");
            exit(1);
        }       

        // obtain its mode 
        long mode = blobbete_mode(fp, curr_byte); 

        // find pathname and the length of contents
        char pathname[BLOBETTE_MAX_PATHNAME_LENGTH];
        unsigned long content_length = blobbete_name_content_len(fp, curr_byte, pathname);    

        fseek(fp, content_length + 1, SEEK_CUR); // + 1 for hash
        
        // set curr_byte to 1st byte of next blobbete
        curr_byte = fgetc(fp);

        printf("%06lo %5lu %s\n", mode, content_length, pathname);
    }
    
    return;
}


// extract the contents of blob_pathname

void extract_blob(char *blob_pathname) {
    // create function that combine fgetc and blobby hash



    // REPLACE WITH YOUR CODE FOR -x

    printf("extract_blob called to extract '%s'\n", blob_pathname);
}

// create blob_pathname from NULL-terminated array pathnames
// compress with xz if compress_blob non-zero (subset 4)

void create_blob(char *blob_pathname, char *pathnames[], int compress_blob) {

    // REPLACE WITH YOUR CODE FOR -c

    printf("create_blob called to create %s blob '%s' containing:\n",
           compress_blob ? "compressed" : "non-compressed", blob_pathname);

    for (int p = 0; pathnames[p]; p++) {
        printf("%s\n", pathnames[p]);
    }

}


// ADD YOUR FUNCTIONS HERE

// extract the bytes of mode, construct them together 
// then return as a long int
long blobbete_mode(FILE *fp, long curr_byte) {
    long mode = 0;
    int shift = (BLOBETTE_MODE_LENGTH_BYTES - 1) * BITS_IN_BYTE;
    while (shift >= 0) {
        curr_byte = fgetc(fp);
        mode = mode | (curr_byte << shift);
        shift -= BITS_IN_BYTE;
    }
    return mode;
}

// given a character array of BLOBETTE_MAX_PATHNAME_LENGTH
// find the pathname and insert it into the array.
// also return content length of blobette
unsigned long blobbete_name_content_len(FILE *fp, long curr_byte, 
                                        char pathname[BLOBETTE_MAX_PATHNAME_LENGTH]) {
    // find length of pathname by constructing 2 bytes
    int pathname_length = 0;
    curr_byte = fgetc(fp);
    pathname_length = pathname_length | (curr_byte << BITS_IN_BYTE);
    curr_byte = fgetc(fp);
    pathname_length = pathname_length | curr_byte;

    // find length of contents by constructing 6 bytes
    unsigned long content_length = 0;
    int shift = (BLOBETTE_CONTENT_LENGTH_BYTES - 1) * BITS_IN_BYTE; 
    while (shift >= 0) {
        curr_byte = fgetc(fp);
        content_length = content_length | (curr_byte << shift);
        shift -= BITS_IN_BYTE;
    }  

    // extract characters of pathname and insert them into the string
    int i = 0;
    while (i < pathname_length) {
        pathname[i] = fgetc(fp);
        i++;
    }
    pathname[i] = '\0';

    return content_length;
} 


/*/ fgetc + get hash
long fgetc_hash(FILE *fp, int *hash) {
    uint8_t hash = 0;
    long byte = fgetc(fp);
    *hash = blobby_hash(hash, byte);
    return byte;
}*/

// YOU SHOULD NOT CHANGE CODE BELOW HERE

// Lookup table for a simple Pearson hash
// https://en.wikipedia.org/wiki/Pearson_hashing
// This table contains an arbitrary permutation of integers 0..255

const uint8_t blobby_hash_table[256] = {
    241, 18,  181, 164, 92,  237, 100, 216, 183, 107, 2,   12,  43,  246, 90,
    143, 251, 49,  228, 134, 215, 20,  193, 172, 140, 227, 148, 118, 57,  72,
    119, 174, 78,  14,  97,  3,   208, 252, 11,  195, 31,  28,  121, 206, 149,
    23,  83,  154, 223, 109, 89,  10,  178, 243, 42,  194, 221, 131, 212, 94,
    205, 240, 161, 7,   62,  214, 222, 219, 1,   84,  95,  58,  103, 60,  33,
    111, 188, 218, 186, 166, 146, 189, 201, 155, 68,  145, 44,  163, 69,  196,
    115, 231, 61,  157, 165, 213, 139, 112, 173, 191, 142, 88,  106, 250, 8,
    127, 26,  126, 0,   96,  52,  182, 113, 38,  242, 48,  204, 160, 15,  54,
    158, 192, 81,  125, 245, 239, 101, 17,  136, 110, 24,  53,  132, 117, 102,
    153, 226, 4,   203, 199, 16,  249, 211, 167, 55,  255, 254, 116, 122, 13,
    236, 93,  144, 86,  59,  76,  150, 162, 207, 77,  176, 32,  124, 171, 29,
    45,  30,  67,  184, 51,  22,  105, 170, 253, 180, 187, 130, 156, 98,  159,
    220, 40,  133, 135, 114, 147, 75,  73,  210, 21,  129, 39,  138, 91,  41,
    235, 47,  185, 9,   82,  64,  87,  244, 50,  74,  233, 175, 247, 120, 6,
    169, 85,  66,  104, 80,  71,  230, 152, 225, 34,  248, 198, 63,  168, 179,
    141, 137, 5,   19,  79,  232, 128, 202, 46,  70,  37,  209, 217, 123, 27,
    177, 25,  56,  65,  229, 36,  197, 234, 108, 35,  151, 238, 200, 224, 99,
    190
};

// Given the current hash value and a byte
// blobby_hash returns the new hash value
//
// Call repeatedly to hash a sequence of bytes, e.g.:
// uint8_t hash = 0;
// hash = blobby_hash(hash, byte0);
// hash = blobby_hash(hash, byte1);
// hash = blobby_hash(hash, byte2);
// ...

uint8_t blobby_hash(uint8_t hash, uint8_t byte) {
    return blobby_hash_table[hash ^ byte];
}
