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

  int token;
  int teammate_rank = (world_rank % 2 == 0) ? world_rank + 2 : world_rank - 2;

  // Receive from the lower process and send to the higher process. Take care
  // of the special case when you are the first process to prevent deadlock.
  if (world_rank != 0) {
    MPI_Recv(&token, 1, MPI_INT, world_rank - 1, 0, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
    printf("Process %d received token %d from process %d\n", world_rank, token,
           world_rank - 1);

    // Notify teammate
    if(teammate_rank >= 0 && teammate_rank < world_size){
      MPI_Send(&token, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD);
      printf("Process %d notified teammate %d\n", world_rank, teammate_rank);
    }

    // Send the token to the next process
    MPI_Send(&token, 1, MPI_INT, (world_rank + 1) % world_size, 0,
             MPI_COMM_WORLD);
    printf("Process %d sent token %d to process %d\n", world_rank, token,
           (world_rank + 1) % world_size);
  } else {
    // Set the token's value if you are process 0
    token = -1;

    // Send the token to the next process
    MPI_Send(&token, 1, MPI_INT, (world_rank + 1) % world_size, 0,
             MPI_COMM_WORLD);
    printf("Process %d sent token %d to process %d\n", world_rank, token,
           (world_rank + 1) % world_size);

    // Notify teammate
    if(teammate_rank >= 0 && teammate_rank < world_size){
      MPI_Send(&token, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD);
      printf("Process %d notified teammate %d\n", world_rank, teammate_rank);
    }
  }
  // Now process 0 can receive from the last process. This makes sure that at
  // least one MPI_Send is initialized before all MPI_Recvs (again, to prevent
  // deadlock)
  if (world_rank == 0) {
    MPI_Recv(&token, 1, MPI_INT, world_size - 1, 0, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
    printf("Process %d received token %d from process %d\n", world_rank, token,
           world_size - 1);

    // Notify teammate
    if(teammate_rank >= 0 && teammate_rank < world_size){
      MPI_Send(&token, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD);
      printf("Process %d notified teammate %d\n", world_rank, teammate_rank);
    }
  }

  MPI_Finalize();
}
