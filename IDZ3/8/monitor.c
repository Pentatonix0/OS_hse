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
		printf("Использование: %s <server_ip> <server_port>\n", argv[0]);
		return 1;
	}
	char *server_ip = argv[1];
	int server_port = atoi(argv[2]);
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("Ошибка создания сокета");
		return 1;
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Ошибка подключения");
		close(sock);
		return 1;
	}
	printf("Подключено к серверу %s:%d\n", server_ip, server_port);
	printf("Для запроса состояния сада введите 'grid'. Для отключения монитора введите 'q' или 'exit'\n");

	// Отправка идентификации монитора
	if (send(sock, "MONITOR\n", 8, 0) < 0)
	{
		perror("Ошибка отправки MONITOR");
		close(sock);
		return 1;
	}

	// Ожидание ответа OK
	char buffer[1024];
	int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
	if (n <= 0)
	{
		if (n < 0)
			perror("Ошибка получения OK");
		printf("Сервер закрыл соединение при идентификации\n");
		close(sock);
		return 1;
	}
	buffer[n] = '\0';
	if (strcmp(buffer, "OK\n") != 0)
	{
		printf("Ошибка идентификации: ожидалось OK, получено %s", buffer);
		close(sock);
		return 1;
	}

	// Настройка poll для обработки ввода с клавиатуры и сокета
	struct pollfd fds[2];
	fds[0].fd = sock; // Сокет для получения сообщений от сервера
	fds[0].events = POLLIN;
	fds[1].fd = STDIN_FILENO; // Стандартный ввод для команд пользователя
	fds[1].events = POLLIN;

	int grid_request_interval = 10; // Запрашивать сетку каждые 10 секунд
	time_t last_grid_request = time(NULL);

	while (1)
	{
		// Периодический запрос 
		time_t now = time(NULL);
		if (now - last_grid_request >= grid_request_interval)
		{
			if (send(sock, "GET_GRID\n", 9, 0) < 0)
			{
				perror("Ошибка отправки GET_GRID");
				break;
			}
			last_grid_request = now;
		}

		// Проверка событий с тайм-аутом 1000 мс
		int ret = poll(fds, 2, 1000);
		if (ret < 0)
		{
			perror("Ошибка poll");
			break;
		}

		// Обработка сообщений от сервера
		if (fds[0].revents & POLLIN)
		{
			int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
			if (n <= 0)
			{
				if (n < 0)
					perror("Ошибка получения данных");
				printf("Сервер закрыл соединение\n");
				break;
			}
			buffer[n] = '\0';
			// Выводим только таблицу сада
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
						perror("Ошибка отправки GET_GRID (ручной)");
						break;
					}
					last_grid_request = now;
				}
				else if (strcmp(input, "exit") == 0 || strcmp(input, "q") == 0)
				{
					if (send(sock, "EXIT\n", 5, 0) < 0)
					{
						perror("Ошибка отправки EXIT");
					}
					else
					{
						// fprintf(stderr, "[DEBUG] Sent: EXIT\n");
					}
					break;
				}
			}
		}
	}
	close(sock);
	printf("Монитор завершил работу\n");
	return 0;
}