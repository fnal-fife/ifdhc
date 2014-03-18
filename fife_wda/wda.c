#define _GNU_SOURCE 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <openssl/md5.h>
#include <unistd.h>
#include <sys/types.h>

#include <curl/curl.h>

#include "wda_version.h"
#include "wda.h"


#define DEBUG   0
/*
 * Internal data structures
 */

typedef struct {
    size_t ncolumns;    // Number of columns in CSV row
    size_t nelements;   // Number of elements in data array
    char **columns;     // Pointers to columns
    double data[2];     // The data array
} DataRec;


typedef struct {
    char *memory;       // The buffer from HTTP response
    size_t size;        // The size of the buffer (in bytes)
    char **rows;        // Array of rows in the buffer
    size_t nrows;       // Number of rows
    size_t idx;         // Current index
    long http_code;     // Status code
    DataRec *dataRecs[0];   // Array of parsed data records. Filled by getTuple calls
} HttpResponse;


static int destroyHttpResponse(HttpResponse *response);
static int initHttpResponse(HttpResponse *response);
static HttpResponse get_http_response(const char *url, const char *headers[], size_t nheaders, int timeout, int *status);


#define PRINT_ALLOC_ERROR(a)   fprintf(stderr, "Not enough memory (%s returned NULL)" \
            " at %s:%d\n", #a, __FILE__, __LINE__)

/*
 * Generates a random string of specified size
 */
static char *randStr(char *dst, int size)
{
   static const char text[] = "abcdefghijklmnopqrstuvwxyz"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "0123456789";
   int i;

   srandom(time(0));

   for (i = 0; i < size; ++i) {
      dst[i] = text[random() % (sizeof text - 1)];
   }
   dst[i] = '\0';
   return dst;
}


/*
 * Calculates MD5 signature for the buffer with a given password, salt string, and optional arguments string.
 */
static char *MD5Signature(const char *pwd, const char *salt, const char *args, const char *buf, size_t length)
{
    int n;
    MD5_CTX c;
    unsigned char digest[16];
    static char out[34];

    MD5_Init(&c);

    MD5_Update(&c, pwd, strlen(pwd));
    MD5_Update(&c, salt, strlen(salt));
    if (args)
        MD5_Update(&c, args, strlen(args));
    MD5_Update(&c, buf, length);

    MD5_Final(digest, &c);

    for (n = 0; n < 16; n++) {
        snprintf(&(out[n*2]), 16*2, "%02x", (unsigned int)digest[n]);
    }

    return out;
}



/*
 *  LibCurl API
 */
static size_t writeMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    HttpResponse *response = (HttpResponse *)userp;

    response->memory = (char *)realloc(response->memory, response->size + realsize + 8);
    if (response->memory == NULL) {
        /* out of memory! */
        PRINT_ALLOC_ERROR(realloc);
        return 0;
    }

    memcpy(&(response->memory[response->size]), contents, realsize);
    response->size += realsize;
    response->memory[response->size] = 0;

    return realsize;
}




/*
 * The function communicates with the server using CURL library
 * It returns pointer to the data as return value and passes data size via parameter
 */
void *getHTTP(const char *url, const char *headers[], size_t nheaders, size_t *length, int *status)
{
    HttpResponse response = get_http_response(url, headers, nheaders, 0, status);

    *length = response.size;                // Return data length
    return (void *)response.memory;         // Return pointer to the buffer
}


/*
 * Internal common function
 */
