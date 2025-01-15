#include "util.c"

int main(int argc, [[maybe_unused]] char* argv[argc+1]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: json_test <json_filename>\n");
        return 1;
    }
    char const* const filename = argv[1];
    json_data data = {};
    if (0 == json_read_from_file(filename, &data)) {
        printf("Successfully parsed JSON file %s. Contents:\n", filename);
        json_data_printf(&data);
        json_data_destroy(&data);
    } else {
        printf("Failed to parse JSON file %s.\n", filename);
        return 2;
    }
    return 0;
}

