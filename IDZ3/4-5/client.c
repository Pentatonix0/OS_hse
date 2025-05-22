#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define M 5
#define N 5
#define P 5

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
	else if (id == 2)
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
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
	connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

	char buffer[1024];
	sprintf(buffer, "ID %d\n", id);
	send(sock, buffer, strlen(buffer), 0);
	recv(sock, buffer, sizeof(buffer), 0);

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
			send(sock, buffer, strlen(buffer), 0);
			int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
			buffer[n] = '\0';
			if (strcmp(buffer, "MOVED\n") == 0)
			{
				printf("Садовник %d переместился в (%d, %d)\n", id, x, y);
				break;
			}
			else if (strcmp(buffer, "WAIT\n") == 0)
			{
				printf("Садовник %d ждет освобождения клетки (%d, %d)\n", id, x, y);
				usleep(100000);
			}
		}
		sprintf(buffer, "GET_STATE %d %d\n", x, y);
		send(sock, buffer, strlen(buffer), 0);
		int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
		buffer[n] = '\0';
		int T;
		if (strstr(buffer, "INACCESSIBLE"))
		{
			printf("Клетка (%d, %d) недоступна\n", x, y);
			T = 1;
		}
		else if (strstr(buffer, "PROCESSED_BY"))
		{
			printf("Клетка (%d, %d) доступна и обработана\n", x, y);
			T = 1;
		}
		else if (strstr(buffer, "UNPROCESSED"))
		{
			printf("Клетка (%d, %d) доступна и необработана\n", x, y);
			T = P;
		}
		else
		{
			printf("Неизвестный ответ от сервера: %s", buffer);
			T = 1;
		}
		sprintf(buffer, "OCCUPY %d %d\n", x, y);
		send(sock, buffer, strlen(buffer), 0);
		recv(sock, buffer, sizeof(buffer), 0);
		printf("Садовник %d занял клетку (%d, %d)\n", id, x, y);
		if (T == P)
		{
			printf("Садовник %d начал обработку клетки (%d, %d)\n", id, x, y);
		}
		sleep(T);
		if (T == P)
		{
			sprintf(buffer, "FINISH_PROCESSING %d %d\n", x, y);
			send(sock, buffer, strlen(buffer), 0);
			recv(sock, buffer, sizeof(buffer), 0);
			printf("Садовник %d завершил обработку клетки (%d, %d)\n", id, x, y);
		}
		sprintf(buffer, "RELEASE %d %d\n", x, y);
		send(sock, buffer, strlen(buffer), 0);
		recv(sock, buffer, sizeof(buffer), 0);
		printf("Садовник %d освободил клетку (%d, %d)\n", id, x, y);
	}
	send(sock, "FINISHED\n", 9, 0);
	close(sock);
	printf("Садовник %d завершил свою задачу\n", id);
	return 0;
}