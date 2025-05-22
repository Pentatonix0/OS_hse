#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define M 5
#define N 5
#define P 5

typedef struct
{
	int type;		  // 0: inaccessible, 1: accessible
	int processed_by; // 0: not processed, 1: by gardener1, 2: by gardener2
	int occupied_by;  // 0: none, 1: gardener1, 2: gardener2
} Cell;

Cell grid[M][N];
int gardener_pos[2][2];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int finished_gardeners = 0;
int monitor_sock = -1;

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

void send_to_monitor(const char *message)
{
	if (monitor_sock != -1)
	{
		time_t now = time(NULL);
		char *timestamp = ctime(&now);
		timestamp[strlen(timestamp) - 1] = '\0'; // Удаляем \n
		char timed_message[2048];
		snprintf(timed_message, sizeof(timed_message), "[%s] %s", timestamp, message);
		printf("%s", timed_message); // Выводим в консоль
		if (send(monitor_sock, timed_message, strlen(timed_message), 0) < 0)
		{
			perror("Send to monitor failed");
		}
	}
	else
	{
		// Если монитор не подключён, всё равно выводим в консоль
		time_t now = time(NULL);
		char *timestamp = ctime(&now);
		timestamp[strlen(timestamp) - 1] = '\0';
		printf("[%s] %s", timestamp, message);
	}
}

void print_grid_to_monitor_only()
{
	char grid_str[1024];
	snprintf(grid_str, sizeof(grid_str), "Итоговое состояние сада:\n  ");
	for (int j = 0; j < N; j++)
	{
		snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "%d ", j);
	}
	snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "\n");
	for (int i = 0; i < M; i++)
	{
		snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "%d ", i);
		for (int j = 0; j < N; j++)
		{
			if (grid[i][j].type == 0)
			{
				snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "X ");
			}
			else if (grid[i][j].processed_by == 0)
			{
				snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "U ");
			}
			else
			{
				snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "%d ", grid[i][j].processed_by);
			}
		}
		snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "\n");
	}
	snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str),
			 "Легенда: X - недоступно, U - необработано, 1 - обработано садовником 1, 2 - обработано садовником 2\n");

	// Отправляем только на монитор, без вывода в консоль
	if (monitor_sock != -1)
	{
		time_t now = time(NULL);
		char *timestamp = ctime(&now);
		timestamp[strlen(timestamp) - 1] = '\0';
		char timed_message[2048];
		snprintf(timed_message, sizeof(timed_message), "[%s] %s", timestamp, grid_str);
		if (send(monitor_sock, timed_message, strlen(timed_message), 0) < 0)
		{
			perror("Send to monitor failed");
		}
	}
}

