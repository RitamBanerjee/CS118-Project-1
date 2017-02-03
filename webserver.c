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
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */


void handleRequest(int); /* function prototype */
char* getFileName(char*);
char* getContentType(char*);

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
   int buffer_size = 512;
   char buffer[buffer_size];

   bzero(buffer,buffer_size);
   n = read(sock,buffer,buffer_size-1);
   if (n < 0) error("ERROR reading from socket");
   printf("Here is the message:\n%s\n\n",buffer);
   char* filename = getFileName(buffer);

   if (strlen(filename) != 0) {
      printf("File requested: %s\n", filename);
   }
   else {
      printf("No file requested\n");
   }

   char* response = "HTTP/1.1 200 OK\n"
                     "Connection: close\n"
                     "Date: Tue, 09 Aug 2011 15:44:04 GMT\n"
                     "Server: Apache/2.2.3 (CentOS)\n"
                     "Last-Modified: Tue, 09 Aug 2011 15:11:03 GMT\n"
                     "Content-Length: 44\n"
                     "Content-Type: text/html\n\n"
                     "<html><body><h1>Test Page</h1></body></html>";
                     
   printf("Sending response:\n%s\n\n", response);

   n = write(sock,response,strlen(response));
   if (n < 0) error("ERROR writing to socket");
}

char* getFileName (char* request) {
   char* line = strtok(request, "\n");
   char* filename = strtok(line, "/");
   filename = strtok(NULL, " ");
   return filename;
}
