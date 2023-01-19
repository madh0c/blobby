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

// maximum number of bytes in variable-length blobette fields
#define BLOBETTE_MAX_PATHNAME_LENGTH   65535
#define BLOBETTE_MAX_CONTENT_LENGTH    281474976710655


// misc
#define BITS_IN_BYTE 8
#define LAST_8_BITS 0xFF



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
long blobbete_mode(FILE *fp, long curr_byte, uint8_t *hash_p);
unsigned long blobbete_name_content_len(FILE *fp, long curr_byte,
                                        char pathname[BLOBETTE_MAX_PATHNAME_LENGTH],
                                        uint8_t *hash_p);
uint8_t fgetc_hash(FILE *fp, uint8_t *hash_p);


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

    // exit with error if no such directory or file
    if (fp == NULL) {
        perror(blob_pathname);
        exit(1);
    }

    // hashes are not checked in this function
    // but hash and hash pointer initialised to 
    // make sure sub-functions will run properly
    uint8_t hash = 0;
    uint8_t *hash_p = &hash;

    // loop to print metadata of each blobbete
    long curr_byte = fgetc(fp);
    while (curr_byte != EOF) {     
        
        // check magic number of current blobette
        if (curr_byte != BLOBETTE_MAGIC_NUMBER) {
            fprintf(stderr, "ERROR: Magic byte of blobette incorrect\n");
            exit(1);
        }       

        // obtain its mode 
        long mode = blobbete_mode(fp, curr_byte, hash_p); 

        // find pathname and the length of contents
        char pathname[BLOBETTE_MAX_PATHNAME_LENGTH];
        unsigned long content_length = blobbete_name_content_len(fp, curr_byte, pathname, hash_p);    

        // seek till end of the current blobette
        fseek(fp, content_length + 1, SEEK_CUR); // + 1 for hash

        // print perms, size and name
        printf("%06lo %5lu %s\n", mode, content_length, pathname);

        // set curr_byte to 1st byte of next blobbete
        curr_byte = fgetc(fp);
    }
    
    fclose(fp);
    return;
}


// extract the contents of blob_pathname

void extract_blob(char *blob_pathname) {
    FILE *fp = fopen(blob_pathname, "r"); 

    // exit with error if no such directory or file
    if (fp == NULL) {
        perror(blob_pathname);
        exit(1);
    }

    //
    uint8_t hash = 0;
    uint8_t *hash_p = &hash;


    // loop to extract each blobbete    
    for (long curr_byte = fgetc(fp); curr_byte != EOF; curr_byte = fgetc(fp)) {
        // update the hash
        hash = blobby_hash(0, curr_byte);

        // check magic number of current blobette
        if (curr_byte != BLOBETTE_MAGIC_NUMBER) {
            fprintf(stderr, "ERROR: Magic byte of blobette incorrect\n");
            exit(1);
        } 

        // extract mode 
        long mode = blobbete_mode(fp, curr_byte, hash_p);

        // find pathname and the length of contents
        char pathname[BLOBETTE_MAX_PATHNAME_LENGTH];
        unsigned long content_length = blobbete_name_content_len(fp, curr_byte, pathname, hash_p);
        
        // print process to terminal
        printf("Extracting: %s\n", pathname);

        // create new file with current blobbete's pathname
        FILE *extracted_file = fopen(pathname, "w");        
        for (unsigned long i = 0; i < content_length; i++) {
            fputc(fgetc_hash(fp, hash_p), extracted_file);
        }

        // set perms according to mode and
        // print error if failed
        if (chmod(pathname, mode) != 0) {
            perror(pathname);  
            exit(1);
        }        

        fclose(extracted_file);

        // checking the hash byte
        curr_byte = fgetc(fp);
        
        if (curr_byte != hash) {
            fprintf(stderr, "ERROR: blob hash incorrect\n");
            exit(1);            
        }        
    }

    fclose(fp);
    return;
}

// create blob_pathname from NULL-terminated array pathnames
// compress with xz if compress_blob non-zero (subset 4)

