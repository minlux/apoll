//-----------------------------------------------------------------------------
/*!
   \file
   \brief Http Query String Parser (hqsp)
*/
//-----------------------------------------------------------------------------


/* -- Includes ------------------------------------------------------------ */
#include <string.h>
#include <stdlib.h>
#include "hqsp.h"

/* -- Defines ------------------------------------------------------------- */

/* -- Types --------------------------------------------------------------- */

/* -- Global Variables ---------------------------------------------------- */

/* -- Module Global Variables --------------------------------------------- */

/* -- Module Global Function Prototypes ----------------------------------- */
static int is_delimiter(char c);


/* -- Implementation ------------------------------------------------------ */

int hqsp_is_method_get(const char * request)
{
   int isGet = 1;
   isGet &= (request[0] == 'G');
   isGet &= (request[1] == 'E');
   isGet &= (request[2] == 'T');
   return isGet;
}


int hqsp_is_method_post(const char * request)
{
   int isPost = 1;
   isPost &= (request[0] == 'P');
   isPost &= (request[1] == 'O');
   isPost &= (request[2] == 'S');
   isPost &= (request[3] == 'T');
   return isPost;
}


int hqsp_get_resource(const char * request, const char ** x)
{
   const char * start;
   const char * end;
   int len;

   //find start token '/'
   for (start = request; *start != '/'; ++start) { if ((*start == 0) || (*start == '\n')) return 0; }
   *x = start;
   //determin length
   for (end = start, len = 0; !is_delimiter(*end); ++end, ++len);
   //set results
   return len;
}



int hqsp_get_header_value(const char * request, const char * header, const char ** x)
{
   const int headerLen = strlen(header);
   const char * iterator;
   const char * start;
   const char * end;
   int matchLen;
   int len;

   start = NULL;
   matchLen = 0;
   uint32_t shiftReg = 0;
   const uint32_t endOfHeader = ((uint32_t)'\r' << 24) | ((uint32_t)'\n' << 16) | ((uint32_t)'\r' << 8) | (uint32_t)'\n'; //end of header is indicated by two "\r\n"
   for (iterator = request; shiftReg != endOfHeader; ++iterator) //until end of header ...
   {
      if (*iterator == 0) return 0;
      shiftReg = (shiftReg  << 8) | (uint8_t)*iterator;

      if (*iterator == header[matchLen]) //match?
      {
         ++matchLen; //compare next char of header
         if (matchLen == headerLen)
         {
            //lock ahead if next two chars are ':' and ' '
            if ((iterator[1] == ':') && (iterator[2] == ' '))//yes
            {
               start = &iterator[3]; //set start of value poitner
               break; //stop searching
            }
            matchLen = 0; //otherwise: header not found -> restart comparison from begining
         }
         continue;
      }
      matchLen = 0; //no match -> restart comparison from begining
   }

   //check if parameter was found
   if (start != NULL)
   {
      *x = start;
      //determin length
      for (end = start, len = 0; *end != '\r'; ++end, ++len); //until end of line (marked by "\r\n")
      //set results
      return len;
   }

   //header not found
   return 0;
}


int hqsp_get_parameter_value(const char * request, const char * parameter, const char ** x)
{
   const int parameterLen = strlen(parameter);
   const char * iterator;
   const char * start;
   const char * end;
   int matchLen;
   int len;

   start = NULL;
   matchLen = 0;
   for (iterator = request; (*iterator != 0) && (*iterator != '\n'); ++iterator)
   {
      if (*iterator == parameter[matchLen]) //match?
      {
         ++matchLen; //compare next char of parameter
         if (matchLen == parameterLen)
         {
            //lock ahead if next char is '=' token
            if (iterator[1] == '=') //yes
            {
               start = &iterator[2]; //set start of value poitner
               break; //stop searching
            }
            matchLen = 0; //otherwise: parameter not found -> restart comparison from begining
         }
         continue;
      }
      matchLen = 0; //no match -> restart comparison from begining
   }

   //check if parameter was found
   if (start != NULL)
   {
      *x = start;
      //determin length
      for (end = start, len = 0; !is_delimiter(*end); ++end, ++len);
      //set results
      return len;
   }

   //parameter not found
   return 0;
}


int hqsp_get_status_code(const char * response)
{
   return atoi(&response[9]);
}


int hqsp_get_post_content(const char * request, const unsigned requestLen, const char ** x)
{
   const char * start;
   unsigned len;

   //find start of post data, preceded by "\r\n\r\n"
   uint32_t shiftReg = 0;
   const uint32_t endOfHeader = ((uint32_t)'\r' << 24) | ((uint32_t)'\n' << 16) | ((uint32_t)'\r' << 8) | (uint32_t)'\n';
   for (start = request, len = 0; shiftReg != endOfHeader; ++start, ++len)
   {
      if (*start == 0)
      {
      return 0;
      }
      shiftReg = (shiftReg  << 8) | (uint8_t)*start;
   }
   *x = start;
   //determin remaining length
   return (requestLen - len);
}



//check if char of query string is a delimiter token
static int is_delimiter(char c)
{
   switch (c)
   {
   case 0:
   case '\r': //0x0D
   case '\n': //0x0A
   case ' ':
   case '?':
   case '&':
      return 1;
   }
   return 0;
}


