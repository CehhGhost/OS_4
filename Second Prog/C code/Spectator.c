#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

char SERVER_IP[1024] = {0};
int PORT = 8080;

void connect_to_server() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\n Invalid address/ Address not supported \n");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\n Connection Failed \n");
        return;
    }

    char msg[1024];
    sprintf(msg, "spectator");
    send(sock, msg, sizeof(msg), 0);

    while (1) {
        char buffer[1024] = {0};
        int msg_size = read(sock, buffer, 1024);
        if (msg_size <= 0) {
            printf("Server disconnected\n");
            break;
        }
        printf("%s", buffer);
    }

    close(sock);
}

int main(int argc, char const *argv[]) {
    if (argc == 3) {
        strcat(SERVER_IP, argv[1]);
        PORT = atoi(argv[2]);
    } else {
        strcat(SERVER_IP, "127.0.0.1");
    }
    connect_to_server();
    return 0;
}

