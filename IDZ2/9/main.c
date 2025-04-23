#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <mqueue.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define M 10
#define N 10
#define BASE_TIME_UNIT 100000
#define QUEUE_NAME "/garden_queue"
#define SEM_NAME "/garden_sem"

typedef struct
{
	int cells[M][N];
} Garden;

sem_t *sem;
mqd_t mq;

void print_garden(Garden *garden)
{
	printf("\nИтоговое состояние сада:\n");
	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			int val = garden->cells[i][j];
			printf(val == 0 ? "X " : val == 2 ? "1 "
								 : val == 3	  ? "2 "
											  : ". ");
		}
		printf("\n");
	}
}

void sigint_handler(int sig)
{
	printf("\nУдаление ресурсов...\n");
	sem_close(sem);
	sem_unlink(SEM_NAME);
	mq_close(mq);
	mq_unlink(QUEUE_NAME);
	exit(0);
}

void init_resources()
{
	// Создание POSIX семафора
	sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
	if (sem == SEM_FAILED)
	{
		perror("sem_open");
		exit(1);
	}

	// Создание POSIX очереди
	struct mq_attr attr = {0};
	attr.mq_maxmsg = 10;
	attr.mq_msgsize = sizeof(Garden);
	mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);
	if (mq == (mqd_t)-1)
	{
		perror("mq_open");
		exit(1);
	}

	// Инициализация сада
	Garden garden;
	srand(time(NULL));
	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			garden.cells[i][j] = (rand() % 100 < 20) ? 0 : 1;
		}
	}

	if (mq_send(mq, (char *)&garden, sizeof(Garden), 0) == -1)
	{
		perror("mq_send");
		exit(1);
	}

	printf("Ресурсы инициализированы. Запустите садовников.\n");
}

void gardener_work(int id, int process_time)
{
	Garden garden;
	int x, y, direction;

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

	while (1)
	{
		// Чтение текущего состояния
		if (mq_receive(mq, (char *)&garden, sizeof(Garden), NULL) == -1)
		{
			perror("mq_receive");
			exit(1);
		}

		sem_wait(sem);
		if (garden.cells[x][y] == 1)
		{
			printf("[Садовник %d] Обрабатывает клетку (%d, %d)\n", id, x, y);
			usleep(BASE_TIME_UNIT * process_time);
			garden.cells[x][y] = id + 1;
		}
		sem_post(sem);

		// Отправка обновленного состояния
		if (mq_send(mq, (char *)&garden, sizeof(Garden), 0) == -1)
		{
			perror("mq_send");
			exit(1);
		}

		// Перемещение
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

	printf("\n[Садовник %d] Завершил работу\n", id);
	print_garden(&garden);
	exit(0);
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Неправильное использование");
		return 1;
	}

	signal(SIGINT, sigint_handler);

	if (strcmp(argv[1], "init") == 0)
	{
		init_resources();
		pause();
		Garden garden;
		mq_receive(mq, (char *)&garden, sizeof(Garden), NULL);
		printf("\nФинальное состояние:\n");
		print_garden(&garden);
		sem_close(sem);
		sem_unlink(SEM_NAME);
		mq_close(mq);
		mq_unlink(QUEUE_NAME);
	}
	else if (strcmp(argv[1], "1") == 0 || strcmp(argv[1], "2") == 0)
	{
		int process_time = 1;
		for (int i = 2; i < argc; i++)
		{
			if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
				process_time = atoi(argv[++i]);
		}

		sem = sem_open(SEM_NAME, 0);
		mq = mq_open(QUEUE_NAME, O_RDWR);

		if (sem == SEM_FAILED || mq == (mqd_t)-1)
		{
			printf("Ресурсы не инициализированы!\n");
			return 1;
		}

		gardener_work(atoi(argv[1]), process_time);
	}
	else
	{
		printf("Неверная команда\n");
		return 1;
	}
	return 0;
}