#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> // read(), write(), close()
#include <arpa/inet.h>

#include "flat.h"
#include "tssd.h"
#include "types.h"

using namespace std;

#define SA struct sockaddr


int recvRequest(int sockfd)
{
    tssd::Manager::Register<Request>();
    SocketReader socketReader(sockfd);
    Request request;
    tssd::RBuffer rbuf;
    if (auto ret = rbuf.Read(socketReader, request)) {
        printf("recv request failure: %d\n", ret);
    }

    printf("request: %d %s %s\n", request.fid, request.tid.c_str(), request.types.c_str());

    return request.fid;
}

bool sendStudent(int sockfd)
{
    Student student;
    student.name = "George W. Bush";
    student.age = 25;
    student.isMale = true;

    tssd::Manager::Register<Student>();
    SocketWriter socketWriter(sockfd);
    auto ret = tssd::RBuffer::Write(student, socketWriter);
    if (ret != tssd::OK) {
        printf("Write data failure: %d\n", ret);
        return false;
    }
    return true;
}

void func(int connfd)
{
    // infinite loop for chat
    for (;;) {
        auto fid = recvRequest(connfd);
        sendStudent(connfd);
    }
}

// Driver function
int server(int argc, char *argv[])
{
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    servaddr.sin_port = htons((short)std::stoi(argv[2]));

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");
    len = sizeof(cli);

    // Accept the data packet from client and verification
    connfd = accept(sockfd, (SA*)&cli, &len);
    if (connfd < 0) {
        printf("server accept failed...\n");
        exit(0);
    }
    else
        printf("server accept the client...\n");

    // Function for chatting between client and server
    func(connfd);

    // After chatting close the socket
    close(sockfd);
}
