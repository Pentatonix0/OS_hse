#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define SHM_NAME "/gardenshm"
#define M 10
#define N 10
#define BASE_TIME_UNIT 100000 // 1 единица времени = 100ms

int GARDENER1_PROCESS_TIME = 1;
int GARDENER2_PROCESS_TIME = 1;

void gardener1_work();
void gardener2_work();
void print_garden();
void parse_args(int argc, char *argv[]);

typedef struct
{
	int rows;
	int cols;
	sem_t cell_mutex[M][N]; // Неименованные семафоры
	int cells[M][N];
} SharedData;

SharedData *shared_data = NULL;
volatile sig_atomic_t exiting = 0;

// Обработчик сигнала SIGINT
void sigint_handler(int sig)
{
	if (exiting)
		return;
	exiting = 1;

	if (shared_data)
	{
		for (int i = 0; i < M; i++)
		{
			for (int j = 0; j < N; j++)
			{
				sem_destroy(&shared_data->cell_mutex[i][j]); // Уничтожение семафоров
			}
		}
		munmap(shared_data, sizeof(SharedData));
		shm_unlink(SHM_NAME);
	}
	printf("\nУдаление ресурсов...\n");
	exit(EXIT_SUCCESS);
}

// Логика первого садовника
void gardener1_work()
{
	int x = 0, y = 0, direction = 1;
	while (x < M)
	{
		if (sem_trywait(&shared_data->cell_mutex[x][y]) == 0)
		{
			if (shared_data->cells[x][y] == 1)
			{
				printf("[Садовник 1] обрабатывает клетку (%d, %d)\n", x, y);
				usleep(BASE_TIME_UNIT * GARDENER1_PROCESS_TIME);
				shared_data->cells[x][y] = 2;
			}
			sem_post(&shared_data->cell_mutex[x][y]);
		}

		usleep(BASE_TIME_UNIT); // Перемещение

		y += direction;
		if (y >= N || y < 0)
		{
			x++;
			direction *= -1;
			y += direction;
		}
	}
	exit(EXIT_SUCCESS);
}

// Логика второго садовника
void gardener2_work()
{
	int x = M - 1, y = N - 1, direction = -1;
	while (y >= 0)
	{
		if (sem_trywait(&shared_data->cell_mutex[x][y]) == 0)
		{
			if (shared_data->cells[x][y] == 1)
			{
				printf("[Садовник 2] обрабатывает клетку (%d, %d)\n", x, y);
				usleep(BASE_TIME_UNIT * GARDENER2_PROCESS_TIME);
				shared_data->cells[x][y] = 3;
			}
			sem_post(&shared_data->cell_mutex[x][y]);
		}

		usleep(BASE_TIME_UNIT); // Перемещение

		x += direction;
		if (x < 0 || x >= M)
		{
			y--;
			direction *= -1;
			x += direction;
		}
	}
	exit(EXIT_SUCCESS);
}

// Вывод состояния сада
void print_garden()
{
	printf("\nИтоговое состояние сада:\n");
	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			int val = shared_data->cells[i][j];
			if (val == 0)
				printf("X ");
			else if (val == 2)
				printf("1 ");
			else if (val == 3)
				printf("2 ");
			else
				printf(". ");
		}
		printf("\n");
	}
}

// Парсинг аргументов
void parse_args(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-t1") == 0 && i + 1 < argc)
		{
			GARDENER1_PROCESS_TIME = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-t2") == 0 && i + 1 < argc)
		{
			GARDENER2_PROCESS_TIME = atoi(argv[++i]);
		}
	}
}

int main(int argc, char *argv[])
{
	parse_args(argc, argv);
	srand(time(NULL));

	struct sigaction sa;
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);

	// Создание разделяемой памяти
	int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	ftruncate(shm_fd, sizeof(SharedData));
	shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

	// Инициализация семафоров и сада
	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			sem_init(&shared_data->cell_mutex[i][j], 1, 1); // Инициализация неименованных семафоров
			shared_data->cells[i][j] = (rand() % 100 < 20) ? 0 : 1;
		}
	}

	// Создание процессов
	pid_t pid1 = fork();
	if (pid1 == 0)
		gardener1_work();

	pid_t pid2 = fork();
	if (pid2 == 0)
		gardener2_work();

	// Ожидание завершения
	waitpid(pid1, NULL, 0);
	waitpid(pid2, NULL, 0);

	print_garden();
	sigint_handler(SIGINT);
	return 0;
}