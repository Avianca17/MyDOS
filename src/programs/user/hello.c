#include <lib/user_api.h>

int main(int argc, char **argv, const mydos_user_api_t *api) {
    int row = 3;
    api->print("Hello from a MyDOS user binary!", 15, 0, row);
    api->scroll(&row);
    api->print("argc=", 7, 0, row);
    char digit[2];
    digit[0] = (char)('0' + argc);
    digit[1] = '\0';
    api->print(digit, 15, 5, row);
    api->scroll(&row);
    if (argc > 1) {
        api->print(argv[1], 7, 0, row);
        api->scroll(&row);
    }
    return 0;
}
