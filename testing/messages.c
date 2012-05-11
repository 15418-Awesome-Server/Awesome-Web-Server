#include <mpi.h>
#include <stdio.h>
#include <sys/time.h>

double gettime()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}


int main(int argc, char *argv[])
{
  int procID;
  double val, val2;
  int i;
  MPI_Status status;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &procID);

  if (procID == 0)
  {
    val = gettime();
    MPI_Send(&val, 1, MPI_DOUBLE, 1, 4, MPI_COMM_WORLD);
    MPI_Recv(&val2, 1, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    val2 = gettime();
    printf("Time: %.7f\n", (val2 - val) / 2);
  }

  if (procID == 1)
  {
    MPI_Recv(&val, 1, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    MPI_Send(&val2, 1, MPI_DOUBLE, 0, 4, MPI_COMM_WORLD);
  }

  MPI_Finalize();
  return 0;
}
