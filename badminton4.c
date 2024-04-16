#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int token;
    int teammate_rank = (world_rank + 2) % world_size;

    if (world_rank == 0) {
        token = -1;
        // 发送token给下一个进程
        MPI_Send(&token, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("Process %d sent token %d to process %d\n", world_rank, token, 1);
        // 通知队友
        MPI_Send(&token, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD);
        printf("Process %d notified teammate %d\n", world_rank, teammate_rank);
    }

    if (world_rank != 0) {
        // 接收来自前一个进程的token
        MPI_Recv(&token, 1, MPI_INT, world_rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Process %d received token %d from process %d\n", world_rank, token, world_rank - 1);
        // 通知队友
        MPI_Send(&token, 1, MPI_INT, teammate_rank, 0, MPI_COMM_WORLD);
        printf("Process %d notified teammate %d\n", world_rank, teammate_rank);
        // 发送token给下一个进程
        if (world_rank < world_size - 1) {
            MPI_Send(&token, 1, MPI_INT, world_rank + 1, 0, MPI_COMM_WORLD);
            printf("Process %d sent token %d to process %d\n", world_rank, token, world_rank + 1);
        }
    }

    if (world_rank == world_size - 1) {
        // 最后一个进程发送token回到进程0
        MPI_Send(&token, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        printf("Process %d sent token %d to process %d\n", world_rank, token, 0);
    }

    if (world_rank == 0) {
        // 接收来自最后一个进程的token
        MPI_Recv(&token, 1, MPI_INT, world_size - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Process %d received token %d from process %d\n", world_rank, token, world_size - 1);
    }

    MPI_Finalize();
    return 0;
}
//