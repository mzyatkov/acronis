#include <atomic>
#include <thread>
#include <stdio.h>
#include <unistd.h>
#include <queue>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <cstdlib>
#include <malloc.h>
#include <mutex>
#include <condition_variable>
#include <sys/epoll.h>

enum
{
	READY,
	PROCESSING
};

#define PIPE1 "pipe_1"
#define PIPE2 "pipe_2"
#define PIPE3 "pipe_3"
#define PIPE4 "pipe_4"
#define PIPE5 "pipe_5"
#define PIPESNUM 5

#ifndef THREADNUM
#define THREADNUM 16
#endif

typedef struct
{
	char bytes[PATH_MAX];
	int size;
} JobItem;

typedef struct
{
	std::queue<JobItem *> jobs;
	std::atomic<int> count = {0};
	// std::atomic<int> status = {READY};
	std::mutex mtx;
} MyQueue;

MyQueue queue[THREADNUM] = {};

void back_off(unsigned int n)
{
	for (unsigned int i = 0; i < n; ++i)
		__asm__ volatile("nop\n\t");
}

// int queue_push(MyQueue *queue, JobItem *job)
// {
// 	queue->jobs.push(job);
// 	queue->count.fetch_add(1, std::memory_order_relaxed);
// 	return 0;
// }

int queue_push(MyQueue *queue, JobItem *job) {
	// thread safe queue push with mutex 
	std::unique_lock<std::mutex> lock(queue->mtx);
	queue->jobs.push(job);
	queue->count.fetch_add(1, std::memory_order_relaxed);
	lock.unlock();
	return 0;
}

JobItem *queue_pop(MyQueue *queue, int stealing)
{
	// thread safe queue pop with mutex
	std::unique_lock<std::mutex> lock(queue->mtx);
	int cur_size = queue->count.load(std::memory_order_relaxed);
	if (cur_size == 0 || (stealing && cur_size == 1))
	{
		lock.unlock();
		return NULL;
	}
	JobItem *tmp = queue->jobs.front();
	queue->jobs.pop();
	queue->count.fetch_sub(1, std::memory_order_relaxed);
	lock.unlock();
	return tmp;
}

// JobItem *queue_pop(MyQueue *queue, int stealing)
// {
// 	queue->mtx.lock();
// 	int cur_size = queue->jobs.size();
// 	if (cur_size == 0 || (stealing && cur_size == 1))
// 	{
// 		queue->mtx.unlock();
// 		return NULL;
// 	}
// 	JobItem *tmp = queue->jobs.front();
// 	queue->jobs.pop();
// 	queue->count.fetch_sub(1, std::memory_order_relaxed);
// 	queue->mtx.unlock();
// 	return tmp;
// }

int check_jobs_number(int threadnum)
{
	return queue[threadnum].count.load(std::memory_order_relaxed);
}

int get_max_min_proc(int *max_proc)
{ // вернет наименее загруженный поток, max_proc - наиболее загруженный
	int mintr = 0;
	int maxtr = 0;

	int size = check_jobs_number(0);
	int min = size, max = size;

	for (int i = 1; i < THREADNUM; ++i)
	{
		int size = check_jobs_number(i);
		if (size > max)
		{
			max = size;
			maxtr = i;
		}
		else if (size <= min)
		{
			min = size;
			mintr = i;
		}
	}

	if (max <= 1)
		maxtr = mintr;

	if (max_proc != NULL)
	{
		*max_proc = maxtr;
	}

	return mintr;
}

int make_job(JobItem *job, int fd)
{
	if (job->size == 0)
	{
		return -1;
	}
	write(fd, job->bytes, job->size);
	fsync(fd);
	return 0;
}

void worker_thread(int threadnum)
{
	JobItem *current;
	int max_load_proc = threadnum;
	int res = 0; // если получили последний job, то завершаем процесс
	int fd = open("result.txt", O_WRONLY | O_CREAT | O_APPEND);
	if (fd == -1)
	{
		perror("не открыли файл результата");
		exit(1);
	}

	while (1)
	{
		while ((current = queue_pop(queue + threadnum, 0)) != NULL)
		{
			res = make_job(current, fd);
			printf("поток %d выполнил свою работу\n", threadnum);
			free(current);
		}
		if (res)
			break;

		std::this_thread::yield();
		int min_load_proc = get_max_min_proc(&max_load_proc); // последнюю задачу воровать не будем
		if (min_load_proc == max_load_proc)
		{
			back_off(100);
			continue;
		}
		if ((current = queue_pop(queue + max_load_proc, 1)) != NULL)
		{
			make_job(current, fd);
			printf("поток %d выполнил работу потока %d \n", threadnum, max_load_proc);
			free(current);
		}
	}

	close(fd);
	printf("поток %d закончил\n", threadnum);
	fflush(0);
}

void close_fds(int *fds)
{

	for (int i = 0; i < PIPESNUM; ++i)
	{
		if (close(fds[i]) == -1 && errno != EBADF)
		{
			printf("error when close pipe %d", i);
			exit(1);
		}
	}

	free(fds);
}

int open_fds(int **fds, int flags)
{

	(*fds) = (int *)calloc(sizeof(int), PIPESNUM);

	(*fds)[0] = open(PIPE1, flags);
	(*fds)[1] = open(PIPE2, flags);
	(*fds)[2] = open(PIPE3, flags);
	(*fds)[3] = open(PIPE4, flags);
	(*fds)[4] = open(PIPE5, flags);
	if ((*fds)[0] == -1 || (*fds)[1] == -1 || (*fds)[2] == -1 || (*fds)[3] == -1 || (*fds)[4] == -1)
	{
		exit(1);
	}

	return PIPESNUM;
}

