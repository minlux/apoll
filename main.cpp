//-----------------------------------------------------------------------------
/*!
   \file
   \brief Apoll: Simple web server supporting long polling.

   This implementation is based on a super-loop application with non-blocking sockets.
   The server can handle static content, as every other webserver. In addition it can
   handle so called "dynamic content". Thats content that is pushed the server via POST
   requests and pulled from the server using GET request. Dynamic content is identified by
   a "HASH value". This hash value is exchanged using a custom HTTP header "Content-Hash".
   When a client requests a dynmaic resource it may set that custom HTTP header. If it
   doesn't, the request will be served immediatelly(*) and the server will reply the
   content of the resource. The server will also send the HASH value of the content using
   HTTP header "Content-Hash". This HASH value can then be used for subsequent (long polling)
   requests by the client with the header "Content-Hash" set!
   So if the client request a dynamic resource with HTTP header "Content-Hash" set to
   a value X, the server compares X to the HASH value of the requested resource.
   If the HASH values are different, the request will be served immediatelly, the server
   will reply the content and the (new) HASH of the resource.
   However if the HASH values are equal, the server will not reply. Instead, it will leave
   the connection open, until the content of the requested resource was changed by other
   POST requests served in the meanwile. As far as the content of the dynamic resource has
   changed (and thereby its HASH) the server will will reply the (updated) content and the
   (new) HASH of the resource.

   Usage:
   ------
   Usage: apoll [HTML-base-path] [TCP-port-number]
   - HTML-base-path:
      Absolute or relative path to the base folder that shall be served by apoll.
      The path must not be prepended with a '/'. E.g. '/home/users/webmaster/www'
      or '~/www' etc. Default is '.'
      Within that folder, must be a file called 'dynres.txt' that contains the definition
      of all the available, dynamic resources specified line-by-line. One line specifies
      the URI of the dynamic resource, e.g. '/getTemperature' or '/api/service/xy'

   - TPC-port-number:
      The TCP port number apoll shall listen to. E.g. 8080. Default is 8083.


   Program Flow:
   -------------
   1) A server socket is listening for incomming connections. Whenever the server socket
   accepts a new connection, that connection is added to a list of active connections.

   2) All active connections are polled for incomming messages, cyclicalliy. If a
   connection was closed remotely, it is removed from the list of active connections.
   However if a HTML request is received on an active connection, the HTML method (GET or POST)
   defines the subsequent processing:

   2a) GET:
   --------
   First it is checked, if the requestd resource is static content, provided by a file.
   If that true, the file content is replied, the connection is closed and removed
   from the list of active connections.
   Otherwise, it is checked if the request resource is dynamic content. If thats true,
   The request is deferred for subsequent processing.
   If the request addresses neither available static nor available dynamic content,
   the request is anwsered with a "404 Not Found" reply. The connection is closed and
   removed from the list of active connections.

   2b) POST:
   ---------
   POST request are only possible for dynamic content. Therefore it is checked, if the
   requested resource is dynamic content. It thats true, the POST data of the request
   is stored in the respective "dynamic-resource-object". The request is answered with
   a "200 OK" reply. The connection is closed and removed from the list of active
   connections.
   If the requested dynamic content is not available, the request is answered with a
   "400 Not Found". The connection is closed and removed from the list of active
   connections.

   3) For all active connections, with a pending, deferred request, the server compares
   the HASH value of the request with the HASH value of the resource.If they are different,
   the request is answered with a "200 OK" reply, the content and the new HASH. The
   connection is closed and removed from the list of active  connections.


   ---------------------------------------------------------
   (*) Only for non empty dynamic resources. Request to empty dynamic resources are also deferred!
*/
//-----------------------------------------------------------------------------

/* -- Includes ------------------------------------------------------------ */
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <list>
#include <vector>
#include "tcp_connection.h"
#include "dynamic_resource.h"
#include "hqsp.h"



/* -- Defines ------------------------------------------------------------- */
using namespace std;


/* -- Types --------------------------------------------------------------- */
typedef struct
{
   NbTcpConnection * connection;
   DynamicResource * resource;
   uint32_t hash;
} Connection;



/* -- (Module) Global Variables ------------------------------------------- */
static int ctrlC;
static DynamicResource * code200;
static DynamicResource * code404;
static string htmlBasePath;

/* -- Module Global Function Prototypes ----------------------------------- */
static int m_serve_requests(Connection& connection, list<DynamicResource *>& dynamicResources);
static int m_reply_dynamic_content(Connection& connection);
static int m_reply_static_content(Connection& connection, const string& uri);
static string m_get_content_type_by_uri(const string& uri, const string& fallback);


/* -- Implementation ------------------------------------------------------ */
void m_signal_handler(int a)
{
   ctrlC = 1;
}


