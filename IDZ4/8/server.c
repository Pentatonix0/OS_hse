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
#define BUFFER_SIZE 2048
#define MAX_CLIENTS 10
#define MAX_RETRIES 3

typedef struct
{
	int type;		  // 0: недоступно, 1: доступно
	int processed_by; // 0: не обработано, 1: садовник1, 2: садовник2
	int occupied_by;  // 0: никем, 1: садовник1, 2: садовник2
} Cell;

typedef struct
{
	struct sockaddr_in addr;
	int gardener_id; // 1 или 2 для садовников, 0 для монитора
	time_t last_active;
	int active; // Флаг активности клиента
} ClientInfo;

Cell grid[M][N];
int gardener_pos[2][2];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int finished_gardeners = 0;
ClientInfo clients[MAX_CLIENTS];
int num_clients = 0;

// Прототипы функций
void initialize_grid();
int find_client(struct sockaddr_in *addr);
void send_to_all_monitors(int sock, const char *message, struct sockaddr_in *client_addr);
void send_grid_to_all_monitors(int sock);
void send_to_monitor(int sock, struct sockaddr_in *monitor_addr, const char *message);
void send_grid(int sock, struct sockaddr_in *addr);
void handle_message(int sock, char *buffer, struct sockaddr_in *client_addr);
void cleanup_clients();

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

	gardener_pos[0][0] = gardener_pos[0][1] = -1;
	gardener_pos[1][0] = gardener_pos[1][1] = -1;
}

int find_client(struct sockaddr_in *addr)
{
	for (int i = 0; i < num_clients; i++)
	{
		if (clients[i].active && memcmp(&clients[i].addr.sin_addr, &addr->sin_addr, sizeof(addr->sin_addr)) == 0 &&
			clients[i].addr.sin_port == addr->sin_port)
		{
			return i;
		}
	}
	return -1;
}

void send_to_all_monitors(int sock, const char *message, struct sockaddr_in *client_addr)
{
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < num_clients; i++)
	{
		if (clients[i].active && clients[i].gardener_id == 0)
		{
			send_to_monitor(sock, &clients[i].addr, message);
		}
	}
	pthread_mutex_unlock(&mutex);
}

void send_grid_to_all_monitors(int sock)
{
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < num_clients; i++)
	{
		if (clients[i].active && clients[i].gardener_id == 0)
		{
			send_grid(sock, &clients[i].addr);
		}
	}
	pthread_mutex_unlock(&mutex);
}

void send_to_monitor(int sock, struct sockaddr_in *monitor_addr, const char *message)
{
	if (monitor_addr != NULL)
	{
		time_t now = time(NULL);
		char *timestamp = ctime(&now);
		timestamp[strlen(timestamp) - 1] = '\0';
		char full_msg[BUFFER_SIZE];
		snprintf(full_msg, sizeof(full_msg), "[%s] %s", timestamp, message);
		for (int i = 0; i < MAX_RETRIES; i++)
		{
			if (sendto(sock, full_msg, strlen(full_msg), 0, (struct sockaddr *)monitor_addr, sizeof(*monitor_addr)) >= 0)
				break;
			perror("Ошибка отправки монитору, повторная попытка");
			sleep(1);
		}
	}
}

void send_grid(int sock, struct sockaddr_in *addr)
{
	char grid_str[BUFFER_SIZE - 64] = "Текущее состояние сада:\n  ";

	for (int j = 0; j < N; j++)
	{
		snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "%2d ", j);
	}
	strcat(grid_str, "\n");

	for (int i = 0; i < M; i++)
	{
		snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "%2d ", i);
		for (int j = 0; j < N; j++)
		{
			if (grid[i][j].type == 0)
			{
				strcat(grid_str, " X ");
			}
			else if (grid[i][j].occupied_by == 1)
			{
				strcat(grid_str, " 1 ");
			}
			else if (grid[i][j].occupied_by == 2)
			{
				strcat(grid_str, " 2 ");
			}
			else if (grid[i][j].processed_by == 1)
			{
				strcat(grid_str, " . ");
			}
			else if (grid[i][j].processed_by == 2)
			{
				strcat(grid_str, " * ");
			}
			else
			{
				strcat(grid_str, " U ");
			}
		}
		strcat(grid_str, "\n");
	}

	strcat(grid_str, "Легенда: X-препятствие, U-необработано, 1/2-садовник, ./*-обработано\n");

	time_t now = time(NULL);
	char *timestamp = ctime(&now);
	timestamp[strlen(timestamp) - 1] = '\0';
	char full_msg[BUFFER_SIZE];
	snprintf(full_msg, sizeof(full_msg), "[%s] %s", timestamp, grid_str);

	for (int i = 0; i < MAX_RETRIES; i++)
	{
		if (sendto(sock, full_msg, strlen(full_msg), 0, (struct sockaddr *)addr, sizeof(*addr)) >= 0)
			break;
		perror("Ошибка отправки состояния поля, повторная попытка");
		sleep(1);
	}
}

