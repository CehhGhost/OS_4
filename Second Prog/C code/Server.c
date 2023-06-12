#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

int PORT = 8080;
#define ROOMS_AMOUNT 25
#define MAX_SPECTATORS 10

pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t spec_mutex = PTHREAD_MUTEX_INITIALIZER;

struct guest {
    int id;
    int budget;
    int room_number;
};

struct room {
    int id;
    int price;
    bool is_free;
};

struct room rooms[ROOMS_AMOUNT];
int spectator_sockets[MAX_SPECTATORS];
int spectator_count = 0;

void init_rooms() {
    for (int i = 0; i < 10; i++) {
        rooms[i].id = i+1;
        rooms[i].price = 2000;
        rooms[i].is_free = true;
    }
    for (int i = 10; i < 20; i++) {
        rooms[i].id = i+1;
        rooms[i].price = 4000;
        rooms[i].is_free = true;
    }
    for (int i = 20; i < ROOMS_AMOUNT; i++) {
        rooms[i].id = i+1;
        rooms[i].price = 6000;
        rooms[i].is_free = true;
    }
}

void init_guest(struct guest *g, int id) {
    srand(time(NULL));
    g->id = id;
    g->budget = rand() % 10000 + 1000;
    g->room_number = 0;
}

void broadcast_to_spectators(char* message) {
    pthread_mutex_lock(&spec_mutex);
    for (int i = 0; i < spectator_count; i++) {
        send(spectator_sockets[i], message, strlen(message), 0);
    }
    pthread_mutex_unlock(&spec_mutex);
}

void* handle_spectator(void* socket) {
    int* socket_fd = (int*)socket;
    while (1) {
        char buffer[1024] = {0};
        int valread = read(*socket_fd, buffer, 1024);
        if (valread <= 0) {
            pthread_mutex_lock(&spec_mutex);
            for (int i = 0; i < spectator_count; i++) {
                if (spectator_sockets[i] == *socket_fd) {
                    for (int j = i; j < spectator_count - 1; j++) {
                        spectator_sockets[j] = spectator_sockets[j+1];
                    }
                    spectator_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&spec_mutex);
            break;
        }
    }
    free(socket);
    pthread_exit(NULL);
}

void* handle_client(void* socket) {
    int* socket_fd = (int*)socket;
    struct guest g;
    init_guest(&g, *socket_fd);

    char message[1024] = {0};
    sprintf(message, "Guest %d with budget %d is connected\n", g.id, g.budget);
    printf("%s", message);
    broadcast_to_spectators(message);

    while (true) {
	char buffer[1024] = {0};
        int msg_size = read(*socket_fd, buffer, 1024);
        if (msg_size <= 0) {
            char message[1024] = {0};
            sprintf(message, "Guest %d disconnected\n", g.id);
            printf("%s", message);
            broadcast_to_spectators(message);
            pthread_mutex_unlock(&room_mutex);
            break;
        }
        if (strcmp(buffer, "get_available_rooms ") == 0) {
            pthread_mutex_lock(&room_mutex);
            char message[1024] = {0};
            sprintf(message, "Guest %d is looking for a room...\n", g.id);
            printf("%s", message);
            broadcast_to_spectators(message);
            char response[1024] = {0};
            for (int i = 0; i < ROOMS_AMOUNT; i++) {
                if (!rooms[i].is_free) {
                    continue;
                }
                if (g.budget >= rooms[i].price) {
                    char room_str[10];
                    sprintf(room_str, "%d", rooms[i].id);
                    strcat(response, room_str);
                    strcat(response, " ");
                }
            }
            pthread_mutex_unlock(&room_mutex);
            write(*socket_fd, response, sizeof(response));
        } else if (strncmp(buffer, "book_room ", 10) == 0) {
            pthread_mutex_lock(&room_mutex);
            int room_id = atoi(buffer + 10);
            if ((room_id < 1) || (room_id > ROOMS_AMOUNT)) {
                pthread_mutex_unlock(&room_mutex);
                write(*socket_fd, "invalid_room", sizeof("invalid_room"));
            } else if (!rooms[room_id-1].is_free) {
                pthread_mutex_unlock(&room_mutex);
                write(*socket_fd, "room_not_available", sizeof("room_not_available"));
            } else if (g.budget < rooms[room_id-1].price) {
                pthread_mutex_unlock(&room_mutex);
                write(*socket_fd, "not_enough_money", sizeof("not_enough_money"));
            } else {
                if (g.room_number != 0) {
                    rooms[g.room_number - 1].is_free = true;
                    printf("Guest %d left room %d and booking another\n", g.room_number - 1, room_id);
                }
                rooms[room_id-1].is_free = false;
                g.budget -= rooms[room_id-1].price;
                g.room_number = room_id;
                char message[1024] = {0};
                sprintf(message, "Guest %d booked room %d\n", g.id, room_id);
                printf("%s", message);
                broadcast_to_spectators(message);
                pthread_mutex_unlock(&room_mutex);
                write(*socket_fd, "room_booked", sizeof("room_booked"));
            }
        } else if (strcmp(buffer, "leave_hotel") == 0) {
            pthread_mutex_lock(&room_mutex);
            int room_id = g.room_number;
            if (room_id == 0) {
                pthread_mutex_unlock(&room_mutex);
                write(*socket_fd, "no_room_booked", sizeof("no_room_booked"));
            } else {
                rooms[room_id-1].is_free = true;
                g.room_number = 0;
                char message[1024] = {0};
                sprintf(message, "Guest %d left room %d\n", g.id, room_id);
                printf("%s", message);
                broadcast_to_spectators(message);
                pthread_mutex_unlock(&room_mutex);
                write(*socket_fd, "room_cleared", sizeof("room_cleared"));
            }
        }
    }
    free(socket);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
    if (argc == 2) {
        PORT = atoi(argv[1]);
    }
    init_rooms();
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Создание сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Hotel server is listening on port %d...\n", PORT);

    while (true) {
        int *new_socket = malloc(sizeof(int));
        if ((*new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            free(new_socket);
            exit(EXIT_FAILURE);
        }

        char buffer[1024] = {0};
        int msg_size = read(*new_socket, buffer, 1024);
        if (msg_size <= 0) {
            close(*new_socket);
            free(new_socket);
            exit(EXIT_FAILURE);
        }
        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strcmp(buffer, "guest") == 0) {
            pthread_t guest_thread;
            pthread_create(&guest_thread, NULL, handle_client, (void*)new_socket);
            pthread_detach(guest_thread);
        } else if (strcmp(buffer, "spectator") == 0) {
            if (spectator_count >= MAX_SPECTATORS) {
                char* msg = "Spectator limit reached\n";
                send(*new_socket, msg, strlen(msg), 0);
                close(*new_socket);
                free(new_socket);
                continue;
            }

            pthread_mutex_lock(&spec_mutex);
            spectator_sockets[spectator_count] = *new_socket;
            spectator_count++;
            pthread_mutex_unlock(&spec_mutex);

            pthread_t spectator_thread;
            pthread_create(&spectator_thread, NULL, handle_spectator, (void*)new_socket);
            pthread_detach(spectator_thread);
        } else {
            close(*new_socket);
            free(new_socket);
        }
    }
    close(server_fd);

    return 0;
}
