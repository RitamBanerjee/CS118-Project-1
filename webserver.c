/* A simple server in the internet domain using TCP
   The port number is passed as an argument
   This version runs forever, forking off a separate
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */

#define BUFFER_SIZE 512

#define STATUS_OK "200 OK"
#define STATUS_NOT_FOUND "404 Not Found"

void handleRequest(int); /* function prototype */
char* getFileName(char*);
void sendResponse(int, char*);
char* getContentType(char*);
char* getFile(char*);
size_t getFileSize(char*);

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0)
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);

     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0)
              error("ERROR on binding");

     listen(sockfd,5);

     clilen = sizeof(cli_addr);

     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/

     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

         if (newsockfd < 0)
             error("ERROR on accept");

         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");

         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             handleRequest(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this
     } /* end of while */
     return 0; /* we never get here */
}

/******** handleRequest() *********************
 There is a separate instance of this function
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void handleRequest (int sock)
{
   int n;
   char buffer[BUFFER_SIZE];

   bzero(buffer,BUFFER_SIZE);
   n = read(sock,buffer,BUFFER_SIZE-1);
   if (n < 0) error("ERROR reading from socket");
   printf("Here is the message:\n%s\n\n",buffer);

   // extract filename from buffer
   char* filename = getFileName(buffer);
   if (strlen(filename) != 0) {
      printf("File requested: %s\n", filename);
   }
   else {
      filename = malloc(strlen("index.html")+1);
      strcpy(filename, "index.html");
      printf("No file requested, using: %s\n", filename);
    // printf("No file requested\n");
   }
   sendResponse(sock, filename);
}

// extract filename from buffer
char* getFileName (char* request) {
    // get the first line
   char* line = strtok(request, "\n");

   // get the filename from the first line
   char* filename = strtok(line, " ");
   filename = strtok(NULL, " ");
   filename++;
   return filename;
}

// extract content-type from the filename
char* getContentType (char* filename) {
    // get filetype from the filename
    char* fileType = strtok(filename, ".");
    fileType = strtok(NULL, " ");

    // read filetype and assign content-type to it
    char* contentType;
    if (strcmp(fileType, "html") == 0) {
        contentType = "Content-Type: text/html\n\n";
    }
    else if (strcmp(fileType, "gif") == 0) {
        contentType = "Content-Type: image/gif\n\n";
    }
    else if (strcmp(fileType, "jpeg") == 0 || strcmp(fileType, "jpg") == 0) {
        contentType = "Content-Type: image/jpeg\n\n";
    }
    else {
        contentType = STATUS_NOT_FOUND;
    }
    return contentType;
}

// retrieve file from the system
char* getFile(char* filename) {
    char* buffer;
    FILE *fp;
    int fileSize;

    // open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        return STATUS_NOT_FOUND;
    }

    /* Get the number of bytes */
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);

    /* reset the file position indicator to the beginning of the file */
    fseek(fp, 0, SEEK_SET);

    buffer = malloc(sizeof(char)*fileSize);

    // copy file into buffer
    fread(buffer, sizeof(char), fileSize, fp);
    fclose(fp);
    return buffer;
}

// get filesize of file
size_t getFileSize(char *filename) {

    // similar to the above, but returning the filesize instead of the actual file
    char* buffer;
    FILE *fp;
    int fileSize;

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        return 0;
    }

    /* Get the number of bytes */
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fclose(fp);
    return fileSize;
}

// takes filename and generates response
void sendResponse (int sock, char* filename) {

    // initializing variables so we can just return them if they came up
    // in thise case we are just going to return 404 for both cases of not
    // found and unsupported file type
    char* errorNotFoundResponse = "HTTP/1.1 404 Not Found\n\n"
                    "<html><body><h1>404 Not Found</h1></body></html>";
    char* errorNotSupported = "HTTP/1.1 404 Not Found\n\n"
                    "<html><body><h1>Content Type Not Supported</h1></body></html>";

    // declaring necessary variables for the reponse header
    char *connection, *status, *contentType, *date, *server, *lastModified;

    // getting file and filesize based on filename
    char* file = getFile(filename);
    // printf("%s\n", file);
    size_t fileSize = getFileSize(filename);
    char size[16];
    sprintf(size, "%zu", fileSize);

    // check if found, if not found, then return not found
    if (strcmp(file, STATUS_NOT_FOUND) == 0) {
        int n = send(sock,errorNotFoundResponse,strlen(errorNotFoundResponse),0);
        if (n < 0) {
            error("ERROR writing to socket");
        }
        return;
    }
    else {    // if found, set status to ok for now
        status = "HTTP/1.1 200 OK\n";
    }

    // set content type to the proper one
    contentType = getContentType(filename);
    // printf("%s\n", contentType);

    // send not found response if not proper content type
    if (strcmp(contentType, STATUS_NOT_FOUND) == 0) {
        int n = send(sock,errorNotSupported,strlen(errorNotSupported),0);
        if (n < 0) {
            error("ERROR writing to socket");
        }
        return;
    }

    // set appropriate headers
    connection = "Connection: close\n";
    date = "Date: Tue, 09 Aug 2011 15:44:04 GMT\n";
    server = "Server: Apache/2.2.3 (CentOS)\n";
    lastModified = "Last-Modified: Tue, 09 Aug 2011 15:11:03 GMT\n";
    char* contentLength = malloc(50);
    strcpy(contentLength, "Content-Length: ");
    strcat(contentLength, size);
    strcat(contentLength, "\n");
    // printf("%s\n", contentLength);

    int bufferLength = strlen(status) + strlen(connection) + strlen(date)
                        + strlen(server) + strlen(lastModified) + strlen(contentLength)
                        + strlen(contentType);
    char* response = malloc(bufferLength);
    strcat(response, status);
    strcat(response, connection);
    strcat(response, date);
    strcat(response, server);
    strcat(response, lastModified);
    strcat(response, contentLength);
    strcat(response, contentType);

    // char* okResponse = "HTTP/1.1 200 OK\n"
    //                  "Connection: close\n"
    //                  "Date: Tue, 09 Aug 2011 15:44:04 GMT\n"
    //                  "Server: Apache/2.2.3 (CentOS)\n"
    //                  "Last-Modified: Tue, 09 Aug 2011 15:11:03 GMT\n"
    //                  "Content-Length: 44\n"
    //                  "Content-Type: text/html\n\n"
    //                  "<html><body><h1>Test Page</h1></body></html>";

    printf("Sending response:\n%s\n\n", response);

    int n = send(sock,response,strlen(response),0);

    if (n < 0) {
        error("ERROR writing to socket");
    }

    n = send(sock,file,fileSize,0);

    if (n < 0) {
        error("ERROR writing to socket");
    }
}
