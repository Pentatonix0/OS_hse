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

	return 0;
}