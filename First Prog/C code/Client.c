#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

char SERVER_IP[1024] = {0};
int PORT = 8080;

void print_menu() {
    printf("\nWhat do you want to do?\n");
    printf("1. Check available rooms\n");
    printf("2. Book a room\n");
    printf("3. Leave the hotel\n");
    printf("4. Exit\n");
    printf("Enter your choice: ");
}

int read_integer() {
    char buffer[1024] = {0};
    fgets(buffer, 1024, stdin);
    buffer[strcspn(buffer, "\r\n")] = 0;
    return atoi(buffer);
}

void handle_response(char buffer[]) {
    if (strcmp(buffer, "invalid_room") == 0) {
        printf("Invalid room number\n");
    } else if (strcmp(buffer, "room_not_available") == 0) {
        printf("Room is not available\n");
    } else if (strcmp(buffer, "not_enough_money") == 0) {
        printf("Not enough money\n");
    } else if (strcmp(buffer, "room_booked") == 0) {
        printf("Room is booked\n");
    } else if (strcmp(buffer, "no_room_booked") == 0) {
        printf("No room is booked\n");
    } else if (strcmp(buffer, "room_cleared") == 0) {
        printf("Room is cleared\n");
    } else if (strlen(buffer) > 0) {
        printf("Available rooms: %s\n", buffer);
    } else {
        printf("No Available rooms\n");
    }
}

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
    
    while (true) {
        char buffer[1024] = {0};
        print_menu();
        int choice = read_integer();
        if (choice == 1) {
            char msg[1024];
            sprintf(msg, "get_available_rooms ");
            send(sock, msg, sizeof(msg), 0);
            read(sock, buffer, 1024);
            handle_response(buffer);
        } else if (choice == 2) {
            printf("Enter room number: ");
            int room_number = read_integer();
            char msg[1024];
            sprintf(msg, "book_room %d", room_number);
            send(sock, msg, sizeof(msg), 0);
            read(sock, buffer, 1024);
            handle_response(buffer);
        } else if (choice == 3) {
            send(sock, "leave_hotel", sizeof("leave_hotel"), 0);
            read(sock, buffer, 1024);
            handle_response(buffer);
        } else if (choice == 4) {
            break;
        } else {
            printf("Invalid choice\n");
        }
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
