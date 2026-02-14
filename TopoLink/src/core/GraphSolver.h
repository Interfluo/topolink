#ifndef GRAPHSOLVER_H
#define GRAPHSOLVER_H

#include <functional>
#include <gp_Pnt.hxx>
#include <vector>

/**
 * @brief General iterative solver for graph-based smoothing.
 * Used for smoothing groups of faces where internal edges should be relaxed.
 */
class GraphSolver {
public:
  struct Node {
    gp_Pnt pos;
    bool isFixed;
    std::vector<int> neighbors; // Indices into the nodes vector
  };

  struct Params {
    int iterations = 1000;
    double relaxation = 0.5; // Lower default for graphs to maintain stability
  };

  /**
   * @brief Smooths a general graph of nodes.
   *
   * @param nodes List of graph nodes (positions and connectivity)
   * @param params Solver parameters
   * @param constraintFunc Optional function to project points back to geometry
   * @param progressFunc Optional callback for progress reporting
   * @return std::vector<double> Convergence history (max displacement per
   * iteration)
   */
  static std::vector<double> smoothGraph(
      std::vector<Node> &nodes, const Params &params,
      std::function<gp_Pnt(int, const gp_Pnt &)> constraintFunc = nullptr,
      std::function<void(int, double)> progressFunc = nullptr);
};

#endif // GRAPHSOLVER_H
