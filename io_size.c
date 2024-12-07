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

// time is in microseconds
long long timey_wimey() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

void io_size_test(const char *file_name, int io_size) {
    int fd;

    // buffer needs space allocated to it
    char *buffer = malloc(io_size);
    if (!buffer) {
        perror("malloc");
        exit(1);
    }

    // writing
    memset(buffer, 0, io_size);

    long long write_start, write_end, write_total;
    long long read_start, read_end, read_total;
    long long bytes_processed;

    // open the file
    fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error opening file for writing");
        free(buffer);
        exit(1);
    }

    // o direct behavior on mac
    if (fcntl(fd, F_NOCACHE, 1) == -1) {
        perror("Error setting F_NOCACHE");
        close(fd);
        free(buffer);
        exit(1);
    }

    // writing
    write_start = timey_wimey();
    for (long long offset = 0; offset < FILE_SIZE; offset += io_size) {
        if (pwrite(fd, buffer, io_size, offset) != io_size) {
            perror("pwrite");
            close(fd);
            free(buffer);
            exit(1);
        }

        // use fsync to ensure data so data makes it physically to disk, not just cache
        if (fsync(fd) == -1) {
            perror("fsync");
            close(fd);
            free(buffer);
            exit(1);
        }
    }
    write_end = timey_wimey();
    write_total = write_end - write_start;

    // reading
    lseek(fd, 0, SEEK_SET);

    bytes_processed = 0;
    read_start = timey_wimey();
    for (long long offset = 0; offset < FILE_SIZE; offset += io_size) {
        if (pread(fd, buffer, io_size, offset) != io_size) {
            perror("pread");
            close(fd);
            free(buffer);
            exit(1);
        }
        bytes_processed += io_size;
    }
    read_end = timey_wimey();
    read_total = read_end - read_start;

    printf("I/O Size: %d bytes\n", io_size);
    printf("Write Throughput: %.2f MB/s\n",
           (double)(FILE_SIZE * 1e6) / (1024 * 1024 * write_total));
    printf("Read Throughput: %.2f MB/s\n",
           (double)(bytes_processed * 1e6) / (1024 * 1024 * read_total));

    // truncating to clear
    if (ftruncate(fd, 0) == -1) {
        perror("ftruncate");
        close(fd);
        free(buffer);
        exit(1);
    }
    close(fd);

    free(buffer);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <io_size> <file_name>\n", argv[0]);
        exit(1);
    }

    // parsing the command line args
    int io_size = atoi(argv[1]);
    const char *file_name = argv[2];

    if (io_size < MIN_IO_SIZE) {
        fprintf(stderr, "Error: I/O size must be at least %d bytes\n", MIN_IO_SIZE);
        exit(1);
    }

    // show io size
    printf("Using I/O size: %d bytes\n", io_size);

    io_size_test(file_name, io_size);

    return 0;
}


