#include "Topology.h"
#include <gp_Pnt.hxx>
#include <gtest/gtest.h>

class TopoTest : public ::testing::Test {
protected:
  Topology topology;
};

TEST_F(TopoTest, CreateNode) {
  gp_Pnt p1(0, 0, 0);
  TopoNode *node = topology.createNode(p1);

  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->getID(), 1);
  EXPECT_EQ(node->getPosition().X(), 0);
  EXPECT_EQ(node->getPosition().Y(), 0);
  EXPECT_EQ(node->getPosition().Z(), 0);
}

TEST_F(TopoTest, NodeMetadata) {
  gp_Pnt p1(1, 1, 1);
  TopoNode *node = topology.createNode(p1);

  node->setMetadata("type", "anchor");
  EXPECT_TRUE(node->hasMetadata("type"));
  EXPECT_EQ(node->getMetadata("type"), "anchor");
  EXPECT_FALSE(node->hasMetadata("nonexistent"));
}

TEST_F(TopoTest, CreateEdge) {
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));

  TopoEdge *edge = topology.createEdge(n1, n2);

  ASSERT_NE(edge, nullptr);
  EXPECT_EQ(edge->getStartNode(), n1);
  EXPECT_EQ(edge->getEndNode(), n2);
}

TEST_F(TopoTest, CreateFace) {
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNode(gp_Pnt(1, 1, 0));
  TopoNode *n4 = topology.createNode(gp_Pnt(0, 1, 0));

  TopoEdge *e1 = topology.createEdge(n1, n2);
  TopoEdge *e2 = topology.createEdge(n2, n3);
  TopoEdge *e3 = topology.createEdge(n3, n4);
  TopoEdge *e4 = topology.createEdge(n4, n1);

  std::vector<TopoEdge *> edges = {e1, e2, e3, e4};
  TopoFace *face = topology.createFace(edges);

  ASSERT_NE(face, nullptr);
  EXPECT_EQ(face->getEdges().size(), 4);
  EXPECT_EQ(face->getEdges()[0], e1);
}

TEST_F(TopoTest, Grouping) {
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));
  TopoEdge *e1 = topology.createEdge(n1, n2);

  TopoEdgeGroup *group = topology.createEdgeGroup("geo_edge_1");
  ASSERT_NE(group, nullptr);
  EXPECT_EQ(group->geometryID, "geo_edge_1");

  topology.addEdgeToGroup(group->id, e1);
  EXPECT_EQ(group->edges.size(), 1);
  EXPECT_EQ(group->edges[0], e1);
}

TEST_F(TopoTest, Lifecycle) {
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  int id = n1->getID();

  EXPECT_NE(topology.getNode(id), nullptr);
  topology.deleteNode(id);
  EXPECT_EQ(topology.getNode(id), nullptr);
}

TEST_F(TopoTest, EdgeSubdivisions) {
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));
  TopoEdge *edge = topology.createEdge(n1, n2);

  // Default subdivisions should be 11
  EXPECT_EQ(edge->getSubdivisions(), 11);

  // Set and get subdivisions
  edge->setSubdivisions(5);
  EXPECT_EQ(edge->getSubdivisions(), 5);
}

TEST_F(TopoTest, NodeFreedomStatus) {
  TopoNode *node = topology.createNode(gp_Pnt(0, 0, 0));

  // Default freedom should be FREE
  EXPECT_EQ(node->getFreedom(), TopoNode::NodeFreedom::FREE);

  // Set and get freedom
  node->setFreedom(TopoNode::NodeFreedom::LOCKED);
  EXPECT_EQ(node->getFreedom(), TopoNode::NodeFreedom::LOCKED);

  node->setFreedom(TopoNode::NodeFreedom::FREE);
  EXPECT_EQ(node->getFreedom(), TopoNode::NodeFreedom::FREE);
}

TEST_F(TopoTest, UniqueEdgeSubdivisions) {
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNode(gp_Pnt(2, 0, 0));

  TopoEdge *e1 = topology.createEdge(n1, n2);
  TopoEdge *e2 = topology.createEdge(n2, n3);

  // Both edges have default subdivisions (11)
  std::set<int> unique = topology.getUniqueEdgeSubdivisions();
  EXPECT_EQ(unique.size(), 1);
  EXPECT_TRUE(unique.count(11) == 1);

  // Change one edge's subdivisions
  e1->setSubdivisions(5);
  unique = topology.getUniqueEdgeSubdivisions();
  EXPECT_EQ(unique.size(), 2);
  EXPECT_TRUE(unique.count(5) == 1);
  EXPECT_TRUE(unique.count(11) == 1);
}

