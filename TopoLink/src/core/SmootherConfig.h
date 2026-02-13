#ifndef SMOOTHERCONFIG_H
#define SMOOTHERCONFIG_H

struct SmootherConfig {
  int edgeIters = 100;
  double edgeRelax = 0.9;
  double edgeBCRelax = 0.1;

  int faceIters = 1000;
  double faceRelax = 0.9;
  double faceBCRelax = 0.1;

  double singularityRelax = 1.0;
  double growthRateRelax = 1.0;
  int subIters = 1;
  int projFreq = 10;
};

#endif // SMOOTHERCONFIG_H