void handle_message(int sock, char *buffer, struct sockaddr_in *client_addr)
{
	char cmd[20];
	int x, y, gardener_id;
	char response[BUFFER_SIZE];
	char log_msg[BUFFER_SIZE];

	int client_idx = find_client(client_addr);
	printf("Сервер получил: %s от клиента %d\n", buffer, client_idx);

	if (sscanf(buffer, "ID %d", &gardener_id) == 1)
	{
		pthread_mutex_lock(&mutex);
		if (client_idx == -1 && num_clients < MAX_CLIENTS)
		{
			int gardeners_count = 0;
			for (int i = 0; i < num_clients; i++)
			{
				if (clients[i].gardener_id > 0 && clients[i].active)
					gardeners_count++;
			}
			if (gardener_id == 0 || (gardener_id > 0 && gardeners_count < 2))
			{
				clients[num_clients].addr = *client_addr;
				clients[num_clients].gardener_id = gardener_id;
				clients[num_clients].last_active = time(NULL);
				clients[num_clients].active = 1;
				num_clients++;
				snprintf(response, sizeof(response), "OK\n");
				snprintf(log_msg, sizeof(log_msg), "Клиент с ID %d подключился\n", gardener_id);
			}
			else
			{
				snprintf(response, sizeof(response), "ERROR\n");
				snprintf(log_msg, sizeof(log_msg), "Ошибка подключения ID %d: превышен лимит садовников\n", gardener_id);
			}
		}
		else if (client_idx != -1 && !clients[client_idx].active)
		{
			clients[client_idx].active = 1;
			clients[client_idx].last_active = time(NULL);
			snprintf(response, sizeof(response), "OK\n");
			snprintf(log_msg, sizeof(log_msg), "Клиент с ID %d повторно подключился\n", gardener_id);
		}
		else
		{
			snprintf(response, sizeof(response), "ERROR\n");
			snprintf(log_msg, sizeof(log_msg), "Клиент уже зарегистрирован или лимит достигнут\n");
		}
		pthread_mutex_unlock(&mutex);
		for (int i = 0; i < MAX_RETRIES; i++)
		{
			if (sendto(sock, response, strlen(response), 0, (struct sockaddr *)client_addr, sizeof(*client_addr)) >= 0)
				break;
			perror("Ошибка отправки ответа на ID, повторная попытка");
			sleep(1);
		}
		send_to_all_monitors(sock, log_msg, client_addr);
	}
	else if (client_idx != -1 && clients[client_idx].active)
	{
		gardener_id = clients[client_idx].gardener_id;
		if (gardener_id > 0) // Только для садовников
		{
			pthread_mutex_lock(&mutex);
			if (sscanf(buffer, "MOVE %d %d", &x, &y) == 2)
			{
				printf("Обработка MOVE %d %d для садовника %d\n", x, y, gardener_id);
				if (x < 0 || x >= M || y < 0 || y >= N || grid[x][y].type == 0)
				{
					snprintf(response, sizeof(response), "INACCESSIBLE\n");
					snprintf(log_msg, sizeof(log_msg), "Садовник %d: клетка (%d,%d) недоступна\n", gardener_id, x, y);
				}
				else if (grid[x][y].occupied_by == 3 - gardener_id)
				{
					snprintf(response, sizeof(response), "WAIT\n");
					snprintf(log_msg, sizeof(log_msg), "Садовник %d ждет клетку (%d,%d)\n", gardener_id, x, y);
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
					snprintf(response, sizeof(response), "MOVED\n");
					snprintf(log_msg, sizeof(log_msg), "Садовник %d переместился в (%d,%d)\n", gardener_id, x, y);
				}
			}
			else if (sscanf(buffer, "GET_STATE %d %d", &x, &y) == 2)
			{
				printf("Обработка GET_STATE %d %d для садовника %d\n", x, y, gardener_id);
				if (x < 0 || x >= M || y < 0 || y >= N)
				{
					snprintf(response, sizeof(response), "INVALID\n");
				}
				else if (grid[x][y].type == 0)
				{
					snprintf(response, sizeof(response), "INACCESSIBLE\n");
				}
				else if (grid[x][y].processed_by != 0)
				{
					snprintf(response, sizeof(response), "PROCESSED_BY %d\n", grid[x][y].processed_by);
				}
				else
				{
					snprintf(response, sizeof(response), "UNPROCESSED\n");
				}
			}
			else if (sscanf(buffer, "OCCUPY %d %d", &x, &y) == 2)
			{
				if (x < 0 || x >= M || y < 0 || y >= N)
				{
					snprintf(response, sizeof(response), "INVALID\n");
				}
				else
				{
					grid[x][y].occupied_by = gardener_id;
					snprintf(response, sizeof(response), "OK\n");
					snprintf(log_msg, sizeof(log_msg), "Садовник %d занял (%d,%d)\n", gardener_id, x, y);
				}
			}
			else if (sscanf(buffer, "FINISH_PROCESSING %d %d", &x, &y) == 2)
			{
				if (x >= 0 && x < M && y >= 0 && y < N && grid[x][y].processed_by == 0)
				{
					grid[x][y].processed_by = gardener_id;
					grid[x][y].occupied_by = 0;
					snprintf(response, sizeof(response), "OK\n");
					snprintf(log_msg, sizeof(log_msg), "Садовник %d обработал (%d,%d)\n", gardener_id, x, y);
				}
				else
				{
					snprintf(response, sizeof(response), "INVALID\n");
				}
			}
			else if (sscanf(buffer, "RELEASE %d %d", &x, &y) == 2)
			{
				if (x >= 0 && x < M && y >= 0 && y < N)
				{
					grid[x][y].occupied_by = 0;
					snprintf(response, sizeof(response), "OK\n");
					snprintf(log_msg, sizeof(log_msg), "Садовник %d освободил (%d,%d)\n", gardener_id, x, y);
				}
				else
				{
					snprintf(response, sizeof(response), "INVALID\n");
				}
			}
			else if (strcmp(buffer, "FINISHED\n") == 0)
			{
				finished_gardeners++;
				snprintf(response, sizeof(response), "OK\n");
				snprintf(log_msg, sizeof(log_msg), "Садовник %d завершил работу\n", gardener_id);
			}
			pthread_mutex_unlock(&mutex);
			for (int i = 0; i < MAX_RETRIES; i++)
			{
				if (sendto(sock, response, strlen(response), 0, (struct sockaddr *)client_addr, sizeof(*client_addr)) >= 0)
					break;
				perror("Ошибка отправки ответа, повторная попытка");
				sleep(1);
			}
			if (log_msg[0] != '\0')
			{
				send_to_all_monitors(sock, log_msg, client_addr);
				send_grid_to_all_monitors(sock);
			}
		}
		else if (gardener_id == 0) // Для монитора
		{
			pthread_mutex_lock(&mutex);
			if (strcmp(buffer, "GET_GRID\n") == 0)
			{
				send_grid(sock, client_addr);
			}
			pthread_mutex_unlock(&mutex);
		}
	}
}