TEST_F(TopoTest, MergeNodesWithFace) {
  // Create a quad face
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNode(gp_Pnt(1, 1, 0));
  TopoNode *n4 = topology.createNode(gp_Pnt(0, 1, 0));

  TopoEdge *e1 = topology.createEdge(n1, n2);
  TopoEdge *e2 = topology.createEdge(n2, n3);
  TopoEdge *e3 = topology.createEdge(n3, n4);
  TopoEdge *e4 = topology.createEdge(n4, n1);

  TopoFace *face = topology.createFace({e1, e2, e3, e4});
  ASSERT_NE(face, nullptr);
  int faceId = face->getID();

  // Merge n2 into n1
  // This collapses edge e1, which should trigger face deletion
  bool success = topology.mergeNodes(n1->getID(), n2->getID());
  EXPECT_TRUE(success);

  // n2 should be gone
  EXPECT_EQ(topology.getNode(2), nullptr);

  // Face should be deleted
  EXPECT_EQ(topology.getFace(faceId), nullptr);
}

TEST_F(TopoTest, QuadFaceEdgeCollapse) {
  // Create nodes 1, 2, 3, 4
  TopoNode *n1 = topology.createNodeWithID(1, gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNodeWithID(2, gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNodeWithID(3, gp_Pnt(1, 1, 0));
  TopoNode *n4 = topology.createNodeWithID(4, gp_Pnt(0, 1, 0));

  // Edges: 1-2, 2-3, 3-4, 4-1
  TopoEdge *e12 = topology.createEdge(n1, n2);
  TopoEdge *e23 = topology.createEdge(n2, n3);
  TopoEdge *e34 = topology.createEdge(n3, n4);
  TopoEdge *e41 = topology.createEdge(n4, n1);

  TopoFace *face = topology.createFace({e12, e23, e34, e41});
  ASSERT_NE(face, nullptr);
  int faceId = face->getID();

  // Merge Node 1 into Node 2 (Collapses edge 1-2)
  // Expected result:
  // - Node 1 is deleted.
  // - Node 2 remains.
  // - Edge 1-2 becomes 2-2 (self-loop) and is deleted.
  // - Edge 4-1 becomes 4-2.
  // - Nodes remaining: 2, 3, 4 (3 nodes).
  // - Edges remaining: 2-3, 3-4, 4-2 (3 edges).
  // - Face should be deleted.
  bool success = topology.mergeNodes(2, 1);
  EXPECT_TRUE(success);

  // Verification
  EXPECT_EQ(topology.getNodes().size(), 3);
  EXPECT_EQ(topology.getEdges().size(), 3);
  EXPECT_EQ(topology.getFace(faceId), nullptr);
  EXPECT_EQ(topology.getFaces().size(), 0);
}

TEST_F(TopoTest, QuadFaceDiagonalMerge) {
  // Create nodes 1, 2, 3, 4
  TopoNode *n1 = topology.createNodeWithID(1, gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNodeWithID(2, gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNodeWithID(3, gp_Pnt(1, 1, 0));
  TopoNode *n4 = topology.createNodeWithID(4, gp_Pnt(0, 1, 0));

  // Edges: 1-2, 2-3, 3-4, 4-1
  TopoEdge *e12 = topology.createEdge(n1, n2);
  TopoEdge *e23 = topology.createEdge(n2, n3);
  TopoEdge *e34 = topology.createEdge(n3, n4);
  TopoEdge *e41 = topology.createEdge(n4, n1);

  TopoFace *face = topology.createFace({e12, e23, e34, e41});
  ASSERT_NE(face, nullptr);
  int faceId = face->getID();

  // Merge Node 1 into Node 3 (Diagonal merge)
  // Expected result:
  // - Node 1 is deleted.
  // - Node 3 remains.
  // - Edge 1-2 becomes 3-2. This is duplicate of 2-3.
  // - Edge 4-1 becomes 4-3. This is duplicate of 3-4.
  // - Core should merge these duplicates.
  // - Face should be deleted.
  bool success = topology.mergeNodes(3, 1);
  EXPECT_TRUE(success);

  // Verification
  EXPECT_EQ(topology.getNodes().size(), 3);
  EXPECT_EQ(topology.getEdges().size(), 2);
  EXPECT_EQ(topology.getFace(faceId), nullptr);
  EXPECT_EQ(topology.getFaces().size(), 0);
}

TEST_F(TopoTest, QuadFaceOpposingEdgeCollapse) {
  // Create nodes 1, 2, 3, 4
  TopoNode *n1 = topology.createNodeWithID(1, gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNodeWithID(2, gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNodeWithID(3, gp_Pnt(1, 1, 0));
  TopoNode *n4 = topology.createNodeWithID(4, gp_Pnt(0, 1, 0));

  // Edges: 1-2, 2-3, 3-4, 4-1
  TopoEdge *e12 = topology.createEdge(n1, n2);
  TopoEdge *e23 = topology.createEdge(n2, n3);
  TopoEdge *e34 = topology.createEdge(n3, n4);
  TopoEdge *e41 = topology.createEdge(n4, n1);

  TopoFace *face = topology.createFace({e12, e23, e34, e41});
  ASSERT_NE(face, nullptr);
  int faceId = face->getID();

  // "Merging opposing edges" e12 and e34
  // We do this by merging 1 into 4, and 2 into 3.

  // 1. Merge 1 into 4
  bool s1 = topology.mergeNodes(4, 1);
  EXPECT_TRUE(s1);

  // 2. Merge 2 into 3
  bool s2 = topology.mergeNodes(3, 2);
  EXPECT_TRUE(s2);

  // Verification
  // Remaining Nodes should be 3 and 4 (2 nodes)
  EXPECT_EQ(topology.getNodes().size(), 2);
  EXPECT_TRUE(topology.getNode(3) != nullptr);
  EXPECT_TRUE(topology.getNode(4) != nullptr);

  // Remaining Edges should be one: (3,4)
  // e12 became (4,3), e23 became (3,3) deleted, e34 is (3,4), e41 became (4,4)
  // deleted. Then e12 and e34 were merged as duplicates.
  EXPECT_EQ(topology.getEdges().size(), 1);

  // Face should be gone
  EXPECT_EQ(topology.getFace(faceId), nullptr);
  EXPECT_EQ(topology.getFaces().size(), 0);
}

TEST_F(TopoTest, QuadFaceUserExtrusionCollapse) {
  // Mimic extrusion from edge 1-2
  TopoNode *n1 = topology.createNodeWithID(1, gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNodeWithID(2, gp_Pnt(1, 0, 0));
  TopoEdge *e12 = topology.createEdge(n1, n2);

  // New nodes from extrusion: 4 and 5 (User merged 4 into 5)
  TopoNode *n5 = topology.createNodeWithID(5, gp_Pnt(1, 1, 0));
  TopoNode *n4 = topology.createNodeWithID(4, gp_Pnt(0, 1, 0));

  // Face edges: 1-2, 2-5, 5-4, 4-1
  TopoEdge *e25 = topology.createEdge(n2, n5);
  TopoEdge *e54 = topology.createEdge(n5, n4);
  TopoEdge *e41 = topology.createEdge(n4, n1);

  TopoFace *face = topology.createFace({e12, e25, e54, e41});
  ASSERT_NE(face, nullptr);
  int faceId = face->getID();

  // User Action: Merge 4 into 5 (selected 4, hovered 5, pressed M)
  // This collapses edge 5-4.
  bool success = topology.mergeNodes(5, 4);
  EXPECT_TRUE(success);

  // Verification
  // Face should be deleted because e54 became a self-loop (5-5)
  EXPECT_EQ(topology.getFace(faceId), nullptr);
  EXPECT_EQ(topology.getFaces().size(), 0);

  // Remaining nodes: 1, 2, 5 (3 nodes)
  EXPECT_EQ(topology.getNodes().size(), 3);
  // Remaining edges: 1-2, 2-5, 5-1 (3 edges)
  EXPECT_EQ(topology.getEdges().size(), 3);
}

TEST_F(TopoTest, ChainThreeNodesMerge) {
  // Create nodes 1, 2, 3
  TopoNode *n1 = topology.createNodeWithID(1, gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNodeWithID(2, gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNodeWithID(3, gp_Pnt(2, 0, 0));

  // Edges: 1-2, 2-3
  topology.createEdge(n1, n2);
  topology.createEdge(n2, n3);

  // Merge Node 1 into Node 2
  // Edge 1-2 becomes 2-2 (deleted)
  // Edge 2-3 remains
  bool success = topology.mergeNodes(2, 1);
  EXPECT_TRUE(success);

  // Verification
  EXPECT_EQ(topology.getNodes().size(), 2);
  EXPECT_EQ(topology.getEdges().size(), 1);
}

TEST_F(TopoTest, ChainThreeNodesDiagonalMerge) {
  // Create nodes 1, 2, 3
  TopoNode *n1 = topology.createNodeWithID(1, gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNodeWithID(2, gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNodeWithID(3, gp_Pnt(2, 0, 0));

  // Edges: 1-2, 2-3
  topology.createEdge(n1, n2);
  topology.createEdge(n2, n3);

  // Merge Node 1 into Node 3
  // Edge 1-2 becomes 3-2. Edge 2-3 is (2,3).
  // These are duplicates, so they should be merged.
  bool success = topology.mergeNodes(3, 1);
  EXPECT_TRUE(success);

  // Verification
  EXPECT_EQ(topology.getNodes().size(), 2);
  EXPECT_EQ(topology.getEdges().size(), 1);
}

// Level 0: Core Topology Sanity
TEST_F(TopoTest, Topo_HalfEdge_Twin_Integrity) {
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));
  TopoEdge *edge = topology.createEdge(n1, n2);
  topology.createFace({edge}); // Single edge face to trigger HE creation

  TopoHalfEdge *he1 = edge->getForwardHalfEdge();
  TopoHalfEdge *he2 = edge->getBackwardHalfEdge();

  ASSERT_NE(he1, nullptr);
  ASSERT_NE(he2, nullptr);
  EXPECT_EQ(he1->twin, he2);
  EXPECT_EQ(he2->twin, he1);
  EXPECT_EQ(he1->origin, n1);
  EXPECT_EQ(he2->origin, n2);
}

TEST_F(TopoTest, Topo_Face_Winding_Order) {
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNode(gp_Pnt(1, 1, 0));
  TopoNode *n4 = topology.createNode(gp_Pnt(0, 1, 0));

  TopoEdge *e1 = topology.createEdge(n1, n2);
  TopoEdge *e2 = topology.createEdge(n2, n3);
  TopoEdge *e3 = topology.createEdge(n3, n4);
  TopoEdge *e4 = topology.createEdge(n4, n1);

  TopoFace *face = topology.createFace({e1, e2, e3, e4});
  ASSERT_NE(face, nullptr);

  TopoHalfEdge *start = face->getBoundary();
  ASSERT_NE(start, nullptr);

  // Walk next 4 times
  TopoHalfEdge *curr = start;
  for (int i = 0; i < 4; ++i) {
    curr = curr->next;
  }
  EXPECT_EQ(curr, start);

  // Walk prev 4 times
  curr = start;
  for (int i = 0; i < 4; ++i) {
    curr = curr->prev;
  }
  EXPECT_EQ(curr, start);
  EXPECT_EQ(start->prev->next, start);
}

// Level 1: Creation & Extrusion
TEST_F(TopoTest, Create_PullEdge_GeneratesQuad) {
  // Simulate extrusion from Edge E1 (A-B) to create E2 (C-D)
  TopoNode *a = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *b = topology.createNode(gp_Pnt(1, 0, 0));
  TopoEdge *e1 = topology.createEdge(a, b);
  DimensionChord *chord1 = topology.createChord(10);
  e1->setChord(chord1);

  TopoNode *c = topology.createNode(gp_Pnt(0, 1, 0));
  TopoNode *d = topology.createNode(gp_Pnt(1, 1, 0));
  TopoEdge *e2 = topology.createEdge(c, d);
  e2->setChord(chord1); // Shared chord

  TopoEdge *side1 = topology.createEdge(a, c);
  TopoEdge *side2 = topology.createEdge(b, d);
  DimensionChord *chord2 = topology.createChord(5);
  side1->setChord(chord2);
  side2->setChord(chord2);

  TopoFace *face = topology.createFace({e1, side2, e2, side1});
  ASSERT_NE(face, nullptr);

  EXPECT_EQ(e1->getChord(), e2->getChord());
  EXPECT_EQ(side1->getChord(), side2->getChord());
  EXPECT_NE(e1->getChord(), side1->getChord());
}

// Level 2: Dimensioning & Propagation
TEST_F(TopoTest, Dim_Propagate_Linear_Strip) {
  // Create a 1x3 strip of quads
  // (0,0)-(1,0)-(2,0)-(3,0)  Bottom nodes
  // (0,1)-(1,1)-(2,1)-(3,1)  Top nodes
  std::vector<TopoNode *> bottom, top;
  for (int i = 0; i < 4; ++i) {
    bottom.push_back(topology.createNode(gp_Pnt(i, 0, 0)));
    top.push_back(topology.createNode(gp_Pnt(i, 1, 0)));
  }

  std::vector<TopoEdge *> rungs; // vertical
  for (int i = 0; i < 4; ++i) {
    rungs.push_back(topology.createEdge(bottom[i], top[i]));
  }

  DimensionChord *verticalChord = topology.createChord(5);
  for (auto *e : rungs) {
    e->setChord(verticalChord);
  }

  // Change dimension on one rung
  rungs[0]->setSubdivisions(15);

  // Verify propagation to all rungs
  for (auto *e : rungs) {
    EXPECT_EQ(e->getSubdivisions(), 15);
  }
}

// Level 4: Destructive Merging
TEST_F(TopoTest, Merge_Nodes_SharedEdge_Collapse) {
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNode(gp_Pnt(1, 1, 0));

  TopoEdge *e12 = topology.createEdge(n1, n2);
  TopoEdge *e23 = topology.createEdge(n2, n3);
  TopoEdge *e31 = topology.createEdge(n3, n1);

  TopoFace *face = topology.createFace({e12, e23, e31});
  ASSERT_NE(face, nullptr);
  int faceId = face->getID();

  // Merge n1 into n2 (Collapses e12)
  bool success = topology.mergeNodes(n2->getID(), n1->getID());
  EXPECT_TRUE(success);

  // Node n1 should be deleted
  EXPECT_EQ(topology.getNode(n1->getID()), nullptr);

  // Edge e12 should be deleted
  EXPECT_EQ(topology.getEdge(e12->getID()), nullptr);

  // Face should be deleted (degenerate)
  EXPECT_EQ(topology.getFace(faceId), nullptr);

  // Edge e31 should now connect n3 and n2
  EXPECT_EQ(e31->getStartNode(), n3);
  EXPECT_EQ(e31->getEndNode(), n2);
}

// Regression Test for GUI crash: Two quads sharing a node, merging adjacent
// nodes.
TEST_F(TopoTest, Merge_Nodes_SharedNode_Regression) {
  // Quad 1: (0,0)-(1,0)-(1,1)-(0,1)
  TopoNode *n1 = topology.createNode(gp_Pnt(0, 0, 0));
  TopoNode *n2 = topology.createNode(gp_Pnt(1, 0, 0));
  TopoNode *n3 = topology.createNode(gp_Pnt(1, 1, 0));
  TopoNode *n4 = topology.createNode(gp_Pnt(0, 1, 0));
  TopoEdge *e12 = topology.createEdge(n1, n2);
  TopoEdge *e23 = topology.createEdge(n2, n3);
  TopoEdge *e34 = topology.createEdge(n3, n4);
  TopoEdge *e41 = topology.createEdge(n4, n1);
  topology.createFace({e12, e23, e34, e41});

  // Quad 2: (1,1)-(2,1)-(2,2)-(1,2) - Sharing node 3 (shared vertex)
  // Note: shared vertex is n3.
  TopoNode *n5 = topology.createNode(gp_Pnt(2, 1, 0));
  TopoNode *n6 = topology.createNode(gp_Pnt(2, 2, 0));
  TopoNode *n7 = topology.createNode(gp_Pnt(1, 2, 0));
  TopoEdge *e35 = topology.createEdge(n3, n5);
  TopoEdge *e56 = topology.createEdge(n5, n6);
  TopoEdge *e67 = topology.createEdge(n6, n7);
  TopoEdge *e73 = topology.createEdge(n7, n3);
  topology.createFace({e35, e56, e67, e73});

  // Merge n2 into n5. This should rewire edges but NOT crash.
  bool success = topology.mergeNodes(n5->getID(), n2->getID());
  EXPECT_TRUE(success);

  // Verify survivors
  EXPECT_NE(topology.getEdge(n1, n5), nullptr); // e12 now n1-n5
  EXPECT_NE(topology.getEdge(n5, n3),
            nullptr); // e23 now n5-n3 (shared with e35)

  // Rebuild happens internally.
  // Check that we can still create more topology
  TopoNode *n8 = topology.createNode(gp_Pnt(3, 3, 0));
  EXPECT_NE(n8, nullptr);
}
