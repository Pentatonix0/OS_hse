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
	bool chars1[256] = {false};
	bool chars2[256] = {false};

	// Mark characters present in the first string
	for (const char *c = str1; *c; c++)
	{
		chars1[(unsigned char)*c] = true;
	}

	// Mark characters present in the second string
	for (const char *c = str2; *c; c++)
	{
		chars2[(unsigned char)*c] = true;
	}

	// Static buffer to store the result
	static char result[512];
	int index = 0;

	// Prefix for the result string
	const char *prefix = "common symbols:";
	strcpy(result, prefix);
	index = strlen(prefix);

	// Collect common characters
	for (int i = 0; i < 256; i++)
	{
		if (chars1[i] && chars2[i])
		{
			result[index++] = (char)i;
		}
	}
	result[index] = '\0';

	return result;
}

int main(int argc, char **argv)
{
	// Check if the correct number of arguments is provided
	if (argc != 4)
	{
		fprintf(stderr, "Error: Incorrect number of arguments!\n");
		fprintf(stderr, "You must provide exactly three arguments: \n");
		fprintf(stderr, "1. The path to the first input file.\n");
		fprintf(stderr, "2. The path to the second input file.\n");
		fprintf(stderr, "3. The name to the first reading channel.\n");
		exit(1);
	}

	// Get file paths and destination
	char *source1 = argv[1];
	char *source2 = argv[2];
	char *destination = argv[3];

	// Declare pipes for communication
	int read_channel1[2];
	int read_channel2[2];
	int write_channel[2];

	// Create pipes for inter-process communication
	if (pipe(read_channel1) == -1)
	{
		fprintf(stderr, "Error: Failed to create pipe for reading/writing data. ");
		perror("pipe");
		exit(1);
	}

	if (pipe(read_channel2) == -1)
	{
		fprintf(stderr, "Error: Failed to create pipe for reading/writing data. ");
		perror("pipe");
		exit(1);
	}

	if (pipe(write_channel) == -1)
	{
		fprintf(stderr, "Error: Failed to create pipe for reading/writing data. ");
		perror("pipe");
		exit(1);
	}

	// Fork a child process to handle file reading and writing
	pid_t process_id1 = fork();
	if (process_id1 > 0) // Parent process (file reading)
	{
		// Open input files for reading
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

		// Buffers to hold data read from files
		char read_buffer1[BUFFER_SIZE];
		char read_buffer2[BUFFER_SIZE];

		// Read data from files
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

		// Close file descriptors after reading
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

		// Close reading ends of the pipes as we are writing to them
		if (close(read_channel1[0]) < 0)
		{
			fprintf(stderr, "Error: Failed to close the read_channel. ");
			perror("close");
			exit(1);
		}

		if (close(read_channel2[0]) < 0)
		{
			fprintf(stderr, "Error: Failed to close the read_channel. ");
			perror("close");
			exit(1);
		}

		// Write data to pipes
		if (write(read_channel1[1], read_buffer1, bytes_read1) < 0)
		{
			fprintf(stderr, "Error: Failed to write data to read_channel. ");
			perror("write");
			close(read_channel1[1]);
			exit(1);
		}

		if (write(read_channel2[1], read_buffer2, bytes_read2) < 0)
		{
			fprintf(stderr, "Error: Failed to write data to read_channel. ");
			perror("write");
			close(read_channel2[1]);
			exit(1);
		}

		// Close write ends of the pipes after writing
		if (close(read_channel1[1]) < 0)
		{
			fprintf(stderr, "Error: Failed to close the read_channel. ");
			perror("close");
			exit(1);
		}

		if (close(read_channel2[1]) < 0)
		{
			fprintf(stderr, "Error: Failed to close the read_channel. ");
			perror("close");
			exit(1);
		}

		// Wait for the child process to finish
		wait(NULL);

		// Read the result from the write channel
		char write_buffer[BUFFER_SIZE];
		if (close(write_channel[1]) < 0)
		{
			fprintf(stderr, "Error: Failed to close the write_channel. ");
			perror("close");
			exit(1);
		}

		ssize_t bytes_read = read(write_channel[0], write_buffer, sizeof(write_buffer));

		if (bytes_read < 0)
		{
			fprintf(stderr, "Error: Failed to read write_channel. ");
			perror("read");
			close(write_channel[0]);
			exit(1);
		}

		// Close the write end of the pipe after reading
		if (close(write_channel[0]) < 0)
		{
			fprintf(stderr, "Error: Failed to close write_channel. ");
			perror("close");
			exit(1);
		}

		// Create the output file and write the result
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

		// Close the output file after writing
		if (close(file_descriptor) < 0)
		{
			fprintf(stderr, "Error: Failed to close the file. ");
			perror("close");
			exit(1);
		}
	}
	else if (process_id1 == 0) // Child process (intersecting strings)
	{
		// Buffers to hold data from pipes
		char buffer1[BUFFER_SIZE];
		char buffer2[BUFFER_SIZE];

		// Read data from the pipes
		ssize_t bytes_read1 = read(read_channel1[0], buffer1, sizeof(buffer1));
		ssize_t bytes_read2 = read(read_channel2[0], buffer2, sizeof(buffer2));

		if (bytes_read1 < 0)
		{
			fprintf(stderr, "Error: Failed to read read_channel1. ");
			perror("read");
			close(read_channel1[0]);
			exit(1);
		}

		if (bytes_read2 < 0)
		{
			fprintf(stderr, "Error: Failed to read read_channel2. ");
			perror("read");
			close(read_channel2[0]);
			exit(1);
		}

		// Close the read ends of the pipes after reading
		if (close(read_channel1[0]) < 0)
		{
			fprintf(stderr, "Error: Failed to close the read_channel. ");
			perror("close");
			exit(1);
		}

		if (close(read_channel2[0]) < 0)
		{
			fprintf(stderr, "Error: Failed to close the read_channel. ");
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

		// Close the unused write end of the pipe
		if (close(write_channel[0]) < 0)
		{
			fprintf(stderr, "Error: Failed to close write_channel. ");
			perror("close");
			exit(1);
		}

		// Write the common characters to the write pipe
		if (write(write_channel[1], common, strlen(common)) < 0)
		{
			fprintf(stderr, "Error: Failed to write data to write_channel. ");
			perror("write");
			close(write_channel[1]);
			exit(1);
		}

		// Close the write end of the pipe after writing
		if (close(write_channel[1]) < 0)
		{
			fprintf(stderr, "Error: Failed to close write_channel. ");
			perror("close");
			exit(1);
		}
	}
	return 0;
}
