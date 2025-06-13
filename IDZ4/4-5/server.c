#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define M 10
#define N 10
#define MIN_OBSTACLES 10
#define MAX_OBSTACLES 30
#define BUFFER_SIZE 1024

typedef struct
{
	int type;
	int processed_by;
	int occupied_by;
} Cell;

typedef struct
{
	struct sockaddr_in addr;
	int gardener_id;
} ClientInfo;

Cell grid[M][N];
int gardener_pos[2][2];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int finished_gardeners = 0;
ClientInfo clients[2];

void initialize_grid()
{
	int total_cells = M * N;
	int inaccessible_percent = MIN_OBSTACLES + rand() % (MAX_OBSTACLES - MIN_OBSTACLES + 1);
	int inaccessible_count = total_cells * inaccessible_percent / 100;

	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			grid[i][j].type = 1;
			grid[i][j].processed_by = 0;
			grid[i][j].occupied_by = 0;
		}
	}

	for (int k = 0; k < inaccessible_count; k++)
	{
		int x = rand() % M;
		int y = rand() % N;
		if (grid[x][y].type == 0)
			k--;
		else
			grid[x][y].type = 0;
	}

	gardener_pos[0][0] = gardener_pos[0][1] = -1;
	gardener_pos[1][0] = gardener_pos[1][1] = -1;
}

void print_grid()
{
	printf("\nТекущее состояние сада:\n");
	printf("   ");
	for (int j = 0; j < N; j++)
		printf("%2d ", j);
	printf("\n");

	for (int i = 0; i < M; i++)
	{
		printf("%2d ", i);
		for (int j = 0; j < N; j++)
		{
			if (grid[i][j].type == 0)
				printf(" X ");
			else if (grid[i][j].occupied_by == 1)
				printf(" 1 ");
			else if (grid[i][j].occupied_by == 2)
				printf(" 2 ");
			else if (grid[i][j].processed_by == 1)
				printf(" . ");
			else if (grid[i][j].processed_by == 2)
				printf(" * ");
			else
				printf(" U ");
		}
		printf("\n");
	}

	printf("\nЛегенда:\n");
	printf(" X - препятствие\n U - необработанная клетка\n");
	printf(" 1/2 - садовник на клетке\n ./* - обработано садовником\n");
}

