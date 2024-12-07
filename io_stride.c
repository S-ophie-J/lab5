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
// 4 kb is also min stride length
#define MIN_STRIDE 4096
// 100 mb is largest stride length
#define MAX_STRIDE (1024 * 1024 * 100)

// time is in microseconds
long long timey_wimey() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

void io_stride(const char *file_name, int io_size, int stride) {
    int fd;
    char *buffer;

    // make sure the stride is in range
    if (stride < MIN_STRIDE || stride > MAX_STRIDE) {
        fprintf(stderr, "Error: Stride must be between %d and %d bytes\n", MIN_STRIDE, MAX_STRIDE);
        exit(1);
    }

    // buffer needs space allocated to it
    buffer = malloc(io_size);
    if (!buffer) {
        perror("malloc");
        exit(1);
    }
    memset(buffer, 0, io_size);

    long long write_start, write_end, write_total;
    long long read_start, read_end, read_total;
    long long bytes_processed;

    // open file
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

    // write in chunks of io size skipping over size of stride
    write_start = timey_wimey();
    for (long long offset = 0; offset < FILE_SIZE; offset += stride) {
        if (pwrite(fd, buffer, io_size, offset) != io_size) {
            perror("pwrite");
            close(fd);
            free(buffer);
            exit(1);
        }

        // fsync to ensure that the data is physically written to disk
        if (fsync(fd) == -1) {
            perror("fsync");
            close(fd);
            free(buffer);
            exit(1);
        }
    }
    write_end = timey_wimey();
    write_total = write_end - write_start;

    // file offset to beginning of file before starting read time
    lseek(fd, 0, SEEK_SET);
    bytes_processed = 0;
    read_start = timey_wimey();
    for (long long offset = 0; offset < FILE_SIZE; offset += stride) {
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

    // output
    printf("Write Throughput: %.2f MB/s\n",
           (double)(FILE_SIZE * 1e6) / (1024 * 1024 * write_total));
    printf("Read Throughput: %.2f MB/s\n",
           (double)(bytes_processed * 1e6) / (1024 * 1024 * read_total));

    // truncating to get rid of file contents
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
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <io_size> <stride> <file_name>\n", argv[0]);
        exit(1);
    }

    // parsing the command line args
    int io_size = atoi(argv[1]);
    int stride = atoi(argv[2]);
    const char *file_name = argv[3];

    // print io and stride for debugging
    printf("Using I/O size: %d bytes, Stride: %d bytes\n", io_size, stride);

    io_stride(file_name, io_size, stride);

    return 0;
}