static HttpResponse get_http_response(const char *url, const char *headers[], size_t nheaders, int timeout, int *status)
{
    CURL *curl_handle;
    CURLcode ret = CURLE_FAILED_INIT;

    HttpResponse response;

    int i, k;
    time_t t0 = time(NULL);
    time_t t1 = t0;

    initHttpResponse(&response);

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    if (curl_handle) {
        struct curl_slist *headerlist = NULL;
        char user_agent[256];
        snprintf(user_agent, 256, "User-Agent: wdaAPI/%s (UID=%d, PID=%d)", WDA_VERSION, getuid(), getpid());
        if (headers) {
            for (i = 0; i < nheaders; i++) {
                if (headers[i])
                    headerlist = curl_slist_append(headerlist, headers[i]);
            }
        }
        headerlist = curl_slist_append(headerlist, user_agent);

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);

        /* send all data to this function  */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeMemoryCallback);

        /* we pass our 'response' struct to the callback function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&response);

        /* Enable redirection */
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);

        ret = curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headerlist);    /* Set extra headers            */

        k = 0;
        do {
            /* get it! */
            destroyHttpResponse(&response);
            initHttpResponse(&response);
            ret = curl_easy_perform(curl_handle);
            *status = ret;
            if (ret != CURLE_OK) {                                              /* Check for errors             */
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(ret));
                response.size = 0;
                response.http_code = 0;
            } else {
                    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response.http_code);
                if (response.http_code == 200 && ret != CURLE_ABORTED_BY_CALLBACK) {
                    //Succeeded
                    break;
                } else {
                    //Failed
# if DEBUG
                    fprintf(stderr, "HTTP status code=%d: '%s'\n", response.http_code, response.memory);
# endif
                }
            }
            int d = 1 + ((double)random()/(double)RAND_MAX) * (1 << k++);
            sleep(d);
            t1 = time(NULL);
# if DEBUG
            fprintf(stderr, "ret=%d, k=%d, delay=%d, t0=%ld, t1=%ld to=%d\n", ret, k, d, t0, t1, timeout);
# endif
        } while ((t1 - t0) < timeout);
        /* cleanup curl stuff */
        curl_easy_cleanup(curl_handle);
        curl_slist_free_all(headerlist);                                        /* Free the custom headers      */
# if DEBUG
        fprintf(stderr, "get_http_response: %lu bytes retrieved\n", (long)response.size);
# endif
    }
    /* we're done with libcurl, so clean it up */
    curl_global_cleanup();

    *status = ret;                          // Return status
    return response;                        // Return response structure
}


/*
 *
 */
void postHTTPsigned(const char *url, const char* password, const char *headers[], size_t nheaders, const char *data, size_t length, int *status)
{
    char salt[102];
    const char **hdrs = NULL;
    char *signature;
    char *args;
    char *mptr;
    int i;
    int ret;

    args = strchr(url, '?');                                        // Get the pointer to optional arguments
    if (args) args++;                                               // If get the arguments skip '?' character

    randStr(salt, sizeof (salt) - 2);                               // Generate salt
    signature = MD5Signature(password, salt, args, data, length);   // Generate MD5 signature

    hdrs = malloc(nheaders + 2);
    for (i = 0; i < nheaders; i++) {                                // Copy additional headers if provided
        hdrs[i] = headers[i];
    }

    mptr = malloc(128);                                             // Prepare X-Salt header
    ret = snprintf(mptr, 128, "X-Salt: %s", salt);
    if (ret >= 128) {                                               // Memory error
        fprintf(stderr, "Not enough space for the header\n");
    }
    hdrs[i++] = mptr;                                               // Add to headers list

    mptr = malloc(128);
    ret = snprintf(mptr, 128, "X-Signature: %s", signature);        // Prepare X-Signature header
    if (ret >= 128) {                                               // Memory error
        fprintf(stderr, "Not enough space for the header\n");
    }
    hdrs[i++] = mptr;                                               // Add to headers list

    postHTTP(url, hdrs, nheaders+2, data, length, status);          // Post the data with additional headers

    free(hdrs[--i]);                                                // Free allocated memory
    free(hdrs[--i]);                                                // Free allocated memory
    free(hdrs);                                                     // Free allocated memory
}


/*
 * The function communicates with the server using CURL library
 * It posts the databuffer of given size.
 *
 */