void create_blob(char *blob_pathname, char *pathnames[], int compress_blob) {
    int i = 0;

    // open new blob for reading and writing
    FILE *new_blob = fopen(blob_pathname, "w+");

    // open a file to insert as a blobbete
    FILE *curr_file = fopen(pathnames[i], "r");

    // loop through files and insert them into the blob
    while (pathnames[i] != NULL) {
        curr_file = fopen(pathnames[i], "r");              

        // exit with error if no such directory or file
        if (curr_file == NULL) {
            perror(pathnames[i]);
            exit(1);
        }

        // print current process to terminal
        printf("Adding: %s\n", pathnames[i]);

        // obtain metadata of file
        struct stat curr_stats;
        if (stat(pathnames[i], &curr_stats) != 0) {
            perror(pathnames[i]);
            exit(1);
        } 

        // insert magic number 
        fputc(BLOBETTE_MAGIC_NUMBER, new_blob);

        // deconstruct mode and place bytes
        long mode = curr_stats.st_mode;
        int shift = (BLOBETTE_MODE_LENGTH_BYTES - 1) * BITS_IN_BYTE;        
        while (shift >= 0) {
            fputc((mode >> shift) & LAST_8_BITS, new_blob);
            shift -= BITS_IN_BYTE;            
        }

        // deconstruct length of pathname and place bytes
        unsigned int pathname_length = strlen(pathnames[i]);
        shift = BITS_IN_BYTE;        
        while (shift >= 0) {
            fputc((pathname_length >> shift) & LAST_8_BITS, new_blob);
            shift -= BITS_IN_BYTE;            
        }

        // deconstruct content length and place bytes
        unsigned long content_length = curr_stats.st_size;
        shift = (BLOBETTE_CONTENT_LENGTH_BYTES - 1) * BITS_IN_BYTE;        
        while (shift >= 0) {
            fputc((content_length >> shift) & LAST_8_BITS, new_blob);
            shift -= BITS_IN_BYTE;            
        }

        // insert pathname in
        for (unsigned int ch = 0; ch < pathname_length; ch++) {        
            fputc(pathnames[i][ch], new_blob);
        }

        // insert contents
        for (unsigned long j = 0; j < content_length; j++) {
            fputc(fgetc(curr_file), new_blob);
        }

        // finding the hash        
        uint8_t hash = 0;
        
        // size of blobette (minus the hash byte)
        long blobbete_n_bytes_without_hash = BLOBETTE_MAGIC_NUMBER_BYTES + BLOBETTE_MODE_LENGTH_BYTES
                                        + BLOBETTE_PATHNAME_LENGTH_BYTES + BLOBETTE_CONTENT_LENGTH_BYTES
                                        + pathname_length + content_length;

        // seek back to start of current file
        fseek(new_blob, -blobbete_n_bytes_without_hash, SEEK_CUR);

        // loop thru file and update hash for each byte 
        for (unsigned long j = 0; j < blobbete_n_bytes_without_hash; j++) {
            hash = blobby_hash(hash, fgetc(new_blob));
        } 

        // insert hash
        fputc(hash, new_blob);                

        i++;
    }

    fclose(new_blob);
    fclose(curr_file);

}


// ADD YOUR FUNCTIONS HERE

// extract the bytes of mode, construct them together 
// then return as a long int (updates hash concurrently)

long blobbete_mode(FILE *fp, long curr_byte, uint8_t *hash_p) {
    long mode = 0;
    int shift = (BLOBETTE_MODE_LENGTH_BYTES - 1) * BITS_IN_BYTE;
    while (shift >= 0) {
        curr_byte = fgetc_hash(fp, hash_p);
        mode = mode | (curr_byte << shift);
        shift -= BITS_IN_BYTE;
    }
    
    return mode;
}

// given a character array of BLOBETTE_MAX_PATHNAME_LENGTH,
// finds the pathname and inserts it into the array.
// also returns content length of blobette (updates hash concurrently)

unsigned long blobbete_name_content_len(FILE *fp, long curr_byte,
                                        char pathname[BLOBETTE_MAX_PATHNAME_LENGTH],
                                        uint8_t *hash_p) {
    // find length of pathname by constructing 2 bytes
    unsigned int pathname_length = 0;
    curr_byte = fgetc_hash(fp, hash_p);
    pathname_length = pathname_length | (curr_byte << BITS_IN_BYTE);
    curr_byte = fgetc_hash(fp, hash_p);
    pathname_length = pathname_length | curr_byte;

    // find length of contents by constructing 6 bytes
    unsigned long content_length = 0;
    int shift = (BLOBETTE_CONTENT_LENGTH_BYTES - 1) * BITS_IN_BYTE; 
    while (shift >= 0) {
        curr_byte = fgetc_hash(fp, hash_p);
        content_length = content_length | (curr_byte << shift);
        shift -= BITS_IN_BYTE;
    }  

    // extract characters of pathname and insert them into the string
    unsigned int i = 0;
    while (i < pathname_length) {
        pathname[i] = fgetc_hash(fp, hash_p);
        i++;
    }
    pathname[i] = '\0';

    return content_length;
}

// equivalent function to fgetc but it also updates the hash
// using a pointer to the original hash variable

uint8_t fgetc_hash(FILE *fp, uint8_t *hash_p) {
    uint8_t byte = fgetc(fp);
    *hash_p = blobby_hash(*hash_p, byte);
    return byte;
}

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
