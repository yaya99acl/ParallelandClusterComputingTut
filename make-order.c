/**
 * Creates a (very large) málàtàng order.
 *
 * == How to compile ==
 *
 *	  gcc -std=c11 -Wall -Werror -O2 -o make-order make-order.c
 *
 * == How to run ==
 *
 * To make a file of the default size:
 *
 *    ./make-order
 *
 * To make a file of a given number of entries:
 *
 *    ./make-order N
 *
 * where N is a positive integer.
 *
 * == The Data ==
 *
 * The binary file is made in such a way that is easy to generate and load
 * into the process.  It is essentially the concatenation of the "order ID"
 * vector and the "quantity" vector, both stored as machine ints.
 *
 * NOTE: a file created on one architecture should not be shared on a
 * different architecture.
 */

/* Feature test macros to provide strdup(3) */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* Factor is a big composite number so that you can experiment with dividing
 * the data into 2, 3, 4, 6, 8, etc. */
#define FACTOR              (120)
#define DEFAULT_SIZE        (FACTOR * 1000)
#define MAX_ORDERS_PER_LINE (16)

#define EX_USAGE			(64)

#define MAX_LINE_LENGTH     (128)
#define MAX_ITEMS_ON_MENU   (256)

static uint64_t items_on_menu = 0;
static char* item_name[MAX_ITEMS_ON_MENU] = { 0 };
static int item_price[MAX_ITEMS_ON_MENU] = { 0 };
static uint64_t total_per_item[MAX_ITEMS_ON_MENU] = { 0 };

void usage_error(const char *message);
void read_menu();
void write_order(size_t how_large);

/* Utilties */
#define YUAN(centi_rmb) (centi_rmb / 100)
#define FEN(centi_rmb) ((int) (centi_rmb % 100))


int main(int argc, const char *argv[]) {
    uint64_t num_items = DEFAULT_SIZE;
    if (argc > 1) {
        num_items = strtol(argv[1], NULL, 10);
        if (num_items <= 0) {
            fprintf(stderr, "Invalid amount of items: %" PRIu64 "\n", num_items);
            return 1;
        }
    }

    read_menu();
    write_order(num_items);

    printf("Verify totals:\n");
    uint64_t grand_total = 0;
    for (uint64_t i = 0; i < items_on_menu; i++) {
        uint64_t cost = total_per_item[i];
        printf("\t%2" PRIu64 " %20s ¥%6" PRIu64 ".%02d\n", i, item_name[i], YUAN(cost), FEN(cost));
        grand_total += cost;
    }
    printf("Grand total: ¥%" PRIu64 ".%02d\n", YUAN(grand_total), FEN(grand_total));

    return 0;
}

void usage_error(const char *message) {
    fprintf(stderr, "%s\n", message);
    if (errno) {
        perror("Reason");
    }

    exit(EX_USAGE);
}

void read_menu() {
    // Open the menu.tsv file:
    FILE *fp = fopen("menu.tsv", "r");
    if (fp == NULL) {
        usage_error("Could not open menu.tsv");
    }

    char line[MAX_LINE_LENGTH + 1];

    // Read each line
    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        unsigned int index, price;
        char name[MAX_LINE_LENGTH + 1];
        sscanf(line, "%u %s %u", &index, name, &price);
        if (items_on_menu != index) {
            usage_error("Malformed menu.tsv: unexpected index");
        }

        // We will leak memory, because I cannot be bothered:
        item_name[index] = strdup(name);
        item_price[index] = price;
        items_on_menu++;
    }
    // We're done with the menu now.
    fclose(fp);
}


void write_order(size_t n) {
    // Seed the random number generator
    srand(time(NULL));

    int fd = open("order.bin", O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) {
        usage_error("Could not open order.bin for writing");
    }

    // Need to allocate two vectors worth:
    size_t allocation_size = 2 * n * sizeof(int);
    void* mapping = mmap(NULL, allocation_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        usage_error("Could not mmap file");
    }

    // Need to write a byte at the end of the file to make sure that the OS
    // can actually map the memory
    lseek(fd, allocation_size - 1, SEEK_SET);
    ssize_t bytes_written = write(fd, "", 1);
    assert(bytes_written == 1);

    // Now just pretend that those are distinct memory regions:
    int *item_ids = (int *) mapping;
    int *quantities = ((int *) mapping) + n;

    // Now generate the order:
    for (size_t i = 0; i < n; i++) {
        int item_id = rand() % items_on_menu;
        int quantity = 1 + (rand() % (MAX_ORDERS_PER_LINE - 1));
        item_ids[i] = item_id;
        quantities[i] = quantity;

        // Calculate the cost to double-check MPI program:
        total_per_item[item_id] += item_price[item_id] * quantity;
    }

    munmap(mapping, allocation_size);
    close(fd);
}
