#include "GraphSolver.h"
#include <cmath>
#include <gp_XYZ.hxx>

std::vector<double> GraphSolver::smoothGraph(
    std::vector<Node> &nodes, const Params &params,
    std::function<gp_Pnt(int, const gp_Pnt &)> constraintFunc,
    std::function<void(int, double)> progressFunc) {

  std::vector<double> convergence;
  if (nodes.empty())
    return convergence;

  convergence.reserve(params.iterations);
  int N = nodes.size();

  // Double buffering for positions to avoid order-dependency bias
  // (Jacobi-style) or use Gauss-Seidel. Let's use Gauss-Seidel for faster
  // convergence, same as EllipticSolver.

  for (int it = 0; it < params.iterations; ++it) {
    double maxDisplacement = 0.0;

    for (int i = 0; i < N; ++i) {
      if (nodes[i].isFixed)
        continue;

      const auto &neighbors = nodes[i].neighbors;
      if (neighbors.empty())
        continue;

      gp_XYZ sum(0, 0, 0);
      for (int neighborIdx : neighbors) {
        sum += nodes[neighborIdx].pos.XYZ();
      }

      gp_Pnt oldPnt = nodes[i].pos;
      gp_Pnt target(sum / (double)neighbors.size());

      // Relaxation
      gp_XYZ newVal = oldPnt.XYZ() * (1.0 - params.relaxation) +
                      target.XYZ() * params.relaxation;
      gp_Pnt newPnt(newVal);

      // Constraint Projection
      if (constraintFunc) {
        newPnt = constraintFunc(i, newPnt);
      }

      // Update in place (Gauss-Seidel)
      nodes[i].pos = newPnt;

      double distSq = oldPnt.SquareDistance(newPnt);
      if (distSq > maxDisplacement) {
        maxDisplacement = distSq;
      }
    }

    double maxDist = std::sqrt(maxDisplacement);
    convergence.push_back(maxDist);

    if (progressFunc) {
      progressFunc(it, maxDist);
    }

    if (maxDist < 1e-9) // Converged
      break;
  }
  return convergence;
}