int main(int argc, const char * argv[])
{
   list<DynamicResource *> dynamicResources;
   list<Connection> connections;
   list<Connection>::iterator conIt;
   uint16_t port;
   int status;

   //process command line arguments
   if (argc == 3)
   {
      htmlBasePath = argv[1];
      port = (uint16_t)atoi(argv[2]);
   }
   else //otherwise: use defaults
   {
      cout << "Usage: apoll [HTML-base-path] [TCP-port-number]" << endl;
      htmlBasePath = "."; //"this" directory
      port = 8083; //default port
   }


   //create default resources
   code200 = new DynamicResource("/200", "200 OK");
   code200->setContent("OK");
   code404 = new DynamicResource("/200", "404 Not Found");
   code404->setContent("Not Found");

   //create dynamic resources, as specified in "dynres.txt"
   const string filePath = htmlBasePath + "/dynres.txt";
   ifstream file(filePath, ios::in);
   if (file.is_open())
   {
      string uri;
      //read out file, line by line
      while (getline(file, uri))
      {
         if (uri[0] == '/') //only those lines, that starts with a '/'
         {
            //add to list of dynamic resources
            dynamicResources.push_back(new DynamicResource(uri));
         }
      }
      file.close();
   }


   //create server
   NbTcpServer * tcpServer = new NbTcpServer();
   status = tcpServer->open(port);
   if (status < 0)
   {
      cout << "Failed to open server on port " << port << endl;
      return -1;
   }
   cout << "Running webserver on port: " << port << endl;
   cout << "HTML base path: " << htmlBasePath << endl;
   cout << "Use CTRL+C to quit!" << endl;


   //register signal handler, to quit program usin CTRL+C
   signal(SIGINT, &m_signal_handler);

   //enter super-loop
   while (!ctrlC)
   {
      Connection con;

      //push new tcp connections to "connection list"
      con.connection = tcpServer->serve();
      if (con.connection)
      {
         //add to this connection to the list of active connections
         con.resource = NULL;
         con.hash = 0;
         connections.push_back(con);
      }


      //for each connection ...
      //receive HTTP requests
      conIt = connections.begin();
      while (conIt != connections.end())
      {
         status = m_serve_requests(*conIt, dynamicResources);
         if (status != 0) //close connection
         {
            //close that connection
            conIt->connection->close();
            delete conIt->connection;
            //remove from list of active connections
            conIt = connections.erase(conIt);
            continue;
         }
         conIt++;
      }


      //for each connection ...
      //reply dynamic content
      conIt = connections.begin();
      while (conIt != connections.end())
      {
         status = m_reply_dynamic_content(*conIt);
         if (status != 0) //close connection
         {
            //close that connection
            conIt->connection->close();
            delete conIt->connection;
            //remove from list of active connections
            conIt = connections.erase(conIt);
            continue;
         }
         conIt++;
      }

      //
      usleep(50000); //50ms
   }


   //shutdown server
   tcpServer->close();
   delete tcpServer;

   //delete still open connections
   conIt = connections.begin();
   while (conIt != connections.end())
   {
      conIt->connection->close();
      delete conIt->connection;
      conIt++;
   }


   //delete dynamic resources
   list<DynamicResource *>::iterator resIt = dynamicResources.begin();
   while (resIt != dynamicResources.end()) //find requested resource
   {
      DynamicResource * res = *resIt++;
      delete res;
   }
   delete code404;
   delete code200;


   return 0;
}