//  struct curl_slist *headerlist = NULL;
//
//  headerlist = curl_slist_append(headerlist, "X-Salt: .....");
//  headerlist = curl_slist_append(headerlist, "X-Signature: .....");
//
//  res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
//  ...... 
//  res = curl_easy_perform(curl);
//  ......
//  curl_easy_cleanup(curl);
//  ......
//  curl_slist_free_all(headerlist);
//
void postHTTP(const char *url, const char *headers[], size_t nheaders, const char *data, size_t length, int *status)
{
    CURL *curl;
    CURLcode ret;
    int i;

    curl = curl_easy_init();

    if (curl) {
        struct curl_slist *headerlist = NULL;
        char user_agent[256];
        snprintf(user_agent, 256, "User-Agent: wdaAPI/%s (UID=%d, PID=%d)", WDA_VERSION, getuid(), getpid());
        if (headers) {
            for (i = 0; i < nheaders; i++) {
                if (headers[i])
                    headerlist = curl_slist_append(headerlist, headers[i]);
            }
        }
        headerlist = curl_slist_append(headerlist, user_agent);

        curl_easy_setopt(curl, CURLOPT_URL, url);                               /* Set the target URL           */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);                       /* Pass the pointer to the data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)length);            /* Pass the data length         */
        if (headerlist) {
            ret = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);       /* Set extra headers            */
        }
        ret = curl_easy_perform(curl);                                          /* Do actual POST               */

        if (ret != CURLE_OK) {                                                  /* Check for errors             */
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(ret));
        }
        curl_easy_cleanup(curl);                                                /* Cleanup                      */
        
        curl_slist_free_all(headerlist);                                        /* Free the custom headers      */
    }
    curl_global_cleanup();                                                      /* Cleanup                      */
    *status = ret;
}


/*
 * The function communicates with the server using CURL library
 * It returns the structure which contains the array of rows as strings
 * along with its size.
 */
static HttpResponse get_data_rows(const char *url, const char *headers[], size_t nheaders, int timeout, int *status)
{
    int i, k;
    char *row;
    char *running;

    HttpResponse response = get_http_response(url, headers, nheaders, timeout, status);

# if DEBUG
    fprintf(stderr, "get_data_rows: %lu bytes retrieved\n", (long)response.size);
# endif
    if (response.http_code != 200) {
        return response;
    }
    /* Calculate the number of rows */
    for (i = 0, k = 0; i < response.size; i++) {
      if (response.memory[i]=='\n')
          k++;
    }
# if DEBUG
    fprintf(stderr, "get_data_rows: %d lines retrieved\n", k);
# endif
    /* Allocate memory for array of rows */
    response.rows = (char **)malloc(sizeof(char *) * k + 8);
    if (response.rows == NULL) {
        /* out of memory! */
        PRINT_ALLOC_ERROR(malloc);
        response.size = 0;
        return response;
    }
    response.nrows = k;

    /* Now break response to the rows */
    running = response.memory;                                  // Start from the beginning of the response buffer
    for (k = 0; row = strsep(&running, "\n"); k++) {            // Walk through, find all newlines, replace them with '\0'
        //fprintf(stderr, "t = '%s'\n", row);
        //fprintf(stderr, "running = '%lx'\n", running);
        response.rows[k] = row;                                 // Store pointer to the line
    }

    return response;
}



/*
 * The function deallocates used memory buffers.
 */
static int destroyHttpResponse(HttpResponse *response)
{
    if (response!=NULL) {
        if (response->rows!=NULL) {
            free(response->rows);
            response->rows = NULL;
        }
        if (response->memory!=NULL) {
            free(response->memory);
            response->memory = NULL;
        }
    }
    return 0;
}



/*
 * The function initializes response structure.
 */
static int initHttpResponse(HttpResponse *response)
{
    response->memory = (char *)malloc(1);   /* will be grown as needed by the realloc above */
    response->memory[0] = '\0';             /* Zero byte                                    */
    response->rows = NULL;                  /* no data at this point                        */
    response->nrows = response->size = 0;   /* no data at this point                        */
    response->http_code = 0;
}



/*
 * The function deallocates used memory buffers.
 */
static int destroyDataRec(DataRec *dataRec) 
{
    if (dataRec!=NULL) {
        if (dataRec->columns!=NULL) {
            if (dataRec->columns[0]!=NULL) {
                free(dataRec->columns[0]);
            }
            free(dataRec->columns);
            dataRec->columns = NULL;
        }
        free(dataRec);
    }
    return 0;
}

/*
 * Function to parse one row in CSV format.
 */
