#define _GNU_SOURCE 

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <curl/curl.h>

#include "ifbeam_c.h"

#define IFB_RETRY_TIMEOUT 1200
#define	MAX_VECTOR_SIZE	256

#define MAX_UAGENT_SIZE 128
static char g_uagent[MAX_UAGENT_SIZE] = {'\0'};
/*
 * Returns dataset for the bundle from IFBeam DB for the specified time
 */
Dataset getBundleForTime(const char *url, const char *bundle, const double t, int *error) {

    char sbuf[512];

    snprintf(sbuf, sizeof (sbuf)-2, "%s/data?b=%s&t=%.3f&f=csv", url, bundle, t);

    return getDataWithTimeout(sbuf, g_uagent, IFB_RETRY_TIMEOUT, error);
}

/*
 * Returns dataset for the bundle from IFBeam DB for the specified time range
 */
Dataset getBundleForInterval(const char *url, const char *bundle, const double t0, const double t1, int *error) {

    char sbuf[512];

    snprintf(sbuf, sizeof (sbuf)-2, "%s/data?b=%s&t0=%.3f&t1=%.3f&f=csv", url, bundle, t0, t1);

    return getDataWithTimeout(sbuf, g_uagent, IFB_RETRY_TIMEOUT, error);
}


/*
 * Returns dataset for the variable in event from IFBeam DB for the specified time
 */
Dataset getEventVarForTime(const char *url, const char *event, const char *var, const double t, int *error) {

    char sbuf[512];

    snprintf(sbuf, sizeof (sbuf)-2, "%s/data?e=%s&v=%s&t=%.3f&f=csv", url, event, var, t);

    return getDataWithTimeout(sbuf, g_uagent, IFB_RETRY_TIMEOUT, error);
}


/*
 * Returns dataset for the variable in event from IFBeam DB for the specified time range
 */
Dataset getEventVarForInterval(const char *url, const char *event, const char *var, const double t0, const double t1, int *error) {

    char sbuf[512];

    snprintf(sbuf, sizeof (sbuf)-2, "%s/data?e=%s&v=%s&t0=%.3f&t1=%.3f&f=csv", url, event, var, t0, t1);

    return getDataWithTimeout(sbuf, g_uagent, IFB_RETRY_TIMEOUT, error);
}

/*
 * Functions to work with the tuples in the dataset
 */
 
/* 
 * Returns number of tuples in the dataset
 */
int getNmeasurements(Dataset dataset) 
{
	return getNtuples(dataset)-1;
}

/* 
 * Returns NULL if out of range
 */
Measurement getMeasurement(Dataset dataset, int i) 
{
	int error;							// Error code
	int len;							// Vector length
	double b[MAX_VECTOR_SIZE];			// Temporary buffer
	Measurement m = NULL;				// Measurement handler

	errno = ENODATA;
	Tuple t = getTuple(dataset, i);		// Try to get tuple
	if (t) {															// Tuple found
	    m = (Measurement)realloc(m, sizeof(*m));						// Create a measurement object
	    if (m) {														// Success
			m->vector_size = -1;										// Default value
			m->clock = getLongValue(t, 0, &error);						// Store clock
			if (error) return NULL;
			getStringValue(t, 1, m->device, sizeof(m->device), &error);	// Store device name
			if (error) return NULL;
			getStringValue(t, 2, m->units, sizeof(m->units), &error);	// Store units
			if (error) return NULL;
			len = getDoubleArray(t, 3, b, sizeof(b), &error);			// Get vector
			if (error) return NULL;
			errno = 0;
			if (len == 1) {
				m->vector_size = 0;										// Scalar value stored
				m->value = b[0];										// Store value 
			} else if (len > 1) {
				m->vector_size = len;									// Store vector length
				m = realloc(m, sizeof(*m) + sizeof(double)*len);		// Get more memory to store vector
				if (m) {												// Success
					bcopy(b, m->vector, sizeof(double)*len);			// Store vector
				}
			}
		}
	}
	return m;															// Return result
}

/* 
 * Returns the first tuple in the dataset
 */
Measurement getFirstMeasurement(Dataset dataset) 
{
	return getMeasurement(dataset, 1);
}

/* 
 * Returns the next tuple if available
 */
Measurement getNextMeasurement(Dataset dataset) 
{
    return getMeasurement(dataset, getIndex(dataset));
}

int releaseMeasurement(Measurement m)
{
    if (m!=NULL) free(m);
    return 0;
}

void setUserAgent(char *text)
{
    memccpy(g_uagent, text, 0, MAX_UAGENT_SIZE);
}