//return 0 when connection stays open
//return -1 when connection was closed remotely
//return 1 when connection shall be closed
static int m_serve_requests(Connection& connection, list<DynamicResource *>& dynamicResources)
{
   uint8_t buffer[4096];
   int status;

   //check for incomming data
   status = connection.connection->recv(buffer, sizeof(buffer) - 1);

   //connection closed ?
   if (status < 0)
   {
      //connection was closed remotely
      return -1;
   }

   //nothing received
   if (status == 0)
   {
      return 0;
   }

   //otherwise - data received
   const unsigned requestLen = (unsigned)status;
   const char * resource;
   int resourceLen;
   bool isGET;
   bool isPOST;

   //invalidate earlier requests
   connection.resource = NULL;
   connection.hash = 0;

   //parse http request
   buffer[requestLen] = 0; //add termination, just to be safe
   resourceLen = hqsp_get_resource((const char *)buffer, &resource);
   string uri(resource, resourceLen); //uri: resoure as std::stirng
   if (uri == "/") uri = "/index.html"; //redirect to default page


   //GET
   isGET = hqsp_is_method_get((const char *)buffer);
   if (isGET)
   {
      //check if the requested resource is static content
      status = m_reply_static_content(connection, uri);
      if (status != 0) //yes it is ...
      {
         return status; //instruct caller to close connection
      }

      //otherwise
      //check if the requested resource is dynamic content
      list<DynamicResource *>::iterator resIt = dynamicResources.begin();
      while (resIt != dynamicResources.end()) //find requested resource
      {
         DynamicResource * res = *resIt++;
         if (res->uri == uri)
         {
            uint32_t contentHash = 0;
            const char * header;
            int headerLen;

            //clients may use long polling to get content
            //for the purpose of long polling, they may send a hash value for the already known content of a resource
            //by means of that hash value the server can decides weather new data must be sent to the server immediatly or on change
            headerLen = hqsp_get_header_value((const char *)buffer, "Content-Hash", &header);
            if (headerLen > 0)
            {
               contentHash = (uint32_t)strtoul(header, NULL, 10);
            }

            //link resource request to connection
            connection.resource = res;
            connection.hash = contentHash;
            return 0;
         }
      }
   }


   //POST
   isPOST = hqsp_is_method_post((const char *)buffer);
   if (isPOST)
   {
      //POST can only deal with dynamic content
      //find requested res
      list<DynamicResource *>::iterator resIt = dynamicResources.begin();
      while (resIt != dynamicResources.end())
      {
         DynamicResource * res = *resIt++;
         if (res->uri == uri)
         {
            const char * header;
            int headerLen;
            const char * postContent;
            int postContentLen;

            //get content type from HTML header -> set
            headerLen = hqsp_get_header_value((const char *)buffer, "Content-Type", &header);
            if (headerLen > 0)
            {
               string contentType(header, headerLen);
               res->contentType = contentType;
            }

            //get content that is sent via POST -> set
            postContentLen = hqsp_get_post_content((const char *)buffer, requestLen, &postContent);
            string content(postContent, postContentLen);
            res->setContent(content);

            //link resource "200 OK" to that connection in order to "acknowledge" the POST request
            connection.resource = code200;
            connection.hash = 0;
            return 0;
         }
      }
   }


   //when we come to that point, we havn't found the requested resource. Therfore...
   //link resource "404 Not Found" to that connection in order to "acknowledge" the request
   connection.resource = code404;
   connection.hash = 0;
   return 0;
}



//return 0 when connection stays open
//return 1 when connection shall be closed
static int m_reply_dynamic_content(Connection& connection)
{
   DynamicResource * resource = connection.resource;
   if (resource != NULL)
   {
      //check if client needs to informed about modified content
      if (resource->hash != connection.hash)
      {
         const string& content = resource->content;
         string header;

         //update client ...
         //send header
         header  = "HTTP/1.1 " + resource->statusCode + "\r\n";
         header += "Content-Type: " + resource->contentType + "\r\n";
         header += "Content-Hash: " + to_string(resource->hash) + "\r\n";
         header += "Content-Length: " + to_string(content.length()) + "\r\n";
         header += "\r\n";
         connection.connection->send((const uint8_t *)header.c_str(), header.length(), true);
         //send content
         connection.connection->send((const uint8_t *)content.c_str(), content.length());

         //invalidate request
         connection.resource = NULL;
         connection.hash = 0;
         return 1; //instruct to close connection
      }
   }

   //leave connectin open
   return 0;
}


//return 0 when connection stays open
//return 1 when connection shall be closed
static int m_reply_static_content(Connection& connection, const string& uri)
{
   const string contentType = m_get_content_type_by_uri(uri, "application/octet-stream"); //default: binary data
   const string filePath = htmlBasePath + uri;
   ifstream file(filePath, ios::in | ios::binary);
   if (file.is_open())
   {
      //get size of file
      file.seekg(0, ios::end);
      int fileSize = file.tellg();

      //read file content into tbuffer
      uint8_t* buffer = new uint8_t[fileSize];
      file.seekg(0, ios::beg);
      file.read((char *)buffer, fileSize);

      //close file
      file.close();

      //send header
      string header = "HTTP/1.1 200 OK\r\n";
      header += "Content-Type: " + contentType + "\r\n";
      header += "Content-Length: " + to_string(fileSize) + "\r\n";
      header += "\r\n";
      connection.connection->send((const uint8_t *)header.c_str(), header.length(), true);
      //send content
      connection.connection->send(buffer, fileSize);

      //free buffer
      delete[] buffer;
      return 1; //instruct to close connection
   }
   return 0; //not found
}


static string m_get_content_type_by_uri(const string& uri, const string& fallback)
{
   //get file extension
   string fileExtension = uri.substr(uri.find_last_of(".") + 1);

   //text files
   if (fileExtension == "txt")
   {
      return "text/plain";
   }
   if ((fileExtension == "htm") || (fileExtension == "html"))
   {
      return "text/html";
   }
   if (fileExtension == "css")
   {
      return "text/css";
   }
   if (fileExtension == "js")
   {
      return "text/javascript";
   }

   //image files
   if (fileExtension == "gif")
   {
      return "image/gif";
   }
   if (fileExtension == "png")
   {
      return "image/png";
   }
   if ((fileExtension == "jpg") || (fileExtension == "jpeg"))
   {
      return "image/jpeg";
   }
   if (fileExtension == "bmp")
   {
      return "image/bmp";
   }
   if (fileExtension == "ico")
   {
      return "image/x-icon";
   }

   //fallback
   return fallback;
}