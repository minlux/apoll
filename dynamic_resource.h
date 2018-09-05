//---------------------------------------------------------------------------------------------------------------------
/*!
   \file
   \brief Container for "topics" published on the server and "pushed" to clients.
*/
//---------------------------------------------------------------------------------------------------------------------
#ifndef DYNAMIC_RESOURCE_H_INCLUDED
#define DYNAMIC_RESOURCE_H_INCLUDED

/* -- Includes ------------------------------------------------------------ */
#include <string>
#include <stdint.h>



/* -- Defines ------------------------------------------------------------- */

/* -- Types --------------------------------------------------------------- */
class DynamicResource
{
public:
   DynamicResource(const std::string& uri, const std::string& statusCode="200 OK");
   void setContentType(const std::string& contentType);
   void setContent(const std::string& content);

   std::string uri;
   std::string content;
   std::string contentType;
   std::string statusCode;
   uint32_t hash;
};


/* -- Global Variables ---------------------------------------------------- */

/* -- Function Prototypes ------------------------------------------------- */

/* -- Implementation ------------------------------------------------------ */



#endif // TCPCONNECT_H_INCLUDED
