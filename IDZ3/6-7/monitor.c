#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <poll.h>

void print_with_timestamp(const char *message)
{
	time_t now = time(NULL);
	char *timestamp = ctime(&now);
	timestamp[strlen(timestamp) - 1] = '\0'; // Удаляем \n
	printf("[%s] %s", timestamp, message);
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
		return 1;
	}
	char *server_ip = argv[1];
	int server_port = atoi(argv[2]);
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("Socket creation failed");
		return 1;
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Connect failed");
		close(sock);
		return 1;
	}
	printf("Подключено к серверу %s:%d\n", server_ip, server_port);
	printf("Для запроса состояния сада введите 'grid'. Для отключения монитора введите 'q' или 'exit'\n");

	// Настройка poll для обработки ввода с клавиатуры и сокета
	struct pollfd fds[2];
	fds[0].fd = sock; // Сокет для получения сообщений от сервера
	fds[0].events = POLLIN;
	fds[1].fd = STDIN_FILENO; // Стандартный ввод для команд пользователя
	fds[1].events = POLLIN;

	char buffer[1024];
	int grid_request_interval = 10; // Запрашивать сетку каждые 10 секунд
	time_t last_grid_request = time(NULL);

	while (1)
	{

		time_t now = time(NULL);
		if (now - last_grid_request >= grid_request_interval)
		{
			if (send(sock, "GET_GRID\n", 9, 0) < 0)
			{
				perror("Send GET_GRID failed");
				break;
			}
			fprintf(stderr, "[DEBUG] Sent GET_GRID\n");
			last_grid_request = now;
		}

		// Проверка событий с тайм-аутом 100 мс
		int ret = poll(fds, 2, 100);
		if (ret < 0)
		{
			perror("Poll failed");
			break;
		}

		// Обработка сообщений от сервера
		if (fds[0].revents & POLLIN)
		{
			int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
			if (n <= 0)
			{
				printf("Сервер закрыл соединение\n");
				break;
			}
			buffer[n] = '\0';

			if (strstr(buffer, "Итоговое состояние сада:") != NULL)
			{
				print_with_timestamp(buffer);
			}
		}

		// Обработка ввода пользователя
		if (fds[1].revents & POLLIN)
		{
			char input[100];
			if (fgets(input, sizeof(input), stdin))
			{
				input[strcspn(input, "\n")] = '\0';
				if (strcmp(input, "grid") == 0)
				{
					if (send(sock, "GET_GRID\n", 9, 0) < 0)
					{
						perror("Send GET_GRID failed");
						break;
					}
					fprintf(stderr, "[DEBUG] Sent GET_GRID (manual)\n");
					last_grid_request = now;
				}
				else if (strcmp(input, "exit") == 0 || strcmp(input, "q") == 0)
				{
					break;
				}
			}
		}
	}
	close(sock);
	printf("Монитор завершил работу\n");
	return 0;
}
