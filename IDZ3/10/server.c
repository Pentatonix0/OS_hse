#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#define M 5
#define N 5
#define MAX_MONITORS 10 // Максимальное количество мониторов
#define MAX_GARDENERS 2 // Ровно два садовника

typedef struct
{
	int type;		  // 0: недоступно, 1: доступно
	int processed_by; // 0: не обработано, 1: садовник1, 2: садовник2
	int occupied_by;  // 0: никто, 1: садовник1, 2: садовник2
} Cell;

Cell grid[M][N];
int gardener_pos[2][2]; // Позиции садовников: [gardener_id-1][0] = x, [gardener_id-1][1] = y
pthread_mutex_t grid_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t monitor_mutex = PTHREAD_MUTEX_INITIALIZER;
int finished_gardeners = 0;
int monitor_socks[MAX_MONITORS];		 // Массив сокетов мониторов
int gardener_socks[MAX_GARDENERS];		 // Массив сокетов садовников
int monitor_count = 0;					 // Текущее количество мониторов
int gardener_count = 0;					 // Текущее количество садовников
volatile sig_atomic_t shutdown_flag = 0; // Флаг завершения

void signal_handler(int signum)
{
	shutdown_flag = 1;
}

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

void log_message(const char *message)
{
	time_t now = time(NULL);
	char *timestamp = ctime(&now);
	timestamp[strlen(timestamp) - 1] = '\0';
	printf("[%s] %s", timestamp, message);
}

void send_grid_to_all_monitors(const char *grid_str)
{
	time_t now = time(NULL);
	char *timestamp = ctime(&now);
	timestamp[strlen(timestamp) - 1] = '\0';
	char timed_message[2048];
	snprintf(timed_message, sizeof(timed_message), "[%s] %s", timestamp, grid_str);

	pthread_mutex_lock(&monitor_mutex);
	for (int i = 0; i < monitor_count; i++)
	{
		if (monitor_socks[i] != -1)
		{
			if (send(monitor_socks[i], timed_message, strlen(timed_message), 0) < 0)
			{
				perror("Ошибка отправки монитору");
				monitor_socks[i] = -1;
			}
		}
	}
	int j = 0;
	for (int i = 0; i < monitor_count; i++)
	{
		if (monitor_socks[i] != -1)
			monitor_socks[j++] = monitor_socks[i];
	}
	monitor_count = j;
	pthread_mutex_unlock(&monitor_mutex);
}

void print_grid_to_all_monitors()
{
	char grid_str[1024] = "";
	snprintf(grid_str, sizeof(grid_str), "Итоговое состояние сада:\n  ");
	for (int j = 0; j < N; j++)
		snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "%d ", j);
	snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "\n");
	for (int i = 0; i < M; i++)
	{
		snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "%d ", i);
		for (int j = 0; j < N; j++)
		{
			if (grid[i][j].type == 0)
				snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "X ");
			else if (grid[i][j].processed_by == 0)
				snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "U ");
			else
				snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "%d ", grid[i][j].processed_by);
		}
		snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str), "\n");
	}
	snprintf(grid_str + strlen(grid_str), sizeof(grid_str) - strlen(grid_str),
			 "Легенда: X - недоступно, U - необработано, 1 - обработано садовником 1, 2 - обработано садовником 2\n");

	send_grid_to_all_monitors(grid_str);
	log_message(grid_str);
}

void send_shutdown_to_all_clients()
{
	const char *shutdown_msg = "SERVER_SHUTDOWN\n";

	pthread_mutex_lock(&grid_mutex);
	for (int i = 0; i < MAX_GARDENERS; i++)
	{
		if (gardener_socks[i] != -1)
		{
			if (send(gardener_socks[i], shutdown_msg, strlen(shutdown_msg), 0) < 0)
			{
				perror("Ошибка отправки садовнику");
			}
			close(gardener_socks[i]);
			gardener_socks[i] = -1;
		}
	}
	gardener_count = 0;
	pthread_mutex_unlock(&grid_mutex);

	pthread_mutex_lock(&monitor_mutex);
	for (int i = 0; i < monitor_count; i++)
	{
		if (monitor_socks[i] != -1)
		{
			if (send(monitor_socks[i], shutdown_msg, strlen(shutdown_msg), 0) < 0)
			{
				perror("Ошибка отправки монитору");
			}
			close(monitor_socks[i]);
			monitor_socks[i] = -1;
		}
	}
	monitor_count = 0;
	pthread_mutex_unlock(&monitor_mutex);

	log_message("Отправлено уведомление о завершении работы всем клиентам\n");
}

