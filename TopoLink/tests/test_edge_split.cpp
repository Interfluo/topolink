#include "../src/core/Topology.h"
#include <gtest/gtest.h>

// Test edge splitting with parallel propagation in a simple quad mesh
TEST(EdgeSplitTest, ParallelPropagationSingleQuad) {
  Topology topo;

  // Create a simple quad (4 nodes, 4 edges, 1 face)
  //
  //  n1 ---- e1 ---- n2
  //  |               |
  //  e4              e2
  //  |               |
  //  n4 ---- e3 ---- n3
  //

  TopoNode *n1 = topo.createNode(gp_Pnt(0, 1, 0));
  TopoNode *n2 = topo.createNode(gp_Pnt(1, 1, 0));
  TopoNode *n3 = topo.createNode(gp_Pnt(1, 0, 0));
  TopoNode *n4 = topo.createNode(gp_Pnt(0, 0, 0));

  TopoEdge *e1 = topo.createEdge(n1, n2);
  TopoEdge *e2 = topo.createEdge(n2, n3);
  TopoEdge *e3 = topo.createEdge(n3, n4);
  TopoEdge *e4 = topo.createEdge(n4, n1);

  std::vector<TopoEdge *> edges = {e1, e2, e3, e4};
  TopoFace *face = topo.createFace(edges);

  ASSERT_NE(face, nullptr);
  EXPECT_EQ(topo.getEdges().size(), 4);
  EXPECT_EQ(topo.getNodes().size(), 4);

  // Split edge e1 at t=0.5
  TopoNode *newNode = topo.splitEdge(e1->getID(), 0.5);

  ASSERT_NE(newNode, nullptr);

  // After split:
  // - e1 should be deleted and replaced by 2 new edges
  // - Opposite edge e3 should also be split (parallel propagation)
  // - Total: 6 edges (e2, e4 remain; e1->2 edges, e3->2 edges)
  // - Total: 6 nodes (4 original + 2 new from splits)
  EXPECT_EQ(topo.getEdges().size(), 6);
  EXPECT_EQ(topo.getNodes().size(), 6);

  // The face should now have 6 edges (became invalid for quad but demonstrates
  // propagation)
  EXPECT_EQ(face->getEdges().size(), 6);

  // Verify the new node is approximately at the midpoint of the original edge
  gp_Pnt expectedPos(0.5, 1.0, 0.0);
  EXPECT_NEAR(newNode->getPosition().X(), expectedPos.X(), 1e-6);
  EXPECT_NEAR(newNode->getPosition().Y(), expectedPos.Y(), 1e-6);
  EXPECT_NEAR(newNode->getPosition().Z(), expectedPos.Z(), 1e-6);
}

// Test edge splitting across multiple connected quads
TEST(EdgeSplitTest, ParallelPropagationMultipleQuads) {
  Topology topo;

  // Create 2x1 quad mesh (3x2 nodes)
  //
  //  n1 -- e1 -- n2 -- e5 -- n3
  //  |           |            |
  //  e2          e3           e6
  //  |           |            |
  //  n4 -- e4 -- n5 -- e7 -- n6
  //

  TopoNode *n1 = topo.createNode(gp_Pnt(0, 1, 0));
  TopoNode *n2 = topo.createNode(gp_Pnt(1, 1, 0));
  TopoNode *n3 = topo.createNode(gp_Pnt(2, 1, 0));
  TopoNode *n4 = topo.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n5 = topo.createNode(gp_Pnt(1, 0, 0));
  TopoNode *n6 = topo.createNode(gp_Pnt(2, 0, 0));

  TopoEdge *e1 = topo.createEdge(n1, n2);
  TopoEdge *e2 = topo.createEdge(n1, n4);
  TopoEdge *e3 = topo.createEdge(n2, n5);
  TopoEdge *e4 = topo.createEdge(n4, n5);
  TopoEdge *e5 = topo.createEdge(n2, n3);
  TopoEdge *e6 = topo.createEdge(n3, n6);
  TopoEdge *e7 = topo.createEdge(n5, n6);

  // Create two quad faces
  TopoFace *f1 = topo.createFace({e1, e3, e4, e2});
  TopoFace *f2 = topo.createFace({e5, e6, e7, e3});

  ASSERT_NE(f1, nullptr);
  ASSERT_NE(f2, nullptr);
  EXPECT_EQ(topo.getEdges().size(), 7);

  // Split edge e1 at t=0.5
  // This should propagate to e4 (opposite in first quad)
  // And continue to e5 and e7 through shared edges
  TopoNode *newNode = topo.splitEdge(e1->getID(), 0.5);

  ASSERT_NE(newNode, nullptr);

  // After parallel split propagation:
  // - e1, e4, e5, e7 should each be split into 2 edges (8 new edges)
  // - e2, e3, e6 remain (3 edges)
  // - Total: 11 edges
  EXPECT_EQ(topo.getEdges().size(), 11);

  // Should have 4 new nodes from the splits
  EXPECT_EQ(topo.getNodes().size(), 10);
}

// Test edge group preservation during split
TEST(EdgeSplitTest, EdgeGroupPreservation) {
  Topology topo;

  TopoNode *n1 = topo.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topo.createNode(gp_Pnt(1, 0, 0));
  TopoNode *n3 = topo.createNode(gp_Pnt(1, 1, 0));
  TopoNode *n4 = topo.createNode(gp_Pnt(0, 1, 0));

  TopoEdge *e1 = topo.createEdge(n1, n2);
  TopoEdge *e2 = topo.createEdge(n2, n3);
  TopoEdge *e3 = topo.createEdge(n3, n4);
  TopoEdge *e4 = topo.createEdge(n4, n1);

  // Create edge group and add e1
  TopoEdgeGroup *group = topo.createEdgeGroup("test_geometry");
  topo.addEdgeToGroup(group->id, e1);

  EXPECT_EQ(group->edges.size(), 1);
  EXPECT_EQ(group->edges[0], e1);

  // Split e1
  topo.splitEdge(e1->getID(), 0.5);

  // The edge group should now contain 2 edges (the split result)
  EXPECT_EQ(group->edges.size(), 2);
  EXPECT_NE(group->edges[0], e1); // e1 was deleted
  EXPECT_NE(group->edges[1], e1);
}

// Test subdivision inheritance
TEST(EdgeSplitTest, SubdivisionInheritance) {
  Topology topo;

  TopoNode *n1 = topo.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topo.createNode(gp_Pnt(1, 0, 0));

  TopoEdge *edge = topo.createEdge(n1, n2);
  edge->setSubdivisions(10);

  EXPECT_EQ(edge->getSubdivisions(), 10);

  // Split the edge
  topo.splitEdge(edge->getID(), 0.3);

  // Both new edges should inherit the subdivision count
  for (const auto &[id, e] : topo.getEdges()) {
    EXPECT_EQ(e->getSubdivisions(), 10);
  }
}
