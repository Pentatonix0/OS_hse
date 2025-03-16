#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define BUFFER_SIZE 5000

#include <stdio.h>
#include <stdbool.h>

// Function to find common characters between two strings
char *intersect(const char *str1, const char *str2)
{
	bool chars1[256] = {false}; // Array to mark characters from the first string
	bool chars2[256] = {false}; // Array to mark characters from the second string

	// Mark the characters from the first string
	for (const char *c = str1; *c; c++)
	{
		chars1[(unsigned char)*c] = true;
	}

	// Mark the characters from the second string
	for (const char *c = str2; *c; c++)
	{
		chars2[(unsigned char)*c] = true;
	}

	// Static buffer with space for the prefix and common characters
	static char result[512];
	int index = 0;

	// Add a prefix indicating the common symbols
	const char *prefix = "common symbols:";
	strcpy(result, prefix);
	index = strlen(prefix);

	// Collect the common characters from both strings
	for (int i = 0; i < 256; i++)
	{
		if (chars1[i] && chars2[i]) // If the character is in both strings
		{
			result[index++] = (char)i; // Add to the result
		}
	}
	result[index] = '\0'; // Null-terminate the result string

	return result; // Return the result containing common characters
}

int main(int argc, char **argv)
{
	// Ensure exactly six arguments are provided
	if (argc != 7)
	{
		fprintf(stderr, "Error: Incorrect number of arguments!\n");
		fprintf(stderr, "You must provide exactly six arguments: \n");
		fprintf(stderr, "1. The path to the first input file.\n");
		fprintf(stderr, "2. The path to the second input file.\n");
		fprintf(stderr, "3. The path to the output file.\n");
		fprintf(stderr, "4. The name to the first reading channel.\n");
		fprintf(stderr, "5. The name to the second reading channel.\n");
		fprintf(stderr, "6. The name to the third writing channel.\n");
		exit(1);
	}

	// Assigning command line arguments to variables
	char *source1 = argv[1];
	char *source2 = argv[2];
	char *destination = argv[3];
	char *read_channel1 = argv[4];
	char *read_channel2 = argv[5];
	char *write_channel = argv[6];

	// Create named pipes (FIFOs) for inter-process communication
	if (mkfifo(read_channel1, 0666) == -1)
	{
		fprintf(stderr, "Error: Failed to create channel for reading/writing data. ");
		perror("mkfifo");
		exit(1);
	}

	if (mkfifo(read_channel2, 0666) == -1)
	{
		fprintf(stderr, "Error: Failed to create channel for reading/writing data. ");
		perror("mkfifo");
		exit(1);
	}

	if (mkfifo(write_channel, 0666) == -1)
	{
		fprintf(stderr, "Error: Failed to create channel for reading/writing data. ");
		perror("mkfifo");
		exit(1);
	}

	// Create the first child process to read data from files
	pid_t process_id1 = fork();
	if (process_id1 > 0) // Parent process that reads data from files
	{
		// Open the input files for reading
		int file_descriptor1 = open(source1, O_RDONLY);
		int file_descriptor2 = open(source2, O_RDONLY);

		if (file_descriptor1 < 0)
		{
			fprintf(stderr, "Error: Failed to open source1. ");
			perror("open");
			exit(1);
		}

		if (file_descriptor2 < 0)
		{
			fprintf(stderr, "Error: Failed to open source2. ");
			perror("open");
			exit(1);
		}

		// Buffers to hold the data read from the files
		char read_buffer1[BUFFER_SIZE];
		char read_buffer2[BUFFER_SIZE];

		// Read data from the files
		ssize_t bytes_read1 = read(file_descriptor1, read_buffer1, sizeof(read_buffer1));
		ssize_t bytes_read2 = read(file_descriptor2, read_buffer2, sizeof(read_buffer2));

		if (bytes_read1 < 0)
		{
			fprintf(stderr, "Error: Failed to read source1. ");
			perror("read");
			close(file_descriptor1);
			exit(1);
		}

		if (bytes_read2 < 0)
		{
			fprintf(stderr, "Error: Failed to read source2. ");
			perror("read");
			close(file_descriptor2);
			exit(1);
		}

		// Close file descriptors
		if (close(file_descriptor1) < 0)
		{
			fprintf(stderr, "Error: Failed to close source1. ");
			perror("close");
			exit(1);
		}

		if (close(file_descriptor2) < 0)
		{
			fprintf(stderr, "Error: Failed to close source2. ");
			perror("close");
			exit(1);
		}

		// Open the named pipes for writing
		int read_channel_descriptor1 = open(read_channel1, O_WRONLY);
		int read_channel_descriptor2 = open(read_channel2, O_WRONLY);

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

		// Write data to the pipes
		if (write(read_channel_descriptor1, read_buffer1, bytes_read1) < 0)
		{
			fprintf(stderr, "Error: Failed to write data to read_channel1. ");
			perror("write");
			close(read_channel_descriptor1);
			exit(1);
		}

		if (write(read_channel_descriptor2, read_buffer2, bytes_read2) < 0)
		{
			fprintf(stderr, "Error: Failed to write data to read_channel2. ");
			perror("write");
			close(read_channel_descriptor2);
			exit(1);
		}

		// Close the write ends of the pipes
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
	}
	else if (process_id1 == 0) // Child process for intersecting the data
	{
		// Fork another process to handle the intersection task
		pid_t process_id2 = fork();
		if (process_id2 > 0)
		{ // Second process to handle reading data from the pipes
			char buffer1[BUFFER_SIZE];
			char buffer2[BUFFER_SIZE];

			// Open the read ends of the pipes
			int read_channel_descriptor1 = open(read_channel1, O_RDONLY);
			int read_channel_descriptor2 = open(read_channel2, O_RDONLY);

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

			// Read data from the pipes
			ssize_t bytes_read1 = read(read_channel_descriptor1, buffer1, sizeof(buffer1));
			ssize_t bytes_read2 = read(read_channel_descriptor2, buffer2, sizeof(buffer2));

			if (bytes_read1 < 0)
			{
				fprintf(stderr, "Error: Failed to read read_channel1. ");
				perror("read");
				close(read_channel_descriptor1);
				exit(1);
			}

			if (bytes_read2 < 0)
			{
				fprintf(stderr, "Error: Failed to read read_channel2. ");
				perror("read");
				close(read_channel_descriptor2);
				exit(1);
			}

			// Close the read ends of the pipes
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

			// Find the common characters between the two buffers
			char *common = intersect(buffer1, buffer2);
			if (common == NULL)
			{
				fprintf(stderr, "Error: Failed to find the intersection of the strings. ");
				perror("intersect");
				exit(1);
			}

			// Open the write pipe to write the common data
			int write_channel_descriptor = open(write_channel, O_WRONLY);
			if (write(write_channel_descriptor, common, strlen(common)) < 0)
			{
				fprintf(stderr, "Error: Failed to write data to write_channel. ");
				perror("write");
				close(write_channel_descriptor);
				exit(1);
			}

			// Close the write end of the pipe
			if (close(write_channel_descriptor) < 0)
			{
				fprintf(stderr, "Error: Failed to close write_channel. ");
				perror("close");
				exit(1);
			}

			wait(NULL); // Wait for the third process to complete
			exit(0);
		}
		else if (process_id2 == 0) // Third process to write data to the output file
		{
			// Buffer to hold data to be written to the file
			char write_buffer[BUFFER_SIZE];

			// Open the read end of the write channel
			int write_channel_descriptor = open(write_channel, O_RDONLY);

			ssize_t bytes_read = read(write_channel_descriptor, write_buffer, sizeof(write_buffer));

			if (bytes_read < 0)
			{
				fprintf(stderr, "Error: Failed to read write_channel. ");
				perror("read");
				close(write_channel_descriptor);
				exit(1);
			}

			// Close the read end of the write channel
			if (close(write_channel_descriptor) < 0)
			{
				fprintf(stderr, "Error: Failed to close write_channel. ");
				perror("close");
				exit(1);
			}

			// Open the output file to write the common data
			int file_descriptor = creat(destination, 0666);
			if (file_descriptor < 0)
			{
				fprintf(stderr, "Error: Failed to create a file. ");
				perror("create");
				exit(1);
			}

			if (write(file_descriptor, write_buffer, bytes_read) < 0)
			{
				fprintf(stderr, "Error: Failed to write data to file. ");
				perror("write");
				close(file_descriptor);
				exit(1);
			}

			// Close the output file
			if (close(file_descriptor) < 0)
			{
				fprintf(stderr, "Error: Failed to close the file. ");
				perror("close");
				exit(1);
			}
			exit(0);
		}
		else
		{
			fprintf(stderr, "Error: Failed to fork the process. ");
			perror("fork");
			exit(1);
		}
	}
	else
	{
		fprintf(stderr, "Error: Failed to create a new process using fork(). ");
		perror("fork");
		exit(1);
	}

	wait(NULL); // Wait for child processes to finish

	// Clean up: Remove the named pipes
	if (unlink(read_channel1) == -1)
	{
		fprintf(stderr, "Error: Failed to unlink the channel ");
		perror("unlink");
		exit(1);
	}

	if (unlink(read_channel2) == -1)
	{
		fprintf(stderr, "Error: Failed to unlink the channel ");
		perror("unlink");
		exit(1);
	}

	if (unlink(write_channel) == -1)
	{
		fprintf(stderr, "Error: Failed to unlink the channel ");
		perror("unlink");
		exit(1);
	}

	return 0;
}
