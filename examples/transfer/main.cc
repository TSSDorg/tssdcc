#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()

#define SA struct sockaddr
#include "flat.h"
#include "tssd.h"
#include "types.h"

using namespace std;

void recvStudent(int sockfd)
{
    tssd::Manager::Register<Student>();
    tssd::RBuffer fbuf;  // RBuffer to process raw data buffer
    Student student;   // object to receive
    size_t more(0);

    SocketReader socketReader(sockfd);
    if (fbuf.Feed(socketReader) != tssd::OK || !fbuf.Ready(student.Family(), student.Version())) {
        cout << "recv Student err" << endl;
        return;
    }
    auto dbuf = fbuf.Buffer(student.Family(), student.Version());

    if (tssd::Manager::UnmarshalTo(*dbuf, student) != tssd::OK) {
        cout << "unmarshal err:" << endl;
        return;
    }
    // process your data
    student.print();
}

bool sendRequest(int sockfd, int16_t fid)
{
    Request request;
    request.fid = fid;
    tssd::Manager::Register<Request>();
    tssd::Buffer buf(256);
    if (auto ret = tssd::Manager::MarshalTo(request, buf)) {
        cout << "tssd Marshal error:" << ret << endl;
        return false;
    }
    buf.print();

    auto frags  = buf.Fragments();
    for (int i=0; i<frags.size(); i++) {
        //std::span<std::byte> sp(frags[i]->data.data(), frags[i]->data.size());
        int n = write(sockfd, frags[i]->data.data(), frags[i]->data.size());
        if (n<=0) {
            cout << "tcp client send fail" << endl;
            return false;
        }
        cout << "send " << n << " bytes" << endl;
    }
    return true;
}

int server(int argc, char *argv[]);
int main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4) {
        cout << "usage:" << argv[0] << " addr port [fid]" << endl;
        return -1;
    }
    if (argc == 3)
        server(argc, argv);
    int sockfd, connfd;
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

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    // function for chat
    sendRequest(sockfd, std::stoi(argv[3]));
    recvStudent(sockfd);

    // close the socket
    close(sockfd);
    return 0;
}
