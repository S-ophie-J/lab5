// By Sophia Jaselskis

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <string.h>

// 1 gig is the max file size we are writing until
#define FILE_SIZE (1024 * 1024 * 1024)
// 4 kb is the min size
#define MIN_IO_SIZE 4096
// 100 mb is the max size
#define MAX_IO_SIZE (1024 * 1024 * 100)
// largest random offset possible into the file
#define MAX_RANDOM_OFFSET (FILE_SIZE - 1)

// time is in microseconds
long long timey_wimey() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

long long gen_rand(int io_size) {
    // random offset into file
    return rand() % (MAX_RANDOM_OFFSET - io_size);
}

void random_io(const char *file_name, int io_size) {
    int fd;
    char *buffer;

    if (io_size < MIN_IO_SIZE || io_size > MAX_IO_SIZE) {
        fprintf(stderr, "Error: I/O size must be between %d and %d bytes\n", MIN_IO_SIZE, MAX_IO_SIZE);
        exit(1);
    }
    // buffer needs space allocated to it
    buffer = malloc(io_size);
    if (!buffer) {
        perror("malloc");
        exit(1);
    }

    memset(buffer, 0, io_size);

    // store random offsets, keep track of for reading later
    long long *write_offsets = malloc((FILE_SIZE / io_size) * sizeof(long long));
    if (!write_offsets) {
        perror("malloc for write_offsets");
        free(buffer);
        exit(1);
    }

    long long write_start, write_end, write_total;
    long long read_start, read_end, read_total;
    long long bytes_processed;

    // open the file
    fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error opening file for writing");
        free(buffer);
        free(write_offsets);
        exit(1);
    }

    // o direct behavior on mac
    if (fcntl(fd, F_NOCACHE, 1) == -1) {
        perror("Error setting F_NOCACHE");
        close(fd);
        free(buffer);
        exit(1);
    }

    // Write phase: Perform random writes to the file
    write_start = timey_wimey();
    for (long long offset_index = 0; offset_index < FILE_SIZE / io_size; offset_index++) {
        long long random_offset = gen_rand(io_size);
        write_offsets[offset_index] = random_offset;  // Store the offset for future read

        if (pwrite(fd, buffer, io_size, random_offset) != io_size) {
            perror("pwrite");
            close(fd);
            free(buffer);
            free(write_offsets);
            exit(1);
        }

        // Use fsync to ensure data is flushed to disk
        if (fsync(fd) == -1) {
            perror("fsync");
            close(fd);
            free(buffer);
            free(write_offsets);
            exit(1);
        }
    }
    write_end = timey_wimey();
    write_total = write_end - write_start;

    // file offset to beginning of file before starting read time
    lseek(fd, 0, SEEK_SET);
    bytes_processed = 0;
    read_start = timey_wimey();
    for (long long offset_index = 0; offset_index < FILE_SIZE / io_size; offset_index++) {
        long long stored_offset = write_offsets[offset_index];

        if (pread(fd, buffer, io_size, stored_offset) != io_size) {
            perror("pread");
            close(fd);
            free(buffer);
            free(write_offsets);
            exit(1);
        }
        bytes_processed += io_size;
    }
    read_end = timey_wimey();
    read_total = read_end - read_start;  // in microseconds

    // output
    printf("I/O Size: %d bytes\n", io_size);
    printf("Write Throughput: %.2f MB/s\n", (double)(FILE_SIZE * 1e6) / (1024 * 1024 * write_total));
    printf("Read Throughput: %.2f MB/s\n", (double)(bytes_processed * 1e6) / (1024 * 1024 * read_total));

    // truncating to get rid of file contents
    if (ftruncate(fd, 0) == -1) {
        perror("ftruncate");
        close(fd);
        free(buffer);
        free(write_offsets);
        exit(1);
    }

    // Closing fd
    close(fd);

    // free the array and the buffer
    free(buffer);
    free(write_offsets);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <io_size> <file_name>\n", argv[0]);
        exit(1);
    }

    // parsing the command line args
    int io_size = atoi(argv[1]);
    const char *file_name = argv[2];

    // make sure io size is ok
    if (io_size < MIN_IO_SIZE || io_size > MAX_IO_SIZE) {
        fprintf(stderr, "Error: I/O size must be between %d and %d bytes\n", MIN_IO_SIZE, MAX_IO_SIZE);
        exit(1);
    }

    // show io size
    printf("Using I/O size: %d bytes\n", io_size);

    random_io(file_name, io_size);

    return 0;
}
