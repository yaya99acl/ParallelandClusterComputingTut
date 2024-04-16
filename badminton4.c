#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
  const int PING_PONG_LIMIT = 10;

  // Initialize the MPI environment
  MPI_Init(NULL, NULL);
  // Find out rank, size
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // We are assuming 4 processes for this task
  if (world_size != 4) {
    fprintf(stderr, "This game requires exactly 4 processes\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  int ping_pong_count = 0;
  int partner_rank = (world_rank + 2) % 4; // Every player has a partner two steps away in the circle
  int teammate_rank = (world_rank + 1) % 2 == 0 ? world_rank + 1 : world_rank - 1; // Teammate is the adjacent player in the opposite team

  while (ping_pong_count < PING_PONG_LIMIT) {
    if (world_rank == ping_pong_count % 4) {
        // Increment the ping pong count before you send it
        ping_pong_count++;
        MPI_Send(&ping_pong_count, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD);
        printf("Player %d sent and incremented ping_pong_count %d to Player %d\n",
            world_rank, ping_pong_count, partner_rank);
        int teammate_count;
        MPI_Recv(&ping_pong_count, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);
        MPI_Recv(&teammate_count, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);
        ping_pong_count = teammate_count; // Update ping_pong_count with teammate's count
        MPI_Send(&ping_pong_count, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD);
        printf("Player %d notified Player %d\n", world_rank, teammate_rank);
    } else {
        MPI_Recv(&ping_pong_count, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);
        printf("Player %d received ping_pong_count %d from Player %d\n",
            world_rank, ping_pong_count, partner_rank);
        int teammate_count;
        MPI_Recv(&teammate_count, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);
        ping_pong_count = teammate_count; // Update ping_pong_count with teammate's count
        MPI_Send(&ping_pong_count, 1, MPI_INT, partner_rank, 0, MPI_COMM_WORLD);
        printf("Player %d notified by Player %d\n", world_rank, teammate_rank);
    }
  }

  MPI_Finalize();
}
