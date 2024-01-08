/**
 * ================================================
 * = Harvard University | CS265 | Systems Project =
 * ================================================
 * ========== LSM TREE WORKLOAD GENERATOR =========
 * ================================================
 * Contact:
 * ========
 * - Kostas Zoumpatianos <kostas@seas.harvard.edu>
 * - Michael Kester <kester@eecs.harvard.edu>
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <float.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#include "data_types.h"
#include "logo.h"

/////////////////////////////////////////////////////////////////////////////
///////////////             D E F I N I T I O N S             ///////////////
/////////////////////////////////////////////////////////////////////////////
// This is used for storing gets existing in the workload so far
// They are then used for skewed query workload generation
#define MAX_OLD_GETS_POOL_SIZE 10000000 // ~= 80MB

// This is used for storing puts existing issued so far
// They are then used for non empty get query generation
#define MAX_OLD_PUTS_POOL_SIZE 10000000 // ~= 80MB

// Constants for the different operations generated
#define OPERATION_PUT 0
#define OPERATION_GET 1
#define OPERATION_RANGE 2
#define OPERATION_DELETE 3

// This is the product name
#define PRODUCT "\
-------------------------------------------------------------------------\n\
                    W O R K L O A D   G E N E R A T O R                  \n\
-------------------------------------------------------------------------\n\
"
/////////////////////////////////////////////////////////////////////////////


// ====================================================================== //
//                     COMMAND LINE ARGUMENTS PARSING
// ====================================================================== //
/**
 * The settings parsed from command line arguments
 */
struct settings {
    int puts;
    int gets;
    float gets_skewness;
    float gets_misses_ratio;
    int ranges;
    int max_range_size;
    char uniform_ranges; /* true (uniform) or false (gaussian) */
    int deletes;
    int external_puts;
    int seed;
    int min_lookup_batch_size;
    int max_lookup_batch_size;
    int min_update_batch_size;
    int max_update_batch_size;
};

/**
 * Default values for settings 
 */
void initialize_settings(struct settings *s) {
    s->puts                  = 0;
    s->gets                  = 0;
    s->gets_skewness         = 0;
    s->gets_misses_ratio     = 0.5;
    s->ranges                = 0;
    s->max_range_size        = 0;
    s->deletes               = 0;
    s->external_puts         = 0;
    s->seed                  = 13141;
    s->uniform_ranges        = 1;
}

/**
 * Prints the usage of the binary. 
 * i.e., all valid arguments.
 */
void usage(char * binary) {
    #ifdef LOGO_IN_USAGE
    fprintf(stderr, DASLAB_LOGO);
    #endif
    fprintf(stderr, LOGO);
    fprintf(stderr, PRODUCT);
    fprintf(stderr, "Usage: %s\n\
        --puts [number of put operations]\n\
        --gets [number of get operations]\n\
        --gets-skewness [skewness (0-1) of get operations]\n\
        --gets-misses-ratio [empty result queries ratio]\n\
        --ranges [number of range operations]\n\
        --max-range-size [maximum possible size of ranges]\n\
        --uniform-ranges *use uniform ranges (default)*\n\
        --gaussian-ranges *use gaussian ranges*\n\
        --deletes [number of delete operations]\n\
        --external-puts *store puts in external binary files*\n\
        --seed [random number generator seed]\n\
        --help\n\
\n\n", binary);
}

int next_operation(double puts_percent, double gets_percent, double ranges_percent, double deletes_percent) {
    double r = (double)rand() / (double)RAND_MAX;
    if (r < puts_percent) {
        return OPERATION_PUT;
    } else if (r < puts_percent + gets_percent) {
        return OPERATION_GET; 
    } else if (r < puts_percent + gets_percent + ranges_percent) {
        return OPERATION_RANGE; 
    } else {
        return OPERATION_DELETE; 
    }
}

/**
 * Parses the arguments to a settings struct
 */
