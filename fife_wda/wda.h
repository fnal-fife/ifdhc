#ifndef _WDA_H
#define _WDA_H   1

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef void *Dataset;
typedef void *Tuple;


/*
 * Low level generic function which returns the whole buffer received
 * The buffer returned by this function needs to be freed with free(void *ptr) function call
 */
void *getHTTP(const char *url, const char *headers[], size_t nheaders, size_t *length, int *status);

/*
 * Low level generic function which posts the whole buffer of given length
 */
void postHTTP(const char *url, const char *headers[], size_t nheaders, const char *data, size_t length, int *status);

void postHTTPsigned(const char *url, const char* password, const char *headers[], size_t nheaders, const char *data, size_t length, int *status);

/*
 * Low level generic functions which return the whole dataset
 */
Dataset getData(const char *url, const char *uagent, int *error);

Dataset getDataWithTimeout(const char *url, const char *uagent, int timeout, int *error);

/*
 * Functions to work with the tuples in the dataset
 */
int getNtuples(Dataset dataset);            /* Returns number of tuples in the dataset  */

int getIndex(Dataset dataset);				/* Returns current index value in dataset 	*/

Tuple getTuple(Dataset dataset, int i);     /* Returns NULL if out of range             */

Tuple getFirstTuple(Dataset dataset);       /* Returns the first tuple in the dataset   */

Tuple getNextTuple(Dataset dataset);        /* Returns the next tuple if available      */


/*
 * Functions to work with the fields in the tuple
 */
int getNfields(Tuple tuple);                                    /* Returns the number of fields in tuple                */

long getLongValue(Tuple tuple, int position, int *error);       /* Returns integer from specified position in a tuple   */

double getDoubleValue(Tuple tuple, int position, int *error);   /* Returns float from specified position in a tuple     */

/* 
 * Copies a string from specified position in a tuple, returns string length        
 */
int getStringValue(Tuple tuple, int position, char *buffer, int buffer_size, int *error);   

/* 
 * Copies a double array from specified position in a tuple, returns actual array length  
 */
int getDoubleArray(Tuple tuple, int position, double *buffer, int buffer_size, int *error); 

/* 
 * Copies an integer array from specified position in a tuple, returns actual array length
 */
int getLongArray(Tuple tuple, int position, long *buffer, int buffer_size, int *error);


/*
 * Housekeeping functions
 */
int releaseDataset(Dataset dataset);        /* Releases memory allocated for dataset    */

int releaseTuple(Tuple tuple);              /* Releases memory allocated for tuple      */

long getHTTPstatus(Dataset dataset);		/* Returns HTTP status code					*/

char *getHTTPmessage(Dataset dataset);		/* Returns HTTP status message				*/

#ifdef __cplusplus 
} 
#endif 

#endif
