#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdbool.h>

#define INIT_BUF_SIZE 128
#define CHUNK_SIZE 128

// Function to find the common symbols between two strings
char *intersect(const char *str1, const char *str2)
{
	bool chars1[256] = {false}; // Array to mark characters from the first string
	bool chars2[256] = {false}; // Array to mark characters from the second string

	// Mark the characters in the first string
	for (const char *c = str1; *c; c++)
	{
		chars1[(unsigned char)*c] = true;
	}

	// Mark the characters in the second string
	for (const char *c = str2; *c; c++)
	{
		chars2[(unsigned char)*c] = true;
	}

	static char result[512]; // Static buffer to hold the result
	int index = 0;

	const char *prefix = "common symbols:"; // Prefix to indicate common symbols
	strcpy(result, prefix);					// Copy the prefix to the result
	index = strlen(prefix);

	// Find the common characters between both strings
	for (int i = 0; i < 256; i++)
	{
		if (chars1[i] && chars2[i]) // If character is common in both
		{
			result[index++] = (char)i;
		}
	}
	result[index] = '\0'; // Null-terminate the result

	return result; // Return the result string with common symbols
}

char *read_from_fd(int fd)
{
	size_t buf_size = INIT_BUF_SIZE;
	char *data = malloc(buf_size);
	if (!data)
	{
		perror("malloc");
		exit(1);
	}

	size_t length = 0;
	ssize_t bytes_read;
	bool found_null = false;

	while (!found_null && (bytes_read = read(fd, data + length, buf_size - length)))
	{
		if (bytes_read == -1)
		{
			perror("read");
			free(data);
			exit(1);
		}

		char *null_pos = memchr(data + length, '\0', bytes_read);
		if (null_pos)
		{
			length = null_pos - data + 1;
			found_null = true;
		}
		else
		{
			length += bytes_read;
		}

		if (length >= buf_size - 1 && !found_null)
		{
			buf_size *= 2;
			char *tmp = realloc(data, buf_size);
			if (!tmp)
			{
				perror("realloc");
				free(data);
				exit(1);
			}
			data = tmp;
		}
	}

	if (length == 0)
	{
		free(data);
		return NULL;
	}

	data[length] = '\0';
	return data;
}

void write_to_fd(int fd, const char *data, size_t length)
{
	size_t offset = 0;

	while (offset < length)
	{
		size_t bytes_to_write = length - offset;
		bytes_to_write = bytes_to_write > CHUNK_SIZE ? CHUNK_SIZE : bytes_to_write;

		ssize_t bytes_written = write(fd, data + offset, bytes_to_write);

		if (bytes_written == -1)
		{
			perror("write");
			exit(1);
		}

		offset += bytes_written;
	}
}

int main(int argc, char **argv)
{
	// Ensure the correct number of arguments are provided
	if (argc != 4)
	{
		fprintf(stderr, "Error: Incorrect number of arguments!\n");
		fprintf(stderr, "You must provide exactly three arguments: \n");
		fprintf(stderr, "1. The name to the first reading channel.\n");
		fprintf(stderr, "2. The name to the second reading channel.\n");
		fprintf(stderr, "3. The name to the third writing channel.\n");
		exit(1);
	}

	char *read_channel1 = argv[1]; // First reading channel name
	char *read_channel2 = argv[2]; // Second reading channel name
	char *write_channel = argv[3]; // Writing channel name

	// Open the read channels for reading
	int read_channel_descriptor1 = open(read_channel1, O_RDONLY);
	int read_channel_descriptor2 = open(read_channel2, O_RDONLY);

	// Handle errors if channels can't be opened
	if (read_channel_descriptor1 < 0)
	{
		fprintf(stderr, "Error: Failed to open read_channel1. ");
		perror("open");
		exit(1);
	}

	if (read_channel_descriptor2 < 0)
	{
		fprintf(stderr, "Error: Failed to open read_channel2. ");
		perror("open");
		exit(1);
	}

	char *str1 = read_from_fd(read_channel_descriptor1);
	char *str2 = read_from_fd(read_channel_descriptor2);

	// Close the read channels after reading
	if (close(read_channel_descriptor1) < 0)
	{
		fprintf(stderr, "Error: Failed to close read_channel1. ");
		perror("close");
		exit(1);
	}

	if (close(read_channel_descriptor2) < 0)
	{
		fprintf(stderr, "Error: Failed to close read_channel2. ");
		perror("close");
		exit(1);
	}

	// Open the write channel for writing the result
	int write_channel_descriptor = open(write_channel, O_WRONLY);

	char *result = intersect(str1, str2);
	write_to_fd(write_channel_descriptor, result, strlen(result) + 1); // +1 для нуль-терминатора

	// Close the write channel after writing
	if (close(write_channel_descriptor) < 0)
	{
		fprintf(stderr, "Error: Failed to close write_channel. ");
		perror("close");
		exit(1);
	}
	return 0;
}