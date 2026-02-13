#ifndef TOPONODE_H
#define TOPONODE_H

#include <gp_Pnt.hxx>
#include <map>
#include <string>

// Forward declaration
struct TopoHalfEdge;

class TopoNode {
public:
  enum class NodeFreedom {
    LOCKED,        // Pinned to GeoNode
    SLIDING_CURVE, // Pinned to GeoEdge (t-value)
    SLIDING_SURF,  // Pinned to GeoFace (u,v-value)
    FREE           // 3D Space
  };

  TopoNode(int id, const gp_Pnt &position);
  ~TopoNode();

  int getID() const;
  const gp_Pnt &getPosition() const;
  void setPosition(const gp_Pnt &position);

  // Half-Edge Connectivity
  TopoHalfEdge *getOut() const;
  void setOut(TopoHalfEdge *out);

  // Constraint Status
  void setFreedom(NodeFreedom freedom);
  NodeFreedom getFreedom() const;

  void setNormalizedUV(double u, double v = 0.0);
  double getU() const;
  double getV() const;

  // Legacy/Compatibility Constraint Setters (Mapping to new system)
  void setConstraintTargetID(const std::string &targetID);
  std::string getConstraintTargetID() const;

  // Metadata
  void setMetadata(const std::string &key, const std::string &value);
  std::string getMetadata(const std::string &key) const;
  bool hasMetadata(const std::string &key) const;

private:
  int _id;
  gp_Pnt _position;

  TopoHalfEdge *_out; // Outgoing half-edge

  NodeFreedom _freedom;
  std::string _constraintTargetID; // ID of the geometry element
  double _u, _v;                   // Normalized coordinates

  std::map<std::string, std::string> _metadata;
};

#endif // TOPONODE_H
