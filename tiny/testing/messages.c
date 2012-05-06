#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  int procID;
  int val;
  int i;
  MPI_Status status;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &procID);
  MPI_Barrier(MPI_COMM_WORLD);
  printf("Made it past barrier 1!\n");
  MPI_Barrier(MPI_COMM_WORLD);
  printf("Made it past barrier 2!\n");

  if (procID == 0)
  {
    val = 4;
    printf("Before send\n");
    MPI_Send(&val, 1, MPI_INT, 1, 4, MPI_COMM_WORLD);
    printf("after send\n");
    for(i = 0; i < 10; i++);
    {
      printf("Waiting...\n");
    }
    printf("Done waiting!\n");
  }

  if (procID == 1)
  {
    val = 1;
    printf("before: val = %d\n", val);
    sleep(6);
    MPI_Recv(&val, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    printf("after: val = %d\n", val);
  }

  MPI_Finalize();
  return 0;
}
