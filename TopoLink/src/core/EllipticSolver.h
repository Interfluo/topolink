#ifndef ELLIPTICSOLVER_H
#define ELLIPTICSOLVER_H

#include <functional>
#include <gp_Pnt.hxx>
#include <vector>

/**
 * @brief Simple iterative elliptic solver (SOR) for grid smoothing.
 */
class EllipticSolver {
public:
  struct Params {
    int iterations = 1000;
    double relaxation = 0.9;
    double bcRelaxation = 0.1;
  };

  /**
   * @brief Smooths a structured grid of points.
   *
   * @param grid 2D grid of points [M+1][N+1]
   * @param isFixed 2D grid of flags indicating fixed points
   * @param params Solver parameters
   * @param constraintFunc Optional function to project points back to geometry
   */
  static void smoothGrid(
      std::vector<std::vector<gp_Pnt>> &grid,
      const std::vector<std::vector<bool>> &isFixed, const Params &params,
      std::function<gp_Pnt(int, int, const gp_Pnt &)> constraintFunc = nullptr);

private:
  static void
  iterate(std::vector<std::vector<gp_Pnt>> &grid,
          const std::vector<std::vector<bool>> &isFixed, double omega,
          std::function<gp_Pnt(int, int, const gp_Pnt &)> constraintFunc);
};

#endif // ELLIPTICSOLVER_H