void handle_message(char *buffer, struct sockaddr_in *client_addr, int sockfd)
{
	char cmd[20];
	int x, y, gardener_id;
	char response[BUFFER_SIZE];

	if (sscanf(buffer, "ID %d", &gardener_id) == 1)
	{
		pthread_mutex_lock(&mutex);
		if (gardener_id == 1 || gardener_id == 2)
		{
			clients[gardener_id - 1].addr = *client_addr;
			clients[gardener_id - 1].gardener_id = gardener_id;
			printf("Садовник %d подключился\n", gardener_id);
			strcpy(response, "OK\n");
		}
		else
		{
			strcpy(response, "INVALID_ID\n");
		}
		pthread_mutex_unlock(&mutex);
		sendto(sockfd, response, strlen(response), 0,
			   (struct sockaddr *)client_addr, sizeof(*client_addr));
	}
	else if (sscanf(buffer, "MOVE %d %d", &x, &y) == 2)
	{
		pthread_mutex_lock(&mutex);
		int found = 0;
		for (int i = 0; i < 2; i++)
		{
			if (memcmp(client_addr, &clients[i].addr, sizeof(*client_addr)))
			{
				gardener_id = clients[i].gardener_id;
				found = 1;
				break;
			}
		}

		if (!found)
		{
			strcpy(response, "NOT_REGISTERED\n");
		}
		else if (x < 0 || x >= M || y < 0 || y >= N)
		{
			strcpy(response, "INVALID\n");
		}
		else if (grid[x][y].occupied_by == 3 - gardener_id)
		{
			strcpy(response, "WAIT\n");
		}
		else
		{
			int prev_x = gardener_pos[gardener_id - 1][0];
			int prev_y = gardener_pos[gardener_id - 1][1];
			if (prev_x != -1 && prev_y != -1)
			{
				grid[prev_x][prev_y].occupied_by = 0;
			}
			grid[x][y].occupied_by = gardener_id;
			gardener_pos[gardener_id - 1][0] = x;
			gardener_pos[gardener_id - 1][1] = y;
			strcpy(response, "MOVED\n");
			print_grid();
		}
		pthread_mutex_unlock(&mutex);
		sendto(sockfd, response, strlen(response), 0,
			   (struct sockaddr *)client_addr, sizeof(*client_addr));
	}
	else if (sscanf(buffer, "GET_STATE %d %d", &x, &y) == 2)
	{
		pthread_mutex_lock(&mutex);
		if (x < 0 || x >= M || y < 0 || y >= N)
		{
			strcpy(response, "INVALID\n");
		}
		else if (grid[x][y].type == 0)
		{
			strcpy(response, "INACCESSIBLE\n");
		}
		else if (grid[x][y].processed_by != 0)
		{
			sprintf(response, "PROCESSED_BY %d\n", grid[x][y].processed_by);
		}
		else
		{
			strcpy(response, "UNPROCESSED\n");
		}
		pthread_mutex_unlock(&mutex);
		sendto(sockfd, response, strlen(response), 0,
			   (struct sockaddr *)client_addr, sizeof(*client_addr));
	}
	else if (sscanf(buffer, "OCCUPY %d %d", &x, &y) == 2)
	{
		pthread_mutex_lock(&mutex);
		int gardener_id = -1;
		for (int i = 0; i < 2; i++)
		{
			if (memcmp(client_addr, &clients[i].addr, sizeof(*client_addr)))
			{
				gardener_id = clients[i].gardener_id;
				break;
			}
		}

		if (x < 0 || x >= M || y < 0 || y >= N)
		{
			strcpy(response, "INVALID\n");
		}
		else if (gardener_id != -1)
		{
			grid[x][y].occupied_by = gardener_id;
			strcpy(response, "OK\n");
			print_grid();
		}
		else
		{
			strcpy(response, "NOT_REGISTERED\n");
		}
		pthread_mutex_unlock(&mutex);
		sendto(sockfd, response, strlen(response), 0,
			   (struct sockaddr *)client_addr, sizeof(*client_addr));
	}
	else if (sscanf(buffer, "FINISH_PROCESSING %d %d", &x, &y) == 2)
	{
		pthread_mutex_lock(&mutex);
		int gardener_id = -1;
		for (int i = 0; i < 2; i++)
		{
			if (memcmp(client_addr, &clients[i].addr, sizeof(*client_addr)))
			{
				gardener_id = clients[i].gardener_id;
				break;
			}
		}

		if (x >= 0 && x < M && y >= 0 && y < N && gardener_id != -1 &&
			grid[x][y].processed_by == 0)
		{
			grid[x][y].processed_by = gardener_id;
		}
		strcpy(response, "OK\n");
		print_grid();
		pthread_mutex_unlock(&mutex);
		sendto(sockfd, response, strlen(response), 0,
			   (struct sockaddr *)client_addr, sizeof(*client_addr));
	}
	else if (sscanf(buffer, "RELEASE %d %d", &x, &y) == 2)
	{
		pthread_mutex_lock(&mutex);
		if (x >= 0 && x < M && y >= 0 && y < N)
		{
			grid[x][y].occupied_by = 0;
		}
		strcpy(response, "OK\n");
		print_grid();
		pthread_mutex_unlock(&mutex);
		sendto(sockfd, response, strlen(response), 0,
			   (struct sockaddr *)client_addr, sizeof(*client_addr));
	}
	else if (strcmp(buffer, "FINISHED\n") == 0)
	{
		pthread_mutex_lock(&mutex);
		finished_gardeners++;
		int gardener_id = -1;
		for (int i = 0; i < 2; i++)
		{
			if (memcmp(client_addr, &clients[i].addr, sizeof(*client_addr)))
			{
				gardener_id = clients[i].gardener_id;
				break;
			}
		}
		if (gardener_id != -1)
		{
			printf("Садовник %d завершил работу\n", gardener_id);
		}
		pthread_mutex_unlock(&mutex);
	}
	else
	{
		printf("Неизвестная команда: %s", buffer);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Использование: %s <порт>\n", argv[0]);
		return 1;
	}

	int port = atoi(argv[1]);
	srand(time(NULL));
	initialize_grid();

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
	{
		perror("Ошибка создания сокета");
		return 1;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
	{
		perror("Ошибка привязки сокета");
		close(sockfd);
		return 1;
	}

	printf("Сервер UDP запущен на порту %d. Ожидание подключения садовников...\n", port);
	print_grid();

	char buffer[BUFFER_SIZE];
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);

	while (finished_gardeners < 2)
	{
		int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
						 (struct sockaddr *)&client_addr, &addr_len);
		if (n <= 0)
			continue;
		buffer[n] = '\0';

		handle_message(buffer, &client_addr, sockfd);
	}

	printf("\nОба садовника завершили работу. Итоговое состояние сада:\n");
	print_grid();

	close(sockfd);
	return 0;
}