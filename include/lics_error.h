

#ifndef LICENSE_ERROR_H
#define LICENSE_ERROR_H


#define ELICS_OK    (0)
/* 
 * make enough room for grpc error, now grpc use [0-16], but it may grow up in the future, 
 * so we start it from ELICS_BASE(10000).
*/

#define ELICS_BASE  (10000)
#define ELICS_UNKOWN_ERROR (ELICS_BASE + 1)
#define ELICS_CLIENT_NOT_EXIST (ELICS_BASE + 2)
#define ELICS_AUTH_FAILURE (ELICS_BASE + 3)
#define ELICS_ALGO_NOT_EXIST (ELICS_BASE + 4)

#endif