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
    void *shm_ptr;
    unsigned char *data;
    int test_count = 0;
    int shm_fd;

    // Wait for shared memory to be created
    printf("Reader: Waiting for shared memory...\n");

    // Check if IVSHMEM_GRAMINE_VM environment variable is set to 1
    char* ivshmem_env = getenv("IVSHMEM_GRAMINE_VM");
    if (ivshmem_env != NULL) {
        if (strcmp(ivshmem_env, "1") == 0) {
            printf("Reader: IVSHMEM_GRAMINE_VM is set to 1\n");
            printf("Reader: Running in Gramine VM environment\n");
        } else {
            printf("Reader: IVSHMEM_GRAMINE_VM is set to '%s' (not 1)\n", ivshmem_env);
        }

        shm_ptr = (void*) 0xc4000000;
    } else {
        printf("Reader: IVSHMEM_GRAMINE_VM environment variable is not set\n");
        printf("Reader: Running in normal environment\n");

        while ((shm_fd = shm_open(SHM_NAME, O_RDWR, 0)) == -1) {
            if (errno == ENOENT) {
                usleep(100000); // Sleep 100ms
            } else {
                perror("shm_open");
                exit(1);
            }
        }
        
        // Map the shared memory
        shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }    
    }

    data = (unsigned char *)shm_ptr;
    
    printf("Reader: Connected to shared memory. Starting tests...\n");
    
    while (test_count < NUM_TESTS) {
        printf("Reader: Waiting for test %d data...\n", test_count + 1);
        
        // Wait for data to be available (first byte not 0, except for completion signal)
        while (data[0] == 0) {
            usleep(100000); // Sleep 100ms
        }
        
        // Check for completion signal
        if (data[0] == 0xFF) {
            break;
        }
        
        test_count++;
        printf("Reader: Test %d - Reading data...\n", test_count);
        
        // Verify data integrity by calculating checksum or checking pattern
        unsigned long long sum = 0;
        for (size_t i = 0; i < SHM_SIZE; i++) {
            sum += data[i];
        }
        
        printf("Reader: Test %d - Data read successfully. Checksum: 0x%016llx\n", test_count, sum);
        
        // Signal that we're done reading (set first byte to 0)
        data[0] = 0;
    }
    
    printf("Reader: All tests completed.\n");
    
    // Cleanup
    munmap(shm_ptr, SHM_SIZE);
    if (!ivshmem_env) 
      close(shm_fd);
    
    // Unlink the shared memory object
    shm_unlink(SHM_NAME);
    
    return 0;
}
