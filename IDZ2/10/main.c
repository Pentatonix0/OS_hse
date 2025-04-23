#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define M 10
#define N 10
#define BASE_TIME_UNIT 100000

typedef struct
{
	int cells[M][N];
} Garden;

int shm_id;
int sem_id;
Garden *garden;

union semun
{
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};

void print_garden()
{
	printf("\nИтоговое состояние сада:\n");
	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			printf(garden->cells[i][j] == 0 ? "X " : garden->cells[i][j] == 2 ? "1 "
												 : garden->cells[i][j] == 3	  ? "2 "
																			  : ". ");
		}
		printf("\n");
	}
}

void sigint_handler(int sig)
{
	printf("\nУдаление ресурсов...\n");
	shmdt(garden);
	shmctl(shm_id, IPC_RMID, NULL);
	semctl(sem_id, 0, IPC_RMID);
	exit(0);
}

void init_shared_resources()
{
	// Создание разделяемой памяти
	shm_id = shmget(SHM_KEY, sizeof(Garden), IPC_CREAT | 0666);
	garden = (Garden *)shmat(shm_id, NULL, 0);

	// Инициализация сада
	srand(time(NULL));
	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			garden->cells[i][j] = (rand() % 100 < 20) ? 0 : 1;
		}
	}

	// Создание семафоров для каждой клетки
	sem_id = semget(SEM_KEY, M * N, IPC_CREAT | 0666);
	union semun arg;
	unsigned short values[M * N];
	for (int i = 0; i < M * N; i++)
		values[i] = 1;
	arg.array = values;
	semctl(sem_id, 0, SETALL, arg);

	printf("Ресурсы инициализированы. Запустите садовников.\n");
}

void gardener_work(int id, int process_time)
{
	struct sembuf sop;
	int x, y, direction;

	garden = (Garden *)shmat(shm_id, NULL, 0);

	// Начальные позиции
	if (id == 1)
	{
		x = 0;
		y = 0;
		direction = 1;
	}
	else
	{
		x = M - 1;
		y = N - 1;
		direction = -1;
	}

	printf("[Садовник %d] Начал работу\n", id);

	while (1)
	{
		int idx = x * N + y;

		// Захват семафора
		sop.sem_num = idx;
		sop.sem_op = -1;
		sop.sem_flg = 0;
		semop(sem_id, &sop, 1);

		// Обработка клетки
		if (garden->cells[x][y] == 1)
		{
			printf("[Садовник %d] Обрабатывает клетку (%d, %d)\n", id, x, y);
			garden->cells[x][y] = id + 1; // 2 или 3
			usleep(BASE_TIME_UNIT * process_time);
		}

		// Освобождение семафора
		sop.sem_op = 1;
		semop(sem_id, &sop, 1);

		// Перемещение
		usleep(BASE_TIME_UNIT / 10);
		if (id == 1)
		{
			y += direction;
			if (y >= N || y < 0)
			{
				x++;
				direction *= -1;
				y += direction;
				if (x >= M)
					break;
			}
		}
		else
		{
			x += direction;
			if (x < 0 || x >= M)
			{
				y--;
				direction *= -1;
				x += direction;
				if (y < 0)
					break;
			}
		}
	}

	printf("[Садовник %d] Завершил работу\n", id);
	print_garden();
	shmdt(garden);
	exit(0);
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Использование: %s init | 1 | 2 [-tN время]\n", argv[0]);
		return 1;
	}

	signal(SIGINT, sigint_handler);

	if (strcmp(argv[1], "init") == 0)
	{
		init_shared_resources();
		pause();
	}
	else if (strcmp(argv[1], "1") == 0 || strcmp(argv[1], "2") == 0)
	{
		int process_time = 1;
		int id = atoi(argv[1]);

		// Парсинг аргументов времени
		for (int i = 2; i < argc; i++)
		{
			if (strcmp(argv[i], "-t1") == 0 && id == 1)
				process_time = atoi(argv[++i]);
			else if (strcmp(argv[i], "-t2") == 0 && id == 2)
				process_time = atoi(argv[++i]);
		}

		// Подключение ресурсов
		shm_id = shmget(SHM_KEY, sizeof(Garden), 0666);
		sem_id = semget(SEM_KEY, M * N, 0666);

		if (shm_id == -1 || sem_id == -1)
		{
			printf("Ресурсы не инициализированы!\n");
			return 1;
		}

		gardener_work(id, process_time);
	}
	else
	{
		printf("Неверная команда\n");
		return 1;
	}
	return 0;
}