void *handle_client(void *arg)
{
	int sock = *(int *)arg;
	free(arg);
	char buffer[1024];
	char msg[2048];
	int gardener_id = -1;

	while (1)
	{
		int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
		if (n <= 0)
		{
			snprintf(msg, sizeof(msg), "Клиент (ID %d) отключился\n", gardener_id);
			send_to_monitor(msg);
			break;
		}
		buffer[n] = '\0';
		printf("Received request: %s", buffer);

		char cmd[20];
		int x, y;

		if (sscanf(buffer, "ID %d", &gardener_id) == 1)
		{
			printf("Processing ID request for gardener_id: %d\n", gardener_id);
			send(sock, "OK\n", 3, 0);
			printf("Sent response: OK\n");
			snprintf(msg, sizeof(msg), "Садовник %d подключился\n", gardener_id);
			send_to_monitor(msg);
		}
		else if (sscanf(buffer, "MOVE %d %d", &x, &y) == 2)
		{
			printf("Processing MOVE request for x: %d, y: %d\n", x, y);
			pthread_mutex_lock(&mutex);
			if (x < 0 || x >= M || y < 0 || y >= N || grid[x][y].type == 0)
			{
				send(sock, "INACCESSIBLE\n", 13, 0);
				printf("Sent response: INACCESSIBLE\n");
				snprintf(msg, sizeof(msg), "Садовник %d пытался переместиться в недоступную клетку (%d, %d)\n", gardener_id, x, y);
				send_to_monitor(msg);
			}
			else
			{
				int other_gardener = 3 - gardener_id;
				if (grid[x][y].occupied_by == other_gardener)
				{
					send(sock, "WAIT\n", 5, 0);
					printf("Sent response: WAIT\n");
					snprintf(msg, sizeof(msg), "Садовник %d ждет освобождения (%d, %d)\n", gardener_id, x, y);
					send_to_monitor(msg);
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
					printf("Sent response: MOVED\n");
					snprintf(msg, sizeof(msg), "Садовник %d переместился в (%d, %d)\n", gardener_id, x, y);
					send_to_monitor(msg);
				}
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
				snprintf(response, sizeof(response), "ACCESSIBLE PROCESSED_BY %d\n", grid[x][y].processed_by);
			}
			else
			{
				strcpy(response, "ACCESSIBLE UNPROCESSED\n");
			}
			send(sock, response, strlen(response), 0);
			printf("Sent response: %s", response);
			snprintf(msg, sizeof(msg), "Садовник %d запросил состояние клетки (%d, %d)\n", gardener_id, x, y);
			send_to_monitor(msg);
			pthread_mutex_unlock(&mutex);
		}
		else if (sscanf(buffer, "OCCUPY %d %d", &x, &y) == 2)
		{
			printf("Processing OCCUPY request for x: %d, y: %d\n", x, y);
			send(sock, "OK\n", 3, 0);
			printf("Sent response: OK\n");
			snprintf(msg, sizeof(msg), "Садовник %d занял клетку (%d, %d)\n", gardener_id, x, y);
			send_to_monitor(msg);
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
			printf("Sent response: OK\n");
			snprintf(msg, sizeof(msg), "Садовник %d завершил обработку (%d, %d)\n", gardener_id, x, y);
			send_to_monitor(msg);
			pthread_mutex_unlock(&mutex);
		}
		else if (sscanf(buffer, "RELEASE %d %d", &x, &y) == 2)
		{
			printf("Processing RELEASE request for x: %d, y: %d\n", x, y);
			pthread_mutex_lock(&mutex);
			grid[x][y].occupied_by = 0;
			send(sock, "OK\n", 3, 0);
			printf("Sent response: OK\n");
			snprintf(msg, sizeof(msg), "Садовник %d освободил клетку (%d, %d)\n", gardener_id, x, y);
			send_to_monitor(msg);
			pthread_mutex_unlock(&mutex);
		}
		else if (strcmp(buffer, "FINISHED\n") == 0)
		{
			printf("Processing FINISHED request\n");
			pthread_mutex_lock(&mutex);
			finished_gardeners++;
			snprintf(msg, sizeof(msg), "Садовник %d завершил свою задачу\n", gardener_id);
			send_to_monitor(msg);
			if (finished_gardeners == 2)
			{
				print_grid_to_monitor_only();
			}
			pthread_mutex_unlock(&mutex);
			break;
		}
		else
		{
			printf("Unknown command: %s", buffer);
			snprintf(msg, sizeof(msg), "Неизвестная команда от садовника %d: %s", gardener_id, buffer);
			send_to_monitor(msg);
		}
	}
	close(sock);
	return NULL;
}

void *handle_monitor(void *arg)
{
	int sock = *(int *)arg;
	free(arg);
	monitor_sock = sock;
	char buffer[1024];
	char msg[2048];

	while (1)
	{
		int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
		if (n <= 0)
		{
			snprintf(msg, sizeof(msg), "Монитор отключился\n");
			send_to_monitor(msg);
			break;
		}
		buffer[n] = '\0';
		printf("Received request from monitor: %s", buffer);

		if (strcmp(buffer, "GET_GRID\n") == 0)
		{
			printf("Processing GET_GRID request from monitor\n");
			snprintf(msg, sizeof(msg), "Монитор запросил состояние сада\n");
			send_to_monitor(msg);
			print_grid_to_monitor_only();
		}
		else
		{
			printf("Unknown command from monitor: %s", buffer);
			snprintf(msg, sizeof(msg), "Неизвестная команда от монитора: %s", buffer);
			send_to_monitor(msg);
		}
	}
	close(sock);
	monitor_sock = -1;
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
	if (server_sock < 0)
	{
		perror("Socket creation failed");
		return 1;
	}
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Bind failed");
		close(server_sock);
		return 1;
	}
	if (listen(server_sock, 3) < 0)
	{
		perror("Listen failed");
		close(server_sock);
		return 1;
	}
	pthread_t threads[3];
	for (int i = 0; i < 2; i++)
	{
		int *client_sock = malloc(sizeof(int));
		*client_sock = accept(server_sock, NULL, NULL);
		if (*client_sock < 0)
		{
			perror("Accept client failed");
			free(client_sock);
			continue;
		}
		pthread_create(&threads[i], NULL, handle_client, client_sock);
	}
	int *monitor_sock_ptr = malloc(sizeof(int));
	*monitor_sock_ptr = accept(server_sock, NULL, NULL);
	if (*monitor_sock_ptr < 0)
	{
		perror("Accept monitor failed");
		free(monitor_sock_ptr);
	}
	else
	{
		pthread_create(&threads[2], NULL, handle_monitor, monitor_sock_ptr);
	}

	for (int i = 0; i < 3; i++)
	{
		pthread_join(threads[i], NULL);
	}
	close(server_sock);
	return 0;
}