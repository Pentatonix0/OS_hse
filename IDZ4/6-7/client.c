#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <poll.h>

#define M 5
#define N 5
#define P 5
#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 5
#define MAX_RETRIES 3

void generate_sequence(int id, int sequence[M * N][2])
{
	int index = 0;

	if (id == 1)
	{
		for (int i = 0; i < M; i++)
		{
			if (i % 2 == 0)
			{
				for (int j = 0; j < N; j++)
				{
					sequence[index][0] = i;
					sequence[index][1] = j;
					index++;
				}
			}
			else
			{
				for (int j = N - 1; j >= 0; j--)
				{
					sequence[index][0] = i;
					sequence[index][1] = j;
					index++;
				}
			}
		}
	}
	else
	{
		for (int j = N - 1; j >= 0; j--)
		{
			if ((N - 1 - j) % 2 == 0)
			{
				for (int i = M - 1; i >= 0; i--)
				{
					sequence[index][0] = i;
					sequence[index][1] = j;
					index++;
				}
			}
			else
			{
				for (int i = 0; i < M; i++)
				{
					sequence[index][0] = i;
					sequence[index][1] = j;
					index++;
				}
			}
		}
	}
}

int send_receive(int sock, struct sockaddr_in *server_addr,
				 char *send_msg, char *recv_buffer)
{
	socklen_t addr_len = sizeof(*server_addr);
	int retries = 0;

	while (retries < MAX_RETRIES)
	{
		sendto(sock, send_msg, strlen(send_msg), 0,
			   (struct sockaddr *)server_addr, addr_len);

		struct pollfd fd = {
			.fd = sock,
			.events = POLLIN};

		int ready = poll(&fd, 1, TIMEOUT_SEC * 1000);
		if (ready > 0)
		{
			int n = recvfrom(sock, recv_buffer, BUFFER_SIZE - 1, 0,
							 (struct sockaddr *)server_addr, &addr_len);
			if (n > 0)
			{
				recv_buffer[n] = '\0';
				return n;
			}
		}

		retries++;
		printf("Таймаут, повторная попытка %d/%d\n", retries, MAX_RETRIES);
	}

	return -1;
}

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		printf("Usage: %s <server_ip> <server_port> <id>\n", argv[0]);
		return 1;
	}

	char *server_ip = argv[1];
	int server_port = atoi(argv[2]);
	int id = atoi(argv[3]);

	if (id != 1 && id != 2)
	{
		printf("ID садовника должен быть 1 или 2\n");
		return 1;
	}

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		perror("Socket creation failed");
		return 1;
	}

	struct sockaddr_in server_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(server_port),
		.sin_addr.s_addr = inet_addr(server_ip)};

	printf("Садовник %d запущен, подключение к серверу %s:%d\n", id, server_ip, server_port);

	char buffer[BUFFER_SIZE];
	snprintf(buffer, sizeof(buffer), "ID %d\n", id);

	if (send_receive(sock, &server_addr, buffer, buffer) <= 0)
	{
		printf("Ошибка регистрации на сервере\n");
		close(sock);
		return 1;
	}

	if (strcmp(buffer, "OK\n") != 0)
	{
		printf("Ошибка регистрации: %s", buffer);
		close(sock);
		return 1;
	}

	int sequence[M * N][2];
	generate_sequence(id, sequence);

	for (int k = 0; k < M * N; k++)
	{
		int x = sequence[k][0];
		int y = sequence[k][1];

		printf("Садовник %d пытается переместиться в (%d, %d)\n", id, x, y);

		while (1)
		{
			snprintf(buffer, sizeof(buffer), "MOVE %d %d\n", x, y);
			if (send_receive(sock, &server_addr, buffer, buffer) <= 0)
			{
				printf("Ошибка связи с сервером\n");
				close(sock);
				return 1;
			}

			if (strcmp(buffer, "MOVED\n") == 0)
			{
				printf("Садовник %d переместился в (%d, %d)\n", id, x, y);
				break;
			}
			else if (strcmp(buffer, "WAIT\n") == 0)
			{
				printf("Садовник %d ждет освобождения клетки (%d, %d)\n", id, x, y);
				sleep(1);
			}
			else if (strcmp(buffer, "INACCESSIBLE\n") == 0)
			{
				printf("Клетка (%d, %d) недоступна (препятствие)\n", x, y);
				break;
			}
		}

		snprintf(buffer, sizeof(buffer), "GET_STATE %d %d\n", x, y);
		if (send_receive(sock, &server_addr, buffer, buffer) > 0)
		{
			if (strstr(buffer, "UNPROCESSED"))
			{
				snprintf(buffer, sizeof(buffer), "OCCUPY %d %d\n", x, y);
				if (send_receive(sock, &server_addr, buffer, buffer) > 0 && strcmp(buffer, "OK\n") == 0)
				{
					printf("Садовник %d занял клетку (%d, %d)\n", id, x, y);
					sleep(P); // Симуляция обработки
					snprintf(buffer, sizeof(buffer), "FINISH_PROCESSING %d %d\n", x, y);
					if (send_receive(sock, &server_addr, buffer, buffer) > 0 && strcmp(buffer, "OK\n") == 0)
					{
						printf("Садовник %d завершил обработку (%d, %d)\n", id, x, y);
					}
				}
			}
			snprintf(buffer, sizeof(buffer), "RELEASE %d %d\n", x, y);
			if (send_receive(sock, &server_addr, buffer, buffer) > 0 && strcmp(buffer, "OK\n") == 0)
			{
				printf("Садовник %d освободил клетку (%d, %d)\n", id, x, y);
			}
		}
	}

	snprintf(buffer, sizeof(buffer), "FINISHED\n");
	sendto(sock, buffer, strlen(buffer), 0,
		   (struct sockaddr *)&server_addr, sizeof(server_addr));

	close(sock);
	printf("Садовник %d завершил свою работу\n", id);
	return 0;
}