static DataRec *parse_csv(const char *s)
{
    char *cp;
    char *sp;
    char *qp;
    int len;
    int i, j, ncol, inquotes;
    register char *ss;

    ss = strdup(s);

    DataRec *dataRec = (DataRec *)malloc(sizeof (DataRec));
    dataRec = (DataRec *)memset(dataRec, 0, sizeof (DataRec));

    {
        //
        // Find the number of columns
        //
        for (sp = ss, ncol = 0, inquotes = 0; *sp; sp++) {
            if (*sp=='"') inquotes = !inquotes;
            if (inquotes) continue;
            if (*sp==',') ncol++;
        }
        //fprintf(stderr, "parse_csv: n=%d\n", ncol);
        dataRec->ncolumns = ++ncol;                                 // Store the number of columns
        
        size_t csize = sizeof(char *) * dataRec->ncolumns;          // Allocated memory size
        dataRec->columns = (char **)malloc(csize);                  // Allocate pointers to column data
        if (dataRec->columns == NULL) {
            /* out of memory! */
            PRINT_ALLOC_ERROR(malloc);
            return 0;
        }
        memset(dataRec->columns, 0, csize);                         // Clear the column pointers

        sp = ss;                                                    // Start from the begining of the line
        //fprintf(stderr, "parse_csv: s='%s'\n", ss);
        for (cp = sp = ss, ncol = 0, inquotes = 0; ncol < dataRec->ncolumns; sp++) {
            if (*sp=='"') inquotes = !inquotes;
            if (inquotes) continue;
            if (*sp==',' || *sp=='\0') {
                dataRec->columns[ncol++] = cp;                      // Store the pointer to the name
                *sp = '\0';
                cp = sp + 1;
            }
        }
    } 
    return dataRec;
}


/*
 * Low level generic function which returns the whole dataset
 */
Dataset getDataWithTimeout(const char *url, const char *uagent, int timeout, int *error)
{
    int err;
    HttpResponse *response;
    char user_agent[256];
    
    snprintf(user_agent, 256, "User-Agent: %s", uagent);
    const char *headers[] = {user_agent, NULL};
# if DEBUG
    fprintf(stderr, "getData: url='%s'\n", url);
# endif
    *error = errno = 0;
    response = (HttpResponse *)malloc(sizeof (HttpResponse));
    if (response == NULL) {
        /* out of memory! */
        PRINT_ALLOC_ERROR(malloc);
        *error = errno;
        return NULL;
    }
    *response = get_data_rows(url, headers, 1, timeout, &err);
    if (err) {
        *error = errno = ENODATA;
    }
    /* Now grow the response structure to allocate space for dataRecs array */
    response = (HttpResponse *)realloc(response, sizeof (HttpResponse) + response->nrows*sizeof(DataRec));
    if (response == NULL) {
        /* out of memory! */
        PRINT_ALLOC_ERROR(malloc);
        *error = errno;
        return NULL;
    }
    memset(response->dataRecs, 0, response->nrows*sizeof(DataRec));

    return (Dataset)response;
}

/*
 * Low level generic function which returns the whole dataset. Uses default timeout for request
 */
Dataset getData(const char *url, const char *uagent, int *error)
{
    return getDataWithTimeout(url, uagent, 0, error);
}

int getNtuples(Dataset dataset)
{
    HttpResponse *response;

    response = (HttpResponse *)dataset;
    return response->nrows;
}


int getIndex(Dataset dataset)
{
    HttpResponse *response;

    response = (HttpResponse *)dataset;
    return response->idx;
}


Tuple getTuple(Dataset dataset, int idx)
{
    HttpResponse *response;
    DataRec *dataRec;

    response = (HttpResponse *)dataset;
    if (idx < 0 || idx >= response->nrows) {
        errno = ENODATA;
        return NULL;
    }
    if (response->dataRecs[idx]) {
//    fprintf(stderr, "Get from cache\n");
        dataRec = response->dataRecs[idx];
    } else {
//    fprintf(stderr, "Store in cache\n");
        dataRec = parse_csv(response->rows[idx]);          // parse the line
        response->dataRecs[idx] = dataRec;
    }
    (response->idx) = idx + 1;
    return (Tuple)(dataRec);
}


int getNfields(Tuple tuple)
{
    DataRec *dataRec = (DataRec *)tuple;
    return dataRec->ncolumns;
}


Tuple getFirstTuple(Dataset dataset)
{
    return getTuple(dataset, 0);
}


Tuple getNextTuple(Dataset dataset)
{
    HttpResponse *response;

    response = (HttpResponse *)dataset;
    
    return getTuple(dataset, response->idx);
}


long getLongValue(Tuple tuple, int position, int *error)
{
    DataRec *dataRec = (DataRec *)tuple;
    if (position < 0 || position >= dataRec->ncolumns) {
        *error = EINVAL;
        return LONG_MIN;
    }
    errno = 0;
    long val = strtol(dataRec->columns[position], NULL, 10);
//    fprintf(stderr,"getIntValue: converting '%s'\n", dataRec->columns[position]);
//    fprintf(stderr,"getIntValue: returning %ld, errno=%d\n", val, errno);
    *error = errno;
    return val;
}

