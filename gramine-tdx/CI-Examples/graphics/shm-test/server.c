#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define SHM_NAME "/sharedgl_shared_memory"
#define SHM_SIZE (64 * 1024 * 1024)  // 64 MB
#define NUM_TESTS 100

int main() {
    int shm_fd;
    void *shm_ptr;
    unsigned char *data;
    
    // Create shared memory object
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    
    // Set the size of the shared memory
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        exit(1);
    }
    
    // Map the shared memory
    shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    
    data = (unsigned char *)shm_ptr;
    
    // Initialize random seed
    srand(time(NULL));
    
    printf("Writer: Starting tests...\n");
    
    for (int test = 1; test <= NUM_TESTS; test++) {
        printf("Writer: Test %d - Generating random data...\n", test);
        
        // Generate random data
        for (size_t i = 0; i < SHM_SIZE; i++) {
            data[i] = rand() % 256;
        }
        
        printf("Writer: Test %d - Data written. Waiting for reader...\n", test);
        
        // Wait for reader to finish (simple synchronization - wait for first byte to be 0)
        while (data[0] != 0) {
            usleep(100000); // Sleep 100ms
        }
        
        printf("Writer: Test %d completed.\n", test);
    }
    
    // Signal completion
    data[0] = 0xFF;
    
    printf("Writer: All tests completed.\n");
    
    // Cleanup
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    
    return 0;
}
