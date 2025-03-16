#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 5000

int main(int argc, char **argv)
{
	// Ensure the correct number of arguments are provided
	if (argc != 7)
	{
		fprintf(stderr, "Error: Incorrect number of arguments!\n");
		fprintf(stderr, "You must provide exactly six arguments: \n");
		fprintf(stderr, "1. The path to the first input file.\n");
		fprintf(stderr, "2. The path to the second input file.\n");
		fprintf(stderr, "3. The name to the first reading channel.\n");
		fprintf(stderr, "4. The name to the second reading channel.\n");
		fprintf(stderr, "5. The name to the third writing channel.\n");
		fprintf(stderr, "6. The path to the output file.\n");
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

	// Read the data from the write pipe
	char write_buffer[BUFFER_SIZE];
	int write_channel_descriptor = open(write_channel, O_RDONLY);
	if (write_channel_descriptor < 0)
	{
		fprintf(stderr, "Error: Failed to open write_channel. ");
		perror("open");
		exit(1);
	}

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