void parse_settings(int argc, char **argv, struct settings *s) {
    int c;
    while (1)
    {
        static struct option long_options[] =
        {
            {"puts",             required_argument,  0, 'p'},
            {"gets",             required_argument,  0, 'g'},
            {"gets-skewness",    required_argument,  0, 'G'},
            {"gets-misses-ratio",required_argument,  0, 't'},
            {"ranges",           required_argument,  0, 'r'},
            {"max-range-size",   required_argument,  0, 'b'},
            {"uniform-ranges",   no_argument,        0, 'u'},
            {"gaussian-ranges",  no_argument,        0, 'n'},
            {"deletes",          required_argument,  0, 'd'},
            {"seed",             required_argument,  0, 's'},
            {"external-puts",    no_argument,        0, 'e'},
            {"help",             no_argument,        0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;
        c = getopt_long (argc, argv, "p:b:g:G:r:t:d:i:eh",
                         long_options, &option_index);
        /* Detect the end of the options. */
        if (c == -1)
             break;
        switch (c)
        {
            case 't':
                s->gets_misses_ratio = atof(optarg);
                break;
            case 'p':
                s->puts = atoi(optarg);
                break;
            case 'g':
                s->gets = atoi(optarg);
                break;
            case 'r':
                s->ranges = atoi(optarg);
                break;
            case 'b':
                s->max_range_size = atoi(optarg);
                break;
            case 'd':
                s->deletes = atoi(optarg);
                break;
            case 'G':
                s->gets_skewness = atof(optarg);
                break;
            case 'e':
                s->external_puts = 1;
                break;
            case 's':
                s->seed = atoi(optarg);
                break;
            case 'u':
                s->uniform_ranges = 1;
                break;
            case 'n':
                s->uniform_ranges = 0;
                break;
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case '?':
                if (optopt == 'e' || optopt == 'd' || optopt == 'q')
                    fprintf (stderr, "Option -%c requires an argument.\n",
                            optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                exit(EXIT_FAILURE);
            default:
                abort();
        }
    }

    // Check: 0 puts not allowed
    if(s->puts == 0) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Make sure ranges are sane
    if(s->min_update_batch_size > s->max_update_batch_size) {
        s->max_update_batch_size = s->min_update_batch_size;
    }
    
    if(s->min_lookup_batch_size > s->max_lookup_batch_size) {
        s->max_lookup_batch_size = s->min_lookup_batch_size;
    }
}

/**
 * Prints the settings on stderr
 */
void print_settings(struct settings *s) {
    /////////////////////////////////////
    //           PRINT INFO            //
    /////////////////////////////////////
    fprintf(stderr, "+---------- CS 265 ----------+\n");
    fprintf(stderr, "|        WORKLOAD INFO       |\n");
    fprintf(stderr, "+----------------------------+\n");
    // Seed
    fprintf(stderr, "| initial-seed: %d\n", s->seed);
    // Puts
    fprintf(stderr, "| puts-total: %d\n", s->puts);
    // Gets
    fprintf(stderr, "| gets-total: %d\n", s->gets);
    fprintf(stderr, "| get-skewness: %.4lf\n", s->gets_skewness);
    // Ranges
    fprintf(stderr, "| ranges: %d\n", s->ranges);
    fprintf(stderr, "| range distribution: %s\n", 
                       s->uniform_ranges ? "uniform" : "gaussian");
    // Deletes
    fprintf(stderr, "| deletes: %d\n", s->deletes);
    // Gets misses ratio
    fprintf(stderr, "| gets-misses-ratio: %.4lf\n", s->gets_misses_ratio);
    fprintf(stderr, "+----------------------------+\n");
}

// ====================================================================== //
//                         MAIN WORKLOAD GENERATOR
// ====================================================================== //
/**
 * Generates a workload based on some settings
 */
void generate_workload(struct settings *s) {
    int i, j;
    
    /////////////////////////////////////
    ///    INITIALIZE NUMBER POOLS    ///
    /////////////////////////////////////
    // Initialize random generator
    //gsl_rng_default_seed = s->seed; // Remove this line
    const gsl_rng_type *T;
    gsl_rng *r;
    gsl_rng_env_setup();
    T = gsl_rng_default;
    r = gsl_rng_alloc(T);
    gsl_rng_set(r, s->seed); // Add this line to set the seed for the random generator

    // Buffer of previous puts (for non empty gets)
    int old_puts_pool_max_size;
    int old_puts_pool_count = 0;
    if(s->puts < MAX_OLD_PUTS_POOL_SIZE) {
        old_puts_pool_max_size = s->puts * sizeof(KEY_t);    
    }
    else {
        old_puts_pool_max_size = MAX_OLD_PUTS_POOL_SIZE * sizeof(KEY_t);      
    }
    KEY_t *old_puts_pool = malloc(sizeof(KEY_t) * old_puts_pool_max_size);
    for(i=0; i<old_puts_pool_max_size; i++) {
        old_puts_pool[i] = 0;
    }

    // Buffer of previous gets (for skewed workloads)
    int old_gets_pool_max_size;
    int old_gets_pool_count = 0;
    if(s->gets < MAX_OLD_GETS_POOL_SIZE) {
        old_gets_pool_max_size = s->gets * sizeof(KEY_t);    
    }
    else {
        old_gets_pool_max_size = MAX_OLD_GETS_POOL_SIZE * sizeof(KEY_t);      
    }
    KEY_t *old_gets_pool = malloc(sizeof(KEY_t) * old_gets_pool_max_size);
    for(i=0; i<old_gets_pool_max_size; i++) {
        old_gets_pool[i] = 0;
    }

    /////////////////////////////////////
    //             PROCESS             //
    /////////////////////////////////////
    int current_deletes = 0;
    int current_puts = 0;
    int current_gets = 0;
    int current_ranges = 0;
    int prev_operation = -1;
    int current_file = 0;
    FILE *pf = NULL;

    while(current_deletes < s->deletes ||
          current_puts < s->puts ||
          current_gets < s->gets || 
          current_ranges < s->ranges) 
    {
        int remaining_puts    = s->puts    - current_puts;
        int remaining_gets    = s->gets    - current_gets;
        int remaining_ranges  = s->ranges  - current_ranges;
        int remaining_deletes = s->deletes - current_deletes;

        int remaining_total = remaining_puts + remaining_gets + remaining_ranges + remaining_deletes;

        double puts_percent    = (s->puts    == 0) ? 0 : (double)remaining_puts   / remaining_total;
        double gets_percent    = (s->gets    == 0) ? 0 : (double)remaining_gets   / remaining_total;
        double ranges_percent  = (s->ranges  == 0) ? 0 : (double)remaining_ranges / remaining_total;
        double deletes_percent = (s->deletes == 0) ? 0 : (double)remaining_deletes / remaining_total;

        // Decide a random operation
        int operation = next_operation(puts_percent, gets_percent, ranges_percent, deletes_percent);

        // TODO: Check if this operation has remaining else continue.
        switch(operation) {
            case OPERATION_PUT:
                if(current_puts >= s->puts) 
                    continue;
                break;
            case OPERATION_GET:
                if(current_gets >= s->gets) 
                    continue;
                break;
            case OPERATION_RANGE:
                if(current_ranges >= s->ranges) 
                    continue;
                break;
            case OPERATION_DELETE:
                if(current_deletes >= s->deletes) 
                    continue;
                break;
            default:
                break;
        }

        // Check if I have to open a new binary file for puts
        if(prev_operation != OPERATION_PUT && 
           operation == OPERATION_PUT &&
           s->external_puts) {
            if(pf != NULL) {
                fclose(pf);
                pf = NULL;
            }
            char pfname[256];
            sprintf(pfname, "%d.dat", current_file++); 
            pf = fopen(pfname, "wb");
            printf("l \"%s\"\n", pfname);
        }
        prev_operation = operation;

        // Perform operation
        if(operation == OPERATION_PUT) {
            ////////////////// PUTS //////////////////
            // Pick key and value
            KEY_t k = GEN_RANDOM_KEY_UNIFORM(r);
            VAL_t v = GEN_RANDOM_VAL_UNIFORM(r);
            
            // Write
            if(s->external_puts) {
                fwrite(&k, sizeof(KEY_t), 1, pf);
                fwrite(&v, sizeof(VAL_t), 1, pf);
            }
            else {
                printf(PUT_PATTERN, k, v);
            } 

            // Store this number in the pool for 
            // choosing it for future get queries
            if(old_puts_pool_count >= old_puts_pool_max_size) {
                old_puts_pool[rand() % old_puts_pool_count] = k;
            }
            else {
                old_puts_pool[old_puts_pool_count++] = k;
            }

            current_puts++;
        }
        else if(operation == OPERATION_GET) {
            // No puts yet
            if(!current_puts)
                continue;

            ////////////////// GETS //////////////////
            KEY_t k;
            // With a probability use a new key as a query
            if((((float)rand()) / RAND_MAX > s->gets_skewness) || old_gets_pool_count == 0) {
                // With a probability use one of the 
                // previously inserted data as a query
                if(((float)rand()) / RAND_MAX > s->gets_misses_ratio) {
                    k = old_puts_pool[rand() % old_puts_pool_count];
                }
                // Or issue a completely random query 
                // most probably causing a miss
                else {
                    k = GEN_RANDOM_KEY_UNIFORM(r);
                }
                // Store this number in the pool for 
                // choosing it for future skewed queries
                if(old_gets_pool_count >= old_gets_pool_max_size) {
                    old_gets_pool[rand() % old_gets_pool_count] = k;
                }
                else {
                    old_gets_pool[old_gets_pool_count++] = k;
                }
            }
            // Or use one of the previously queried keys
            else {
                k = old_gets_pool[rand() % old_gets_pool_count];
            }

            // Write in output
            printf(GET_PATTERN, k);
            
            current_gets++;
        }
        else if(operation == OPERATION_RANGE) {
            // No puts yet
            if(!current_puts)
                continue;

            //////////////// RANGES ////////////////// 
            KEY_t a;
            KEY_t b;
            KEY_t range_size;

            if(s->uniform_ranges) {
                if (s->max_range_size > 0) {
                    a = GEN_RANDOM_KEY_UNIFORM(r);
                    // Pick a random number between +/- s->max_range_size. +1 to avoid zero range size
                    range_size = gsl_rng_uniform_int(r, 2*s->max_range_size + 1) - s->max_range_size;
                    b = a + range_size;
                    if (b > KEY_MAX) b = KEY_MAX;
                    if (b < KEY_MIN) b = KEY_MIN;
                }
                else {
                    a = GEN_RANDOM_KEY_UNIFORM(r);
                    b = GEN_RANDOM_KEY_UNIFORM(r);
                }
            }
            else {
                if (s->max_range_size > 0) {
                    a = GEN_RANDOM_KEY_GAUSS(r);
                    // Pick a random number between +/- s->max_range_size. +1 to avoid zero range size
                    range_size = gsl_rng_uniform_int(r, 2*s->max_range_size + 1) - s->max_range_size;
                    b = a + range_size;
                    if (b > KEY_MAX) b = KEY_MAX;
                    if (b < KEY_MIN) b = KEY_MIN;
                }
                else {
                    a = GEN_RANDOM_KEY_GAUSS(r);
                    b = GEN_RANDOM_KEY_GAUSS(r);
                }
            }

            if(a < b) {
                printf(RANGE_PATTERN, a, b);
            }
            else {
                printf(RANGE_PATTERN, b, a);
            }
            
            current_ranges++;
        }
        else if(operation == OPERATION_DELETE) {
            // No puts yet
            if(!current_puts)
                continue;

            ////////////////// DELETES //////////////////
            KEY_t d = old_puts_pool[rand() % old_puts_pool_count];
            printf(DELETE_PATTERN, d);
            
            current_deletes++;
        }
    }

    // Free memory
    free(old_gets_pool);
    free(old_puts_pool);
    gsl_rng_free(r);

    // Close file
    if(pf != NULL) {
        fclose(pf);
        pf = NULL;
    }
}

/**
 * The main entry point to the software.
 */
int main(int argc, char **argv) {
    /////////////////////////////////////
    //          LOAD SETTINGS          //
    /////////////////////////////////////
    struct settings s;
    initialize_settings(&s);
    parse_settings(argc, argv, &s);
    print_settings(&s);
    
    /////////////////////////////////////
    //        GENERATE WORKLOAD        //
    /////////////////////////////////////
    generate_workload(&s);

    // Done!
    exit(EXIT_SUCCESS);
}
