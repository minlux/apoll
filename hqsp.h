//-----------------------------------------------------------------------------
/*!
   \file
   \brief Http Query String Parser (hqsp)

   - Get request methond: GET/POST
   - Get requested resource (e.g. submit.html)
   - Get value of GET parameter (e.g. value='pass' -> value='wgk3S')

   Examples:
   ---------
   GET /config.html HTTP/1.1
   GET /submit.html?ssid=HEISS&pass=wgk3S HTTP/1.1
   POST /api/setTemperature/kitchen
*/
//-----------------------------------------------------------------------------
#ifndef HQSP_H_
#define HQSP_H_

/* -- Includes ------------------------------------------------------------ */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -- Defines ------------------------------------------------------------- */

/* -- Types --------------------------------------------------------------- */

/* -- Global Variables ---------------------------------------------------- */

/* -- Function Prototypes ------------------------------------------------- */

//returns 0 (false) or 1 (true)
int hqsp_is_method_get(const char * request);

//returns 0 (false) or 1 (true)
int hqsp_is_method_post(const char * request);

//get "string-pointer" to requested resource
//x will be set to the start of the string
//returns length of string
int hqsp_get_resource(const char * request, const char ** x);

//get "string-pointer" to value of the given header
//x will be set to the start of the string
//returns length of string; 0 if request doesn't contain the given header
int hqsp_get_header_value(const char * request, const char * header, const char ** x);

//get "string-pointer" to value of the given parameter
//x will be set to the start of the string
//returns length of string; 0 if request doesn't contain the given paramete
int hqsp_get_parameter_value(const char * request, const char * parameter, const char ** x);

//returns the http status code (e.g. 200, 404, etc)
int hqsp_get_status_code(const char * response);

//get "string-pointer" to requested resource
//x will be set to the start of the string
//returns length of string
int hqsp_get_post_content(const char * request, const unsigned requestLen, const char ** x);

/* -- Implementation ------------------------------------------------------ */



#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
