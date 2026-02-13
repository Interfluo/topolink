#include "EllipticSolver.h"
#include <cmath>

void EllipticSolver::smoothGrid(
    std::vector<std::vector<gp_Pnt>> &grid,
    const std::vector<std::vector<bool>> &isFixed, const Params &params,
    std::function<gp_Pnt(int, int, const gp_Pnt &)> constraintFunc) {
  if (grid.empty() || grid[0].empty())
    return;

  for (int it = 0; it < params.iterations; ++it) {
    iterate(grid, isFixed, params.relaxation, constraintFunc);
  }
}

void EllipticSolver::iterate(
    std::vector<std::vector<gp_Pnt>> &grid,
    const std::vector<std::vector<bool>> &isFixed, double omega,
    std::function<gp_Pnt(int, int, const gp_Pnt &)> constraintFunc) {
  int M = grid.size() - 1;
  int N = grid[0].size() - 1;

  // Gauss-Seidel with SOR
  for (int i = 0; i <= M; ++i) {
    for (int j = 0; j <= N; ++j) {
      if (isFixed[i][j])
        continue;

      // Average of neighbors
      gp_XYZ sum(0, 0, 0);
      int count = 0;

      if (i > 0) {
        sum += grid[i - 1][j].XYZ();
        count++;
      }
      if (i < M) {
        sum += grid[i + 1][j].XYZ();
        count++;
      }
      if (j > 0) {
        sum += grid[i][j - 1].XYZ();
        count++;
      }
      if (j < N) {
        sum += grid[i][j + 1].XYZ();
        count++;
      }

      if (count > 0) {
        gp_Pnt target(sum / (double)count);
        gp_XYZ newVal = grid[i][j].XYZ() * (1.0 - omega) + target.XYZ() * omega;
        gp_Pnt newPnt(newVal);

        if (constraintFunc) {
          newPnt = constraintFunc(i, j, newPnt);
        }

        grid[i][j] = newPnt;
      }
    }
  }
}
