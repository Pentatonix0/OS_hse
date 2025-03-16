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

int main(int argc, char **argv)
{
	// Ensure the correct number of arguments are provided
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

	// Assign arguments to variables
	char *source1 = argv[1];	   // Path to the first input file
	char *source2 = argv[2];	   // Path to the second input file
	char *destination = argv[3];   // Path to the output file
	char *read_channel1 = argv[4]; // First reading channel name
	char *read_channel2 = argv[5]; // Second reading channel name
	char *write_channel = argv[6]; // Writing channel name

	// Create named pipes (FIFOs) for inter-process communication
	if (mkfifo(read_channel1, 0666) == -1)
	{
		fprintf(stderr, "Error: Failed to create channel for reading data. ");
		perror("mkfifo");
		exit(1);
	}

	if (mkfifo(read_channel2, 0666) == -1)
	{
		fprintf(stderr, "Error: Failed to create channel for reading data. ");
		perror("mkfifo");
		exit(1);
	}

	if (mkfifo(write_channel, 0666) == -1)
	{
		fprintf(stderr, "Error: Failed to create channel for writing data. ");
		perror("mkfifo");
		exit(1);
	}

	pid_t process_id1 = fork(); // Fork the first child process
	if (process_id1 > 0)		// Parent process (file reading)
	{
		// Open input files for reading
		int file_descriptor1 = open(source1, O_RDONLY);
		int file_descriptor2 = open(source2, O_RDONLY);

		// Handle errors if files can't be opened
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

		char read_buffer1[BUFFER_SIZE]; // Buffer for data from the first file
		char read_buffer2[BUFFER_SIZE]; // Buffer for data from the second file

		// Read data from the files into buffers
		ssize_t bytes_read1 = read(file_descriptor1, read_buffer1, sizeof(read_buffer1));
		ssize_t bytes_read2 = read(file_descriptor2, read_buffer2, sizeof(read_buffer2));

		// Handle errors during file reading
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

		// Close the input files after reading
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

		// Open the pipes for writing
		int read_channel_descriptor1 = open(read_channel1, O_WRONLY);
		int read_channel_descriptor2 = open(read_channel2, O_WRONLY);

		// Handle errors if the channels can't be opened
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

		// Write the data from buffers to the pipes
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

		// Close the write ends of the pipes after writing
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

		waitpid(process_id1, NULL, WNOHANG); // Wait for the child process to finish

		// Read the data from the write pipe
		char write_buffer[BUFFER_SIZE];
		int write_channel_descriptor = open(write_channel, O_RDONLY);

		ssize_t bytes_read = read(write_channel_descriptor, write_buffer, sizeof(write_buffer));

		// Handle errors during pipe reading
		if (bytes_read < 0)
		{
			fprintf(stderr, "Error: Failed to read write_channel. ");
			perror("read");
			close(write_channel_descriptor);
			exit(1);
		}

		// Close the write channel after reading
		if (close(write_channel_descriptor) < 0)
		{
			fprintf(stderr, "Error: Failed to close write_channel. ");
			perror("close");
			exit(1);
		}

		// Open the output file for writing
		int file_descriptor = creat(destination, 0666);
		if (file_descriptor < 0)
		{
			fprintf(stderr, "Error: Failed to create a file. ");
			perror("create");
			exit(1);
		}

		// Write the data to the output file
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
		exit(0);
	}
	else if (process_id1 == 0) // Child process (finding common symbols)
	{
		char buffer1[BUFFER_SIZE];
		char buffer2[BUFFER_SIZE];

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

		// Read the data from the channels into buffers
		ssize_t bytes_read1 = read(read_channel_descriptor1, buffer1, sizeof(buffer1));
		ssize_t bytes_read2 = read(read_channel_descriptor2, buffer2, sizeof(buffer2));

		// Handle errors during channel reading
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

		// Find the common symbols between the two buffers
		char *common = intersect(buffer1, buffer2);
		if (common == NULL)
		{
			fprintf(stderr, "Error: Failed to find the intersection of the strings. ");
			perror("intersect");
			exit(1);
		}

		// Open the write channel for writing the result
		int write_channel_descriptor = open(write_channel, O_WRONLY);
		if (write(write_channel_descriptor, common, strlen(common)) < 0)
		{
			fprintf(stderr, "Error: Failed to write data to write_channel. ");
			perror("write");
			close(write_channel_descriptor);
			exit(1);
		}

		// Close the write channel after writing
		if (close(write_channel_descriptor) < 0)
		{
			fprintf(stderr, "Error: Failed to close write_channel. ");
			perror("close");
			exit(1);
		}
	}

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
	return 0; // End of program
}