void *handle_client(void *arg)
{
	int sock = *(int *)arg;
	char buffer[1024];
	char msg[2048];
	int gardener_id = -1;
	int is_monitor = 0;

	int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
	if (n <= 0)
	{
		if (n < 0)
			perror("Ошибка получения идентификации");
		snprintf(msg, sizeof(msg), "Клиент отключился при идентификации\n");
		log_message(msg);
		close(sock);
		free(arg);
		return NULL;
	}
	buffer[n] = '\0';

	if (sscanf(buffer, "ID %d", &gardener_id) == 1 && gardener_id >= 1 && gardener_id <= 2)
	{
		pthread_mutex_lock(&grid_mutex);
		if (gardener_count >= MAX_GARDENERS)
		{
			snprintf(msg, sizeof(msg), "Слишком много садовников, отклонено подключение ID %d\n", gardener_id);
			log_message(msg);
			send(sock, "ERROR: Too many gardeners\n", 26, 0);
			pthread_mutex_unlock(&grid_mutex);
			close(sock);
			free(arg);
			return NULL;
		}
		gardener_count++;
		gardener_socks[gardener_id - 1] = sock; // Сохраняем сокет садовника
		pthread_mutex_unlock(&grid_mutex);

		if (send(sock, "OK\n", 3, 0) < 0)
		{
			perror("Ошибка отправки OK садовнику");
			snprintf(msg, sizeof(msg), "Не удалось отправить подтверждение садовнику %d\n", gardener_id);
			log_message(msg);
			pthread_mutex_lock(&grid_mutex);
			gardener_count--;
			gardener_socks[gardener_id - 1] = -1;
			pthread_mutex_unlock(&grid_mutex);
			close(sock);
			free(arg);
			return NULL;
		}
		snprintf(msg, sizeof(msg), "Садовник %d подключился\n", gardener_id);
		log_message(msg);
	}
	else if (strcmp(buffer, "MONITOR\n") == 0)
	{
		is_monitor = 1;
		pthread_mutex_lock(&monitor_mutex);
		if (monitor_count >= MAX_MONITORS)
		{
			snprintf(msg, sizeof(msg), "Достигнуто максимальное количество мониторов\n");
			log_message(msg);
			send(sock, "ERROR: Too many monitors\n", 25, 0);
			pthread_mutex_unlock(&monitor_mutex);
			close(sock);
			free(arg);
			return NULL;
		}
		monitor_socks[monitor_count++] = sock;
		pthread_mutex_unlock(&monitor_mutex);

		if (send(sock, "OK\n", 3, 0) < 0)
		{
			perror("Ошибка отправки OK монитору");
			snprintf(msg, sizeof(msg), "Не удалось отправить подтверждение монитору\n");
			log_message(msg);
			pthread_mutex_lock(&monitor_mutex);
			monitor_count--;
			pthread_mutex_unlock(&monitor_mutex);
			close(sock);
			free(arg);
			return NULL;
		}
		snprintf(msg, sizeof(msg), "Монитор подключился\n");
		log_message(msg);
	}
	else
	{
		snprintf(msg, sizeof(msg), "Неизвестный тип клиента: %s", buffer);
		log_message(msg);
		send(sock, "ERROR: Invalid client type\n", 27, 0);
		close(sock);
		free(arg);
		return NULL;
	}

	while (1)
	{
		n = recv(sock, buffer, sizeof(buffer) - 1, 0);
		if (n <= 0)
		{
			if (n < 0)
				perror("Ошибка получения данных");
			if (is_monitor)
			{
				snprintf(msg, sizeof(msg), "Монитор отключился\n");
				log_message(msg);
				pthread_mutex_lock(&monitor_mutex);
				for (int i = 0; i < monitor_count; i++)
				{
					if (monitor_socks[i] == sock)
					{
						monitor_socks[i] = monitor_socks[monitor_count - 1];
						monitor_count--;
						break;
					}
				}
				pthread_mutex_unlock(&monitor_mutex);
			}
			else
			{
				snprintf(msg, sizeof(msg), "Садовник %d отключился\n", gardener_id);
				log_message(msg);
				pthread_mutex_lock(&grid_mutex);
				gardener_count--;
				gardener_socks[gardener_id - 1] = -1;
				if (gardener_pos[gardener_id - 1][0] != -1)
				{
					grid[gardener_pos[gardener_id - 1][0]][gardener_pos[gardener_id - 1][1]].occupied_by = 0;
				}
				pthread_mutex_unlock(&grid_mutex);
			}
			close(sock);
			free(arg);
			return NULL;
		}
		buffer[n] = '\0';

		if (is_monitor)
		{
			if (strcmp(buffer, "GET_GRID\n") == 0)
			{
				snprintf(msg, sizeof(msg), "Монитор запросил состояние сада\n");
				log_message(msg);
				print_grid_to_all_monitors();
			}
			else if (strcmp(buffer, "EXIT\n") == 0)
			{
				snprintf(msg, sizeof(msg), "Монитор отключился по команде\n");
				log_message(msg);
				pthread_mutex_lock(&monitor_mutex);
				for (int i = 0; i < monitor_count; i++)
				{
					if (monitor_socks[i] == sock)
					{
						monitor_socks[i] = monitor_socks[monitor_count - 1];
						monitor_count--;
						break;
					}
				}
				pthread_mutex_unlock(&monitor_mutex);
				close(sock);
				free(arg);
				return NULL;
			}
			else
			{
				snprintf(msg, sizeof(msg), "Неизвестная команда от монитора: %s", buffer);
				log_message(msg);
			}
		}
		else
		{
			int x, y;
			if (sscanf(buffer, "MOVE %d %d", &x, &y) == 2)
			{
				pthread_mutex_lock(&grid_mutex);
				if (x < 0 || x >= M || y < 0 || y >= N || grid[x][y].type == 0)
				{
					send(sock, "INACCESSIBLE\n", 13, 0);
					snprintf(msg, sizeof(msg), "Садовник %d пытается переместиться в недоступную клетку (%d, %d)\n", gardener_id, x, y);
					log_message(msg);
				}
				else
				{
					int other_gardener = 3 - gardener_id;
					if (grid[x][y].occupied_by == other_gardener)
					{
						send(sock, "WAIT\n", 5, 0);
						snprintf(msg, sizeof(msg), "Садовник %d ждет освобождения (%d, %d)\n", gardener_id, x, y);
						log_message(msg);
					}
					else
					{
						int prev_x = gardener_pos[gardener_id - 1][0];
						int prev_y = gardener_pos[gardener_id - 1][1];
						if (prev_x != -1 && prev_y != -1)
							grid[prev_x][prev_y].occupied_by = 0;
						grid[x][y].occupied_by = gardener_id;
						gardener_pos[gardener_id - 1][0] = x;
						gardener_pos[gardener_id - 1][1] = y;
						send(sock, "MOVED\n", 6, 0);
						snprintf(msg, sizeof(msg), "Садовник %d переместился в (%d, %d)\n", gardener_id, x, y);
						log_message(msg);
					}
				}
				pthread_mutex_unlock(&grid_mutex);
			}
			else if (sscanf(buffer, "GET_STATE %d %d", &x, &y) == 2)
			{
				pthread_mutex_lock(&grid_mutex);
				char response[50];
				if (x < 0 || x >= M || y < 0 || y >= N || grid[x][y].type == 0)
					strcpy(response, "INACCESSIBLE\n");
				else if (grid[x][y].processed_by != 0)
					snprintf(response, sizeof(response), "ACCESSIBLE PROCESSED_BY %d\n", grid[x][y].processed_by);
				else
					strcpy(response, "ACCESSIBLE UNPROCESSED\n");
				send(sock, response, strlen(response), 0);
				snprintf(msg, sizeof(msg), "Садовник %d запросил состояние клетки (%d, %d)\n", gardener_id, x, y);
				log_message(msg);
				pthread_mutex_unlock(&grid_mutex);
			}
			else if (sscanf(buffer, "OCCUPY %d %d", &x, &y) == 2)
			{
				pthread_mutex_lock(&grid_mutex);
				send(sock, "OK\n", 3, 0);
				snprintf(msg, sizeof(msg), "Садовник %d занял клетку (%d, %d)\n", gardener_id, x, y);
				log_message(msg);
				pthread_mutex_unlock(&grid_mutex);
			}
			else if (sscanf(buffer, "FINISH_PROCESSING %d %d", &x, &y) == 2)
			{
				pthread_mutex_lock(&grid_mutex);
				if (x >= 0 && x < M && y >= 0 && y < N && grid[x][y].type != 0 && grid[x][y].processed_by == 0)
					grid[x][y].processed_by = gardener_id;
				send(sock, "OK\n", 3, 0);
				snprintf(msg, sizeof(msg), "Садовник %d завершил обработку (%d, %d)\n", gardener_id, x, y);
				log_message(msg);
				pthread_mutex_unlock(&grid_mutex);
			}
			else if (sscanf(buffer, "RELEASE %d %d", &x, &y) == 2)
			{
				pthread_mutex_lock(&grid_mutex);
				if (x >= 0 && x < M && y >= 0 && y < N)
					grid[x][y].occupied_by = 0;
				send(sock, "OK\n", 3, 0);
				snprintf(msg, sizeof(msg), "Садовник %d освободил клетку (%d, %d)\n", gardener_id, x, y);
				log_message(msg);
				pthread_mutex_unlock(&grid_mutex);
			}
			else if (strcmp(buffer, "FINISHED\n") == 0)
			{
				pthread_mutex_lock(&grid_mutex);
				finished_gardeners++;
				snprintf(msg, sizeof(msg), "Садовник %d завершил свою задачу\n", gardener_id);
				log_message(msg);
				gardener_count--;
				gardener_socks[gardener_id - 1] = -1;
				if (gardener_pos[gardener_id - 1][0] != -1)
					grid[gardener_pos[gardener_id - 1][0]][gardener_pos[gardener_id - 1][1]].occupied_by = 0;
				if (finished_gardeners == MAX_GARDENERS)
					print_grid_to_all_monitors();
				pthread_mutex_unlock(&grid_mutex);
				close(sock);
				free(arg);
				return NULL;
			}
			else
			{
				snprintf(msg, sizeof(msg), "Неизвестная команда от садовника %d: %s", gardener_id, buffer);
				log_message(msg);
				send(sock, "ERROR: Unknown command\n", 23, 0);
			}
		}
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

	for (int i = 0; i < MAX_MONITORS; i++)
		monitor_socks[i] = -1;
	for (int i = 0; i < MAX_GARDENERS; i++)
		gardener_socks[i] = -1;

	int server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock < 0)
	{
		perror("Ошибка создания сокета");
		return 1;
	}

	int opt = 1;
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		perror("Ошибка setsockopt");
		close(server_sock);
		return 1;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Ошибка привязки");
		close(server_sock);
		return 1;
	}

	if (listen(server_sock, 10) < 0)
	{
		perror("Ошибка listen");
		close(server_sock);
		return 1;
	}

	signal(SIGINT, signal_handler);

	printf("Сервер запущен на порту %d\n", port);

	while (!shutdown_flag)
	{
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(server_sock, &read_fds);

		int activity = select(server_sock + 1, &read_fds, NULL, NULL, &timeout);
		if (activity < 0 && errno != EINTR)
		{
			perror("Ошибка select");
			continue;
		}
		if (shutdown_flag)
			break;

		if (FD_ISSET(server_sock, &read_fds))
		{
			int *client_sock = malloc(sizeof(int));
			if (!client_sock)
			{
				perror("Ошибка malloc");
				continue;
			}
			*client_sock = accept(server_sock, NULL, NULL);
			if (*client_sock < 0)
			{
				perror("Ошибка accept");
				free(client_sock);
				continue;
			}

			pthread_t thread;
			if (pthread_create(&thread, NULL, handle_client, client_sock) != 0)
			{
				perror("Ошибка создания потока");
				close(*client_sock);
				free(client_sock);
				continue;
			}
			pthread_detach(thread);
		}
	}

	log_message("Сервер получил сигнал завершения, инициируется shutdown\n");
	send_shutdown_to_all_clients();

	close(server_sock);
	pthread_mutex_destroy(&grid_mutex);
	pthread_mutex_destroy(&monitor_mutex);
	log_message("Сервер завершил работу\n");
	return 0;
}