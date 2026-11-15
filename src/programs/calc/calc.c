#include <mydos.h>

static void ui_calc_border(int y) {
    print("================================================================================", BLUE, 0, y);
}

static void ui_calc_row(int y, const char *text, unsigned char color) {
    char row[77];
    int i = 0;

    while (i < 76 && text[i] != '\0') {
        row[i] = text[i];
        i++;
    }
    while (i < 76) {
        row[i++] = ' ';
    }
    row[76] = '\0';

    print("||", LIGHT_CYAN, 0, y);
    print(row, color, 2, y);
    print("||", LIGHT_CYAN, 78, y);
}

void ui_calc(void) {
    clear_screen();

    ui_calc_border(0);
    ui_calc_row(1, "  CALCULATOR     F1 | Exit", YELLOW);
    ui_calc_border(2);
    ui_calc_row(3, " Decimals and + - * /; sqrt(n), exp(exponent, number). Enter=eval.", WHITE);
    ui_calc_border(4);

    ui_calc_row(5, " > ", WHITE);
    for (int y = 6; y < 24; y++) {
        ui_calc_row(y, "", LIGHT_GREY);
    }
    ui_calc_border(24);
    move_cursor(5, 5);
}

#define CALC_INPUT_MAX 72
#define CALC_VALUE_MAX 2147483647.0
#define CALC_VALUE_MIN (-2147483648.0)

typedef enum {
    CALC_OK,
    CALC_INVALID_EXPRESSION,
    CALC_DIVIDE_BY_ZERO,
    CALC_NEGATIVE_SQRT,
    CALC_INVALID_EXPONENT,
    CALC_OVERFLOW
} calc_status;

static const char *parse_position;
static calc_status parse_status;

static void skip_spaces(void) {
    while (*parse_position == ' ' || *parse_position == '\t') {
        parse_position++;
    }
}

static int consume(char expected) {
    skip_spaces();
    if (*parse_position != expected) return 0;
    parse_position++;
    return 1;
}

static int consume_word(const char *word) {
    int i = 0;
    skip_spaces();
    while (word[i] != '\0' && parse_position[i] == word[i]) i++;
    if (word[i] != '\0') return 0;
    parse_position += i;
    return 1;
}

static int parse_expression(double *value);

static int value_in_range(double value) {
    return value >= CALC_VALUE_MIN && value <= CALC_VALUE_MAX;
}

static double square_root(double number) {
    double guess = number >= 1.0 ? number : 1.0;
    for (int i = 0; i < 40; i++) {
        double next = (guess + number / guess) / 2.0;
        if (next == guess) break;
        guess = next;
    }
    return guess;
}

static int parse_primary(double *value) {
    skip_spaces();

    if (consume_word("sqrt")) {
        double argument;
        if (!consume('(') || !parse_expression(&argument) || !consume(')')) {
            return 0;
        }
        if (argument < 0) {
            parse_status = CALC_NEGATIVE_SQRT;
            return 0;
        }
        *value = square_root(argument);
        return 1;
    }

    if (consume_word("exp")) {
        double exponent_value;
        double base;
        double result = 1.0;
        double factor;
        int exponent;
        if (!consume('(') || !parse_expression(&exponent_value) || !consume(',') ||
            !parse_expression(&base) || !consume(')')) {
            return 0;
        }
        if (exponent_value < 0.0 || exponent_value > CALC_VALUE_MAX ||
            exponent_value != (int)exponent_value) {
            parse_status = CALC_INVALID_EXPONENT;
            return 0;
        }
        exponent = (int)exponent_value;
        factor = base;
        while (exponent > 0) {
            if (exponent & 1) {
                result *= factor;
                if (!value_in_range(result)) {
                    parse_status = CALC_OVERFLOW;
                    return 0;
                }
            }
            exponent >>= 1;
            if (exponent > 0) {
                factor *= factor;
                if (!value_in_range(factor)) {
                    parse_status = CALC_OVERFLOW;
                    return 0;
                }
            }
        }
        *value = result;
        return 1;
    }

    if (consume('(')) {
        if (!parse_expression(value) || !consume(')')) return 0;
        return 1;
    }

    skip_spaces();
    if ((*parse_position >= '0' && *parse_position <= '9') ||
        *parse_position == '.') {
        double number = 0.0;
        double place = 0.1;
        int digits = 0;
        while (*parse_position >= '0' && *parse_position <= '9') {
            number = number * 10.0 + (*parse_position - '0');
            if (number > CALC_VALUE_MAX) {
                parse_status = CALC_OVERFLOW;
                return 0;
            }
            parse_position++;
            digits++;
        }
        if (*parse_position == '.') {
            parse_position++;
            while (*parse_position >= '0' && *parse_position <= '9') {
                number += (*parse_position - '0') * place;
                place *= 0.1;
                parse_position++;
                digits++;
            }
        }
        if (digits == 0 || number > CALC_VALUE_MAX) {
            parse_status = number > CALC_VALUE_MAX ? CALC_OVERFLOW :
                           CALC_INVALID_EXPRESSION;
            return 0;
        }
        *value = number;
        return 1;
    }

    return 0;
}

static int parse_unary(double *value) {
    if (consume('+')) return parse_unary(value);
    if (consume('-')) {
        if (!parse_unary(value)) return 0;
        *value = -*value;
        return 1;
    }
    return parse_primary(value);
}