void make_pipes()
{
	umask(0);
	if (mkfifo(PIPE1, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
	{
		printf("open FIFO %s", PIPE1);
		exit(1);
	}
	if (mkfifo(PIPE2, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
	{
		printf("open FIFO %s", PIPE2);
		exit(1);
	}
	if (mkfifo(PIPE3, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
	{
		printf("open FIFO %s", PIPE3);
		exit(1);
	}
	if (mkfifo(PIPE4, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
	{
		printf("open FIFO %s", PIPE4);
		exit(1);
	}
	if (mkfifo(PIPE5, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
	{
		printf("open FIFO %s", PIPE5);
		exit(1);
	}
}

void master_thread()
{
	struct timeval waiting_time = {
		.tv_sec = 1,
		.tv_usec = 0 
	};

	fd_set read_fds;
	int readsize;
	int *fds = NULL;
	int size = open_fds(&fds, O_RDONLY);
	for (;;)
	{
		FD_ZERO(&read_fds);
		for (int i = 0; i < size; ++i)
		{
			FD_SET(fds[i], &read_fds);
		}

		if (select(FD_SETSIZE, &read_fds, NULL, NULL, &waiting_time) <= 0)
		{
			close_fds(fds);
			break;
		}

		for (int i = 0; i < size; ++i)
		{
			if (FD_ISSET(fds[i], &read_fds))
			{
				ioctl(fds[i], FIONREAD, &readsize);
				if (readsize == 0)
				{
					fds[i] = fds[size - 1];
					size--;
					continue;
				}

				JobItem *job = (JobItem *)calloc(sizeof(JobItem), 1);
				if ((job->size = read(fds[i], job->bytes, PATH_MAX)) < 0)
				{
					free(job);
					printf("Job size < 0, fdsize = %d\n", size);
					exit(1);
				}
				queue_push(queue + get_max_min_proc(NULL), job);
			}
		}

		waiting_time.tv_sec = 1;
	}

	for (int i = 0; i < THREADNUM; ++i)
	{
		JobItem *job = (JobItem *)calloc(sizeof(JobItem), 1);
		queue_push(queue + i, job);
	}

	printf("master_thread finished\n");
	fflush(0);
}

// // master thread with epoll
// void master_thread() {
// 	struct epoll_event ev, events[PIPESNUM];
// 	int epollfd = epoll_create(PIPESNUM);
// 	int *fds = NULL;
// 	int size = open_fds(&fds, O_RDONLY);
// 	for (int i = 0; i < size; ++i) {
// 		ev.events = EPOLLIN;
// 		ev.data.fd = fds[i];
// 		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fds[i], &ev) == -1) {
// 			printf("epoll_ctl: add");
// 			exit(1);
// 		}
// 	}

// 	for (;;) {
// 		int nfds = epoll_wait(epollfd, events, PIPESNUM, 2000);
// 		if (nfds == -1) {
// 			printf("epoll_wait");
// 			exit(1);
// 		} else if (nfds == 0) {
// 			close_fds(fds);
// 			break;
// 		}

// 		for (int i = 0; i < nfds; ++i) {
// 			if (events[i].events & EPOLLIN) {
// 				JobItem *job = (JobItem *)calloc(sizeof(JobItem), 1);
// 				if ((job->size = read(events[i].data.fd, job->bytes, PATH_MAX)) < 0) {
// 					free(job);
// 					printf("Job size < 0, fdsize = %d\n", size);
// 					exit(1);
// 				}
// 				queue_push(queue + get_max_min_proc(NULL), job);
// 			} 
// 		}
// 	}

// 	for (int i = 0; i < THREADNUM; ++i) {
// 		JobItem *job = (JobItem *)calloc(sizeof(JobItem), 1);
// 		queue_push(queue + i, job);
// 	}

// 	printf("master_thread finished\n");
// 	fflush(0);

// }

void emulate_load()
{
	int *fds = NULL;
	int size = open_fds(&fds, O_WRONLY);
	FILE *fin = fopen("text.txt", "rb");
	int readsize = 0;
	int counter = 0;
	char buf[PATH_MAX] = {};
	while ((readsize = fread(buf, sizeof(char), PATH_MAX, fin)) > 0)
	{
		write(fds[counter], buf, PATH_MAX);
		counter = (counter + 1) % size;
	}
	fclose(fin);
	close_fds(fds);
	printf("emulate_load finished\n");
}

int main(int argc, char *argv[])
{
	std::thread *threads[THREADNUM + 1] = {};

	make_pipes();

	pid_t pid = fork();

	if (pid < 0)
	{
		exit(1);
	}

	if (pid == 0)
	{
		emulate_load();
		exit(0);
	}

	auto start = std::chrono::high_resolution_clock::now();

	threads[THREADNUM] = new std::thread(master_thread);

	for (int i = 0; i < THREADNUM; i++)
		threads[i] = new std::thread(worker_thread, i);

	for (int i = 0; i < THREADNUM + 1; i++)
	{
		threads[i]->join();
		delete threads[i];
	}
	auto end = std::chrono::high_resolution_clock::now();
	printf("join succesful\n");
	std::chrono::duration<double> diff = end - start;
	FILE *fout = fopen("times.txt", "a");
	fprintf(fout, "%d,%lf\n", THREADNUM, diff.count());
	return 0;
}