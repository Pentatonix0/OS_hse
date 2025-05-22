#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define M 5
#define N 5
#define P 5

typedef struct
{
	int type;		  // 0: inaccessible, 1: accessible
	int processed_by; // 0: not processed, 1: by gardener1, 2: by gardener2
	int occupied_by;  // 0: none, 1: gardener1, 2: by gardener2
} Cell;

Cell grid[M][N];
int gardener_pos[2][2];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int finished_gardeners = 0;

void initialize_grid()
{
	int total_cells = M * N;
	int inaccessible_percent = 10 + rand() % 21;
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
	gardener_pos[0][0] = -1;
	gardener_pos[0][1] = -1;
	gardener_pos[1][0] = -1;
	gardener_pos[1][1] = -1;
}

void print_grid()
{
	printf("Итоговое состояние сада:\n");
	printf("  ");
	for (int j = 0; j < N; j++)
	{
		printf("%d ", j);
	}
	printf("\n");
	for (int i = 0; i < M; i++)
	{
		printf("%d ", i);
		for (int j = 0; j < N; j++)
		{
			if (grid[i][j].type == 0)
			{
				printf("X ");
			}
			else if (grid[i][j].processed_by == 0)
			{
				printf("U ");
			}
			else
			{
				printf("%d ", grid[i][j].processed_by);
			}
		}
		printf("\n");
	}
	printf("Легенда: X - недоступно, U - необработано, 1 - обработано садовником 1, 2 - обработано садовником 2\n");
}

void *handle_client(void *arg)
{
	int sock = *(int *)arg;
	free(arg);
	char buffer[1024];
	int gardener_id = -1;

	while (1)
	{
		int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
		if (n <= 0)
			break;
		buffer[n] = '\0';
		char cmd[20];
		int x, y;

		// Логирование полученного запроса
		printf("Received request: %s", buffer);

		if (sscanf(buffer, "ID %d", &gardener_id) == 1)
		{
			printf("Processing ID request for gardener_id: %d\n", gardener_id);
			send(sock, "OK\n", 3, 0);
		}
		else if (sscanf(buffer, "MOVE %d %d", &x, &y) == 2)
		{
			printf("Processing MOVE request for x: %d, y: %d\n", x, y);
			pthread_mutex_lock(&mutex);
			int other_gardener = 3 - gardener_id;
			if (grid[x][y].occupied_by == other_gardener)
			{
				send(sock, "WAIT\n", 5, 0);
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
				send(sock, "MOVED\n", 6, 0);
			}
			pthread_mutex_unlock(&mutex);
		}
		else if (sscanf(buffer, "GET_STATE %d %d", &x, &y) == 2)
		{
			printf("Processing GET_STATE request for x: %d, y: %d\n", x, y);
			pthread_mutex_lock(&mutex);
			char response[50];
			if (grid[x][y].type == 0)
			{
				strcpy(response, "INACCESSIBLE\n");
			}
			else if (grid[x][y].processed_by != 0)
			{
				sprintf(response, "ACCESSIBLE PROCESSED_BY %d\n", grid[x][y].processed_by);
			}
			else
			{
				strcpy(response, "ACCESSIBLE UNPROCESSED\n");
			}
			send(sock, response, strlen(response), 0);
			pthread_mutex_unlock(&mutex);
		}
		else if (sscanf(buffer, "OCCUPY %d %d", &x, &y) == 2)
		{
			printf("Processing OCCUPY request for x: %d, y: %d\n", x, y);
			send(sock, "OK\n", 3, 0);
		}
		else if (sscanf(buffer, "FINISH_PROCESSING %d %d", &x, &y) == 2)
		{
			printf("Processing FINISH_PROCESSING request for x: %d, y: %d\n", x, y);
			pthread_mutex_lock(&mutex);
			if (grid[x][y].processed_by == 0)
			{
				grid[x][y].processed_by = gardener_id;
			}
			send(sock, "OK\n", 3, 0);
			pthread_mutex_unlock(&mutex);
		}
		else if (sscanf(buffer, "RELEASE %d %d", &x, &y) == 2)
		{
			printf("Processing RELEASE request for x: %d, y: %d\n", x, y);
			pthread_mutex_lock(&mutex);
			grid[x][y].occupied_by = 0;
			send(sock, "OK\n", 3, 0);
			pthread_mutex_unlock(&mutex);
		}
		else if (strcmp(buffer, "FINISHED\n") == 0)
		{
			printf("Processing FINISHED request\n");
			pthread_mutex_lock(&mutex);
			finished_gardeners++;
			pthread_mutex_unlock(&mutex);
			break;
		}
		else
		{
			printf("Unknown command: %s", buffer);
		}
	}
	close(sock);
	return NULL;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: %s <port>\n", argv[0]);
		return 1;
	}
	int port = atoi(argv[1]);
	srand(time(NULL));
	initialize_grid();

	int server_sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	listen(server_sock, 2);

	pthread_t threads[2];
	for (int i = 0; i < 2; i++)
	{
		int *client_sock = malloc(sizeof(int));
		*client_sock = accept(server_sock, NULL, NULL);
		pthread_create(&threads[i], NULL, handle_client, client_sock);
	}
	for (int i = 0; i < 2; i++)
	{
		pthread_join(threads[i], NULL);
	}
	print_grid();
	close(server_sock);
	return 0;
}