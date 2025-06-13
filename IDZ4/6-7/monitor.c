#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>

#define BUFFER_SIZE 2048

void print_with_timestamp(const char *msg)
{
	time_t now = time(NULL);
	char *time_str = ctime(&now);
	time_str[strlen(time_str) - 1] = '\0';
	printf("[%s] %s", time_str, msg);
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
		return 1;
	}

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));
	server_addr.sin_addr.s_addr = inet_addr(argv[1]);

	// Регистрация как монитора
	char buffer[BUFFER_SIZE] = "ID 0\n";
	sendto(sock, buffer, strlen(buffer), 0,
		   (struct sockaddr *)&server_addr, sizeof(server_addr));

	printf("Монитор подключен к серверу. Команды:\n");
	printf("  grid - запросить текущее состояние сада\n");
	printf("  quit - выйти из монитора\n");

	struct pollfd fds[2] = {
		{.fd = sock, .events = POLLIN},
		{.fd = STDIN_FILENO, .events = POLLIN}};

	while (1)
	{
		int ret = poll(fds, 2, 100); // 100ms timeout
		if (ret < 0)
		{
			perror("Poll failed");
			break;
		}

		// Обработка сообщений от сервера
		if (fds[0].revents & POLLIN)
		{
			struct sockaddr_in from_addr;
			socklen_t from_len = sizeof(from_addr);
			int n = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
							 (struct sockaddr *)&from_addr, &from_len);
			if (n <= 0)
				break;
			buffer[n] = '\0';
			printf("%s", buffer);
		}

		// Обработка ввода пользователя
		if (fds[1].revents & POLLIN)
		{
			char input[100];
			if (!fgets(input, sizeof(input), stdin))
				continue;
			input[strcspn(input, "\n")] = '\0';

			if (strcmp(input, "grid") == 0)
			{
				strcpy(buffer, "GET_GRID\n");
				sendto(sock, buffer, strlen(buffer), 0,
					   (struct sockaddr *)&server_addr, sizeof(server_addr));
			}
			else if (strcmp(input, "quit") == 0 || strcmp(input, "q") == 0)
			{
				break;
			}
		}
	}

	close(sock);
	print_with_timestamp("Монитор отключен\n");
	return 0;
}