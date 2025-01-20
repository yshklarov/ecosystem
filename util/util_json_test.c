#include "util.c"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: json_test <json_filename>\n");
        return 1;
    }
    char const* const filename = argv[1];
    json_data data = {0};
    if (json_read_from_file(filename, &data)) {
        //printf("Successfully parsed JSON file %s. Contents:\n", filename);
        json_data_printf(&data);
        json_data_destroy(&data);
    } else {
        printf("Failed to parse JSON file %s.\n", filename);
        return EXIT_FAILURE;
    }
    /*
    // Test rand_unif().
    rand_init_from_time(&rand_state_global);
    size_t count[6] = {};
    for (int i = 0; i < 6000000; ++i) {
        ++count[rand_unif(1, 6)-1];
    }
    for (int i = 0; i < 6; ++i) {
        printf("%zu ", count[i]);
    }
    printf("\n");
    */
    return EXIT_SUCCESS;
}