static int parse_term(double *value) {
    double right;
    if (!parse_unary(value)) return 0;
    for (;;) {
        if (consume('*')) {
            if (!parse_unary(&right)) return 0;
            *value *= right;
            if (!value_in_range(*value)) {
                parse_status = CALC_OVERFLOW;
                return 0;
            }
        } else if (consume('/')) {
            if (!parse_unary(&right)) return 0;
            if (right == 0) {
                parse_status = CALC_DIVIDE_BY_ZERO;
                return 0;
            }
            *value /= right;
            if (!value_in_range(*value)) {
                parse_status = CALC_OVERFLOW;
                return 0;
            }
        } else {
            return 1;
        }
    }
}

static int parse_expression(double *value) {
    double right;
    if (!parse_term(value)) return 0;
    for (;;) {
        if (consume('+')) {
            if (!parse_term(&right)) return 0;
            *value += right;
            if (!value_in_range(*value)) {
                parse_status = CALC_OVERFLOW;
                return 0;
            }
        } else if (consume('-')) {
            if (!parse_term(&right)) return 0;
            *value -= right;
            if (!value_in_range(*value)) {
                parse_status = CALC_OVERFLOW;
                return 0;
            }
        } else {
            return 1;
        }
    }
}

static calc_status evaluate(const char *expression, double *value) {
    parse_position = expression;
    parse_status = CALC_INVALID_EXPRESSION;
    if (!parse_expression(value)) return parse_status;
    skip_spaces();
    if (*parse_position != '\0') return CALC_INVALID_EXPRESSION;
    return CALC_OK;
}

static int allowed_input_char(char key) {
    if ((key >= '0' && key <= '9') || key == '+' || key == '-' ||
        key == '*' || key == '/' || key == '.' || key == '(' || key == ')' ||
        key == ',' || key == ' ' || key == '\t') {
        return 1;
    }
    return key == 's' || key == 'q' || key == 'r' || key == 't' ||
           key == 'e' || key == 'x' || key == 'p';
}

static void format_decimal(double value, char *output) {
    char whole_text[32];
    char fraction_text[7];
    double magnitude = value < 0.0 ? -value : value;
    unsigned int whole = (unsigned int)magnitude;
    unsigned int fraction = (unsigned int)((magnitude - whole) * 1000000.0 + 0.5);
    int position = 0;

    if (value < 0) {
        output[position++] = '-';
    }
    if (fraction >= 1000000u) {
        whole++;
        fraction = 0;
    }
    itoanum(whole, whole_text);
    for (int i = 0; whole_text[i] != '\0'; i++) output[position++] = whole_text[i];

    if (fraction > 0) {
        for (int i = 5; i >= 0; i--) {
            fraction_text[i] = (char)('0' + fraction % 10u);
            fraction /= 10u;
        }
        fraction_text[6] = '\0';
        int end = 6;
        while (end > 0 && fraction_text[end - 1] == '0') end--;
        output[position++] = '.';
        for (int i = 0; i < end; i++) output[position++] = fraction_text[i];
    }
    output[position] = '\0';
}

static void show_evaluation(const char *input, int input_y) {
    double value;
    char result[24];
    int result_y = input_y + 1;
    calc_status status = evaluate(input, &value);
    if (status == CALC_OK) {
        format_decimal(value, result);
        ui_calc_row(result_y, " = ", GREEN);
        print(result, GREEN, 5, result_y);
    } else if (status == CALC_DIVIDE_BY_ZERO) {
        ui_calc_row(result_y, " Can't divide by 0!", RED);
    } else if (status == CALC_NEGATIVE_SQRT) {
        ui_calc_row(result_y, " sqrt requires a non-negative number.", RED);
    } else if (status == CALC_INVALID_EXPONENT) {
        ui_calc_row(result_y, " exp requires a non-negative integer exponent.", RED);
    } else if (status == CALC_OVERFLOW) {
        ui_calc_row(result_y, " Result is outside the supported number range.", RED);
    } else {
        ui_calc_row(result_y, " Invalid expression.", RED);
    }
}

static void scroll_calc_history(void) {
    volatile unsigned short *vga = (volatile unsigned short *)VGA_ADDRESS;
    for (int y = 5; y <= 21; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga[y * VGA_WIDTH + x] = vga[(y + 2) * VGA_WIDTH + x];
        }
    }
    ui_calc_row(22, "", LIGHT_GREY);
    ui_calc_row(23, "", LIGHT_GREY);
}

void main_calc(void) {
    char input[CALC_INPUT_MAX + 1];
    int length = 0;
    int input_y = 5;

    ui_calc();
    input[0] = '\0';
    for (;;) {
        char key = keyboard_getchar();
        if (key == 0) continue;
        if (key == KEY_F1) {
            clear_screen();
            break;
        }
        if (key == '\b') {
            if (length > 0) {
                length--;
                input[length] = '\0';
                print(" ", WHITE, 5 + length, input_y);
                move_cursor(5 + length, input_y);
            }
        } else if (key == '\n' || key == '\r') {
            if (length > 0) {
                if (input_y >= 23) {
                    scroll_calc_history();
                    input_y -= 2;
                }
                show_evaluation(input, input_y);
                input_y += 2;
                ui_calc_row(input_y, " > ", WHITE);
                length = 0;
                input[0] = '\0';
                move_cursor(5, input_y);
            }
        } else if (allowed_input_char(key) && length < CALC_INPUT_MAX) {
            input[length++] = key;
            input[length] = '\0';
            print((char[]){key, '\0'}, WHITE, 5 + length - 1, input_y);
            move_cursor(5 + length, input_y);
        }
    }
}