void cleanup_clients()
{
	time_t now = time(NULL);
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < num_clients; i++)
	{
		if (clients[i].active && now - clients[i].last_active > 30)
		{
			char log_msg[BUFFER_SIZE];
			snprintf(log_msg, sizeof(log_msg), "Клиент с ID %d отключен из-за неактивности\n", clients[i].gardener_id);
			send_to_all_monitors(clients[i].addr.sin_addr.s_addr, log_msg, &clients[i].addr);
			clients[i].active = 0;
		}
	}
	pthread_mutex_unlock(&mutex);
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

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		perror("Socket creation failed");
		return 1;
	}

	struct sockaddr_in server_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr.s_addr = INADDR_ANY};

	if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Bind failed");
		close(sock);
		return 1;
	}

	printf("Сервер запущен на порту %d (UDP)\n", port);

	char buffer[BUFFER_SIZE];
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);

	while (finished_gardeners < 2)
	{
		int n = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
		if (n <= 0)
		{
			perror("Ошибка при получении данных");
			continue;
		}
		buffer[n] = '\0';

		pthread_mutex_lock(&mutex);
		int client_idx = find_client(&client_addr);
		if (client_idx != -1)
		{
			clients[client_idx].last_active = time(NULL);
		}
		pthread_mutex_unlock(&mutex);

		handle_message(sock, buffer, &client_addr);
		cleanup_clients();
	}

	printf("Все садовники завершили работу\n");
	close(sock);
	return 0;
}