double getDoubleValue(Tuple tuple, int position, int *error)
{
    DataRec *dataRec = (DataRec *)tuple;
    if (position < 0 || position >= dataRec->ncolumns) {
        *error = EINVAL;
        return LONG_MIN;
    }
    errno = 0;
    double val = strtod(dataRec->columns[position], NULL);
    *error = errno;
    return val;
}


int getStringValue(Tuple tuple, int position, char *buffer, int buffer_size, int *error)
{
    DataRec *dataRec = (DataRec *)tuple;
    if (position < 0 || position >= dataRec->ncolumns) {
        *error = EINVAL;
        return -1;
    }
////strncpy(buffer, dataRec->columns[position], buffer_size);
    if (dataRec->columns[position] > 0) {
        memccpy(buffer, dataRec->columns[position], 0, buffer_size);
        *error = 0;
        return strlen(buffer);
    }
    *error = EINVAL;
    return -1;
}

int getDoubleArray(Tuple tuple, int position, double *buffer, int buffer_size, int *error)
{
    DataRec *dataRec = (DataRec *)tuple;
    if (position < 0 || position >= dataRec->ncolumns) {
        *error = EINVAL;
        return 0;
    }
    int i, len;
    double val;
    char *sptr;
    char *eptr;

    errno = 0;
    sptr = dataRec->columns[position];              // Start from the beginning of array
    if (strncmp(sptr, "\"[", 2)==0)
        sptr += 2;                                  // Skip double quote and square bracket
    for (len = i = 0; i < buffer_size; i++) {
        val = strtod(sptr, &eptr);                  // Try to convert
# if DEBUG
        fprintf(stderr, "s='%s' ", sptr);
        fprintf(stderr, "[%d]=%f\n", i, val);
# endif
        if (sptr==eptr) break;                      // End the loop if no coversion was performed
        if (*sptr=='\0') break;                     // End the loop if buffer ends
        buffer[len++] = val;                        // Store converted value, increase the length
        sptr = eptr + 1;                            // Shift the pointer to the next number
    }
    *error = errno;
    return len;
}

int getIntArray(Tuple tuple, int position, long *buffer, int buffer_size, int *error)
{
    DataRec *dataRec = (DataRec *)tuple;
    if (position < 0 || position >= dataRec->ncolumns) {
        *error = EINVAL;
        return 0;
    }
    int i, len;
    long val;
    char *sptr;
    char *eptr;

    errno = 0;
    sptr = dataRec->columns[position];              // Start from the beginning of array
    if (strncmp(sptr, "\"[", 2)==0)
        sptr = dataRec->columns[position] + 2;      // Skip double quote and square bracket
    for (len = i = 0; i < buffer_size; i++) {
        val = strtol(sptr, &eptr, 10);              // Try to convert
        if (sptr==eptr) break;                      // End the loop if no coversion was performed
        if (*sptr=='\0') break;                     // End the loop if buffer ends
        buffer[len++] = val;                        // Store converted value, increase the length
        sptr = eptr + 1;                            // Shift the pointer to the next number
    }
    *error = errno;
    return len;
}


int releaseDataset(Dataset dataset)
{
    int i;
    HttpResponse *response = (HttpResponse *)dataset;
    /* First, release all stored dataRecs   */
    for (i = 0; i < response->nrows; i++) {
        if (response->dataRecs[i] > 0) {
# if DEBUG
            fprintf(stderr, "releaseDataset: destroyDataRec %d\n", i);
# endif
            destroyDataRec(response->dataRecs[i]);
            response->dataRecs[i] = NULL;
        }
    }
    /* Second, release dataset itself       */
    return destroyHttpResponse((HttpResponse *)dataset);
}


int releaseTuple(Tuple tuple)
{
# if 0
    return destroyDataRec((DataRec *)tuple);
# else
    return 0;
# endif
}

long getHTTPstatus(Dataset dataset)
{
    return ((HttpResponse *)dataset)->http_code;
}

char *getHTTPmessage(Dataset dataset)
{
    return ((HttpResponse *)dataset)->memory;
}

