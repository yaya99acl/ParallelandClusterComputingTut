/**
 * Calculate the bill for a particularly expensive málàtàng order.
 *
 * == How to compile ==
 *
 *	mpicc -std=c11 -Wall -Werror -O2 -o solution solution.c
 *
 * == How to run ==
 *
 *	mpirun -np P solution
 *
 *  where P is 1, 2, 3, or 4.
 *
 * == The Data ==
 *
 * The data is in two files:
 *  - menu.tsv: item names and prices
 *  - order.bin: the quantity of items purchased.
 *
 * -- menu.tsv format --
 *
 * Each record in menu.tsv is made of three fields:
 *  - index: a numerical ID, starting from zero, incremented for each record
 *  - name: name of the item as one UTF-8 word
 *  - cost: cost of the menu item
 *
 * For the purposes of this exercise, the name can be completely ignored.
 *
 * -- order.bin format --
 *
 * order.bin is a binary file that is the concatenation of two parallel C arrays.
 * The first array is the item IDs:
 *	  int item_ids[N_ENTRIES];
 *
 * Followed by the quantity of that item ordered:
 *	  int quantities[N_ENTRIES];
 *
 * These are parallel arrays, so use them both with the same index to make up
 * one entry, for example:
 *
 *	  int item_id = item_ids[i];
 *	  int quantity = quantities[i];
 *	  int cost = prices[item_id] * quantity;
 *
 * order.bin is created by make-order.c, and must be run on the same
 * machine as the solution file.
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define ROOT_PROCESS 0
#define ITEMS_ON_MENU 20

/* Parsing and storing the menu */
static void parse_menu(int *menu);
/* Parsing and storing the order */
static int parse_order(int ** restrict item, int ** restrict quantity);
/* Utilities */
#define YUAN(centi_rmb) (centi_rmb / 100)
#define FEN(centi_rmb) ((int) (centi_rmb % 100))

int main(int argc, char* argv[]) {
    // 0. Initialize
    int size, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Prices per each item on the menu, in fēn:
    int menu_prices[ITEMS_ON_MENU] = {0};

    // 1. Parse the menu
    if (rank == ROOT_PROCESS) {
        // Parse menu ONLY on root process!
        parse_menu(menu_prices);
    }

    // 2. Share the prices with the other processes
    MPI_Bcast(menu_prices, ITEMS_ON_MENU, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD);

    // 3. Parse the order
    // The order is made of two **parallel arrays**:
    int* all_items = NULL;
    int* all_quantities = NULL;
    int size_of_order = -1;
    if (rank == ROOT_PROCESS) {
        // Parse and allocate memory for the full order **ONLY** on
        // the root process:
        size_of_order = parse_order(&all_items, &all_quantities);
        assert(all_items != NULL);
        assert(all_quantities != NULL);
    }

    // 3.5. Send the order size to all processes.
    MPI_Bcast(&size_of_order, 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD);
    assert(size_of_order >= 0);

    // 4. Allocate memory for the segments
    // **EVERY** node has its own segment arrays!
    const int segment_size = size_of_order / size;
    int *segment_items = calloc(segment_size, sizeof(int));
    int *segment_quantities = calloc(segment_size, sizeof(int));
    assert(segment_items != NULL);
    assert(segment_quantities != NULL);

    // 5. Scatter both arrays from the root node to all other processes
    //    (including the root node itself!)
    MPI_Scatter(
        all_items, segment_size, MPI_INT,
        segment_items, segment_size, MPI_INT,
        ROOT_PROCESS, MPI_COMM_WORLD
    );
    MPI_Scatter(
        all_quantities, segment_size, MPI_INT,
        segment_quantities, segment_size, MPI_INT,
        ROOT_PROCESS, MPI_COMM_WORLD
    );

    // 6. Each process can sum its own segment!
    long cost_per_menu_item[ITEMS_ON_MENU] = {0};
    for (int i = 0; i < segment_size; i++) {
        int item = segment_items[i];
        int qty = segment_quantities[i];
        int price = menu_prices[item];

        cost_per_menu_item[item] += price * qty;
    }

    // 7. Tree-based reduction to gather all partial sums to the root process
    for (int step = 1; step < size; step *= 2) {
        if (rank % (2 * step) == 0) {
            if (rank + step < size) {
                long received[ITEMS_ON_MENU];
                MPI_Recv(received, ITEMS_ON_MENU, MPI_LONG, rank + step, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                for (int i = 0; i < ITEMS_ON_MENU; i++) {
                    cost_per_menu_item[i] += received[i];
                }
            }
        } else {
            MPI_Send(cost_per_menu_item, ITEMS_ON_MENU, MPI_LONG, rank - step, 0, MPI_COMM_WORLD);
            break; // Exit the loop once sent
        }
    }

    // 8. Calculate the grand total on the root node!
    if (rank == ROOT_PROCESS) {
        long grand_total = 0;
        for (int i = 0; i < ITEMS_ON_MENU; i++) {
            grand_total += cost_per_menu_item[i];
        }
        printf("The bill comes out to ¥%ld.%02d\n", YUAN(grand_total), FEN(grand_total));
    }

    // 9. Teardown
    if (rank == ROOT_PROCESS) {
        // Only the root node has the full item and quantity arrays.
        free(all_items);
        free(all_quantities);
    }
    // All nodes have the segment arrays.
    free(segment_items);
    free(segment_quantities);

    MPI_Finalize();
    return 0;
}

static void parse_menu(int *menu) {
    FILE* f = fopen("menu.tsv", "r");
    assert(f != NULL);

    int n_items = 0;
    for (;;) {
        int index;
        // ignore the name; store the prices
        int parsed = fscanf(f, "%d %*s %d", &index, &menu[n_items]);
        if (parsed != 2) {
            // No more items to parse.
            break;
        }

        n_items++;
    }
    assert(n_items == ITEMS_ON_MENU);

    fclose(f);
}

static int parse_order(int ** restrict item, int ** restrict quantity) {
    FILE* f = fopen("order.bin", "rb");

    if (f == NULL) {
        perror("Could not open order.bin");
        fprintf(stderr, "Are you sure you ran ./make-order "
                "in the current working directory?\n");
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    int file_size = ftell(f);
    rewind(f);

    int size_of_order = file_size / sizeof(int) / 2;
    int *all_items = malloc(size_of_order * sizeof(int));
    int *all_quantities = malloc(size_of_order * sizeof(int));
    if ((all_items == NULL) || (all_quantities == NULL)) {
        perror("could not allocate");
        exit(EXIT_FAILURE);
    }

    int num_items = fread(all_items, sizeof(int), size_of_order, f);
    int num_quantities = fread(all_quantities, sizeof(int), size_of_order, f);
    if ((num_items != size_of_order) || (num_quantities != size_of_order)) {
        fprintf(stderr, "Error: Tried to read %d items from the order, "
                "but could only read %d items and %d quantities. "
                "Quiting!\n", size_of_order, num_items, num_quantities);
        exit(EXIT_FAILURE);
    }
    fclose(f);

    // Pass the pointers back.
    *item = all_items;
    *quantity = all_quantities;
    return size_of_order;
}
