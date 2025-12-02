/*
 * Пример программы демонстрирует обмен сообщениями между двумя процессами по TCP-соединению.
 * TCP-сокеты позволяют установить надежное соединение между процессами. В этом примере
 * процессы A и B моделируют состояния "ready" и "sleep". Процесс A начинает в состоянии READY,
 * выполняет некоторую работу (имитация задержкой), затем отправляет сообщение B и переходит
 * в состояние SLEEP. Процесс B, ожидая сообщения (SLEEP), получает его, переходит в READY,
 * выполняет свою работу, отправляет сообщение процессу A и снова переходит в SLEEP.
 * Так происходит механизм "пинг-понг" обмена сообщениями, когда процессы поочередно
 * передают управление друг другу через TCP-соединение. Процесс A выступает в роли сервера,
 * прослушивая порт и принимая соединение от процесса B (клиента), после чего начинается
 * двусторонний обмен данными.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>

#define PORT 12345
#define MAX_ITERATIONS 10

// Функция для ожидания сообщения (процесс переходит в состояние SLEEP, затем в READY после получения)
void wait_for_message(int sock, const char *process_name)
{
    char buffer[16];
    printf("[%s] Переход в состояние SLEEP (ожидание сообщения)...\n", process_name);
    fflush(stdout);
    if (recv(sock, buffer, sizeof(buffer), 0) <= 0)
    {
        perror("recv");
        exit(1);
    }
    printf("[%s] Переход в состояние READY (сообщение получено)\n", process_name);
    fflush(stdout);
}

// Функция для отправки сообщения (процесс передает управление другому)
void send_message(int sock, const char *process_name, const char *msg)
{
    printf("[%s] Отправка сообщения синхронизации...\n", process_name);
    fflush(stdout);
    if (send(sock, msg, strlen(msg) + 1, 0) == -1)
    {
        perror("send");
        exit(1);
    }
}

// Код процесса A (сервер)
void process_a(int conn_fd)
{
    printf("[Process A] Начальное состояние: READY\n");
    fflush(stdout);
    for (int i = 0; i < MAX_ITERATIONS; i++)
    {
        printf("\n--- Итерация %d (Process A) ---\n", i + 1);
        fflush(stdout);
        // Имитация работы в состоянии READY
        sleep(1);
        // Отправляем сообщение синхронизации процессу B
        send_message(conn_fd, "Process A", "PING");
        // Переходим в состояние SLEEP и ждем ответа
        wait_for_message(conn_fd, "Process A");
    }
    close(conn_fd);
}

// Код процесса B (клиент)
void process_b(int sock_fd)
{
    printf("[Process B] Начальное состояние: SLEEP\n");
    fflush(stdout);
    for (int i = 0; i < MAX_ITERATIONS; i++)
    {
        // Ожидаем сообщение от процесса A
        wait_for_message(sock_fd, "Process B");
        printf("\n--- Итерация %d (Process B) ---\n", i + 1);
        fflush(stdout);
        // Имитация работы в состоянии READY
        sleep(1);
        // Отправляем сообщение синхронизации процессу A
        send_message(sock_fd, "Process B", "PONG");
    }
    close(sock_fd);
}

int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Создаем TCP-сокет
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Настраиваем адрес сокета
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Привязываем сокет к адресу и порту
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    // Начинаем прослушивание порта
    if (listen(server_fd, 1) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("=== Запуск TCP-пингпонг ===\n\n");
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        // Дочерний процесс - Process B (клиент)
        int sock = 0;
        struct sockaddr_in serv_addr;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("socket");
            exit(EXIT_FAILURE);
        }
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
        {
            perror("inet_pton");
            exit(EXIT_FAILURE);
        }
        // Подключаемся к серверу
        sleep(1); // небольшая задержка для гарантии, что сервер готов
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("connect");
            exit(EXIT_FAILURE);
        }
        process_b(sock);
        exit(0);
    }
    else
    {
        // Родительский процесс - Process A (сервер)
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                 (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        process_a(new_socket);
        // Ждем завершения процесса B
        wait(NULL);
        printf("\n=== TCP-пингпонг завершен ===\n");
        fflush(stdout);
    }
    return 0;
}
