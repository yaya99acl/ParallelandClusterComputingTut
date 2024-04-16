#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
  // Initialize the MPI environment
  MPI_Init(NULL, NULL);
  // Find out rank, size
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  int ping_pong_count = 0;
  int next_rank = (world_rank + 1) % world_size; // Next player in the sequence

  while (ping_pong_count < 10) {
    if (world_rank == ping_pong_count % world_size) {
      // Increment the ping pong count before you send it
      ping_pong_count++;
      MPI_Send(&ping_pong_count, 1, MPI_INT, next_rank, 0, MPI_COMM_WORLD);
      printf("Process %d sent and incremented ping_pong_count %d to Process %d\n",
             world_rank, ping_pong_count, next_rank);

      int teammate_rank = (world_rank % 2 == 0) ? world_rank + 1 : world_rank - 1;
      MPI_Send(&ping_pong_count, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD);
      printf("Process %d notified Process %d\n", world_rank, teammate_rank);
    } else {
      int received_count;
      MPI_Recv(&received_count, 1, MPI_INT, (world_rank + world_size - 1) % world_size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      printf("Process %d received ping_pong_count %d from Process %d\n",
             world_rank, received_count, (world_rank + world_size - 1) % world_size);
      ping_pong_count = received_count + 1;

      int teammate_rank = (world_rank % 2 == 0) ? world_rank + 1 : world_rank - 1;
      MPI_Send(&ping_pong_count, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD);
      printf("Process %d notified Process %d\n", world_rank, teammate_rank);
    }
  }
//
  MPI_Finalize();
}
