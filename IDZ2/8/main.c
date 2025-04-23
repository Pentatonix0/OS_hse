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

int GARDENER1_PROCESS_TIME = 1;
int GARDENER2_PROCESS_TIME = 1;

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
			int val = garden->cells[i][j];
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

void sigint_handler(int sig)
{
	printf("\nУдаление ресурсов...\n");
	print_garden();
	shmdt(garden);
	exit(0);
}

void init_shared_resources()
{
	// Создание разделяемой памяти
	if ((shm_id = shmget(SHM_KEY, sizeof(Garden), IPC_CREAT | 0666)) == -1)
	{
		perror("shmget");
		exit(1);
	}

	// Создание набора семафоров
	if ((sem_id = semget(SEM_KEY, M * N, IPC_CREAT | 0666)) == -1)
	{
		perror("semget");
		exit(1);
	}

	// Инициализация всех семафоров
	union semun arg;
	unsigned short values[M * N];
	for (int i = 0; i < M * N; i++)
		values[i] = 1;
	arg.array = values;

	if (semctl(sem_id, 0, SETALL, arg) == -1)
	{
		perror("semctl SETALL");
		exit(1);
	}

	garden = (Garden *)shmat(shm_id, NULL, 0);
	srand(time(NULL));
	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			garden->cells[i][j] = (rand() % 100 < 20) ? 0 : 1;
		}
	}
	printf("Ресурсы инициализированы. Запустите садовников.\n");
}

void gardener_work(int id)
{
	struct sembuf sop;
	int x, y, direction;

	garden = (Garden *)shmat(shm_id, NULL, 0);

	if (id == 1)
	{
		x = 0;
		y = 0;
		direction = 1;
		printf("[Садовник 1] Начал работу с верхнего левого угла\n");
	}
	else
	{
		x = M - 1;
		y = N - 1;
		direction = -1;
		printf("[Садовник 2] Начал работу с нижнего правого угла\n");
	}

	while (1)
	{
		int idx = x * N + y;
		sop.sem_num = idx;
		sop.sem_op = -1;
		sop.sem_flg = IPC_NOWAIT;

		if (semop(sem_id, &sop, 1) != -1)
		{
			if (garden->cells[x][y] == 1)
			{
				printf("[Садовник %d] Обрабатывает клетку (%d, %d)\n", id, x, y);
				fflush(stdout);
				usleep(BASE_TIME_UNIT * (id == 1 ? GARDENER1_PROCESS_TIME : GARDENER2_PROCESS_TIME));
				garden->cells[x][y] = id + 1;
			}
			sop.sem_op = 1;
			semop(sem_id, &sop, 1);
		}

		usleep(BASE_TIME_UNIT);
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

	printf("\n[Садовник %d] Завершил работу. Результат:\n", id);
	print_garden();
	shmdt(garden);
	exit(0);
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Неверное использование");
		return 1;
	}

	for (int i = 2; i < argc; i++)
	{
		if (strcmp(argv[i], "-t1") == 0 && i + 1 < argc)
			GARDENER1_PROCESS_TIME = atoi(argv[++i]);
		else if (strcmp(argv[i], "-t2") == 0 && i + 1 < argc)
			GARDENER2_PROCESS_TIME = atoi(argv[++i]);
	}

	signal(SIGINT, sigint_handler);

	if (strcmp(argv[1], "init") == 0)
	{
		init_shared_resources();
		pause();
		printf("\nФинальное состояние после прерывания:\n");
		print_garden();
		shmctl(shm_id, IPC_RMID, NULL);
		semctl(sem_id, 0, IPC_RMID);
	}
	else if (strcmp(argv[1], "1") == 0 || strcmp(argv[1], "2") == 0)
	{
		shm_id = shmget(SHM_KEY, sizeof(Garden), 0666);
		sem_id = semget(SEM_KEY, M * N, 0666);

		if (shm_id == -1 || sem_id == -1)
		{
			printf("Ресурсы не инициализированы!\n");
			return 1;
		}
		gardener_work(atoi(argv[1]));
	}
	else
	{
		printf("Неверная команда\n");
		return 1;
	}
	return 0;
}