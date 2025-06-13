#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define M 10
#define N 10
#define PROCESS_TIME 2
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

int send_receive(int sockfd, struct sockaddr_in *server_addr,
				 char *send_msg, char *recv_buffer, int expected_len)
{
	socklen_t addr_len = sizeof(*server_addr);
	int retries = 0;

	while (retries < MAX_RETRIES)
	{
		sendto(sockfd, send_msg, strlen(send_msg), 0,
			   (struct sockaddr *)server_addr, addr_len);

		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(sockfd, &read_fds);

		struct timeval timeout;
		timeout.tv_sec = TIMEOUT_SEC;
		timeout.tv_usec = 0;

		int ready = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
		if (ready > 0)
		{
			int n = recvfrom(sockfd, recv_buffer, BUFFER_SIZE - 1, 0,
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
		printf("Использование: %s <IP сервера> <порт> <ID садовника (1 или 2)>\n", argv[0]);
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

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		perror("Ошибка создания сокета");
		return 1;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);

	if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
	{
		perror("Неверный адрес сервера");
		close(sockfd);
		return 1;
	}

	printf("Садовник %d запущен, подключение к серверу %s:%d\n", id, server_ip, server_port);

	char buffer[BUFFER_SIZE];
	sprintf(buffer, "ID %d\n", id);

	if (send_receive(sockfd, &server_addr, buffer, buffer, BUFFER_SIZE) <= 0)
	{
		printf("Ошибка регистрации на сервере\n");
		close(sockfd);
		return 1;
	}

	if (strcmp(buffer, "OK\n") != 0)
	{
		printf("Ошибка регистрации: %s", buffer);
		close(sockfd);
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
			sprintf(buffer, "MOVE %d %d\n", x, y);
			if (send_receive(sockfd, &server_addr, buffer, buffer, BUFFER_SIZE) <= 0)
			{
				printf("Ошибка связи с сервером\n");
				close(sockfd);
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
			else if (strcmp(buffer, "INVALID\n") == 0)
			{
				printf("Садовник %d: неверные координаты (%d, %d)\n", id, x, y);
				break;
			}
		}

		sprintf(buffer, "GET_STATE %d %d\n", x, y);
		if (send_receive(sockfd, &server_addr, buffer, buffer, BUFFER_SIZE) <= 0)
		{
			printf("Ошибка связи с сервером\n");
			close(sockfd);
			return 1;
		}

		if (strstr(buffer, "INACCESSIBLE"))
		{
			printf("Клетка (%d, %d) недоступна (препятствие)\n", x, y);
		}
		else if (strstr(buffer, "PROCESSED_BY"))
		{
			int processed_by;
			sscanf(buffer, "PROCESSED_BY %d", &processed_by);
			printf("Клетка (%d, %d) уже обработана садовником %d\n", x, y, processed_by);
		}
		else if (strstr(buffer, "UNPROCESSED"))
		{
			printf("Клетка (%d, %d) доступна и не обработана\n", x, y);

			sprintf(buffer, "OCCUPY %d %d\n", x, y);
			if (send_receive(sockfd, &server_addr, buffer, buffer, BUFFER_SIZE) <= 0)
			{
				printf("Ошибка связи с сервером\n");
				close(sockfd);
				return 1;
			}

			printf("Садовник %d начал обработку клетки (%d, %d)\n", id, x, y);
			sleep(PROCESS_TIME);

			sprintf(buffer, "FINISH_PROCESSING %d %d\n", x, y);
			if (send_receive(sockfd, &server_addr, buffer, buffer, BUFFER_SIZE) <= 0)
			{
				printf("Ошибка связи с сервером\n");
				close(sockfd);
				return 1;
			}

			printf("Садовник %d завершил обработку клетки (%d, %d)\n", id, x, y);
		}

		sprintf(buffer, "RELEASE %d %d\n", x, y);
		if (send_receive(sockfd, &server_addr, buffer, buffer, BUFFER_SIZE) <= 0)
		{
			printf("Ошибка связи с сервером\n");
			close(sockfd);
			return 1;
		}

		printf("Садовник %d освободил клетку (%d, %d)\n", id, x, y);
	}

	sprintf(buffer, "FINISHED\n");
	sendto(sockfd, buffer, strlen(buffer), 0,
		   (struct sockaddr *)&server_addr, sizeof(server_addr));

	close(sockfd);
	printf("Садовник %d завершил свою работу\n", id);
	return 0;
}