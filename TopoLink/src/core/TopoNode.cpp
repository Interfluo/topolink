#include "TopoNode.h"

TopoNode::TopoNode(int id, const gp_Pnt &position)
    : _id(id), _position(position), _out(nullptr), _freedom(NodeFreedom::FREE),
      _constraintTargetID(""), _u(0.0), _v(0.0) {}

void TopoNode::setNormalizedUV(double u, double v) {
  _u = u;
  _v = v;
}

double TopoNode::getU() const { return _u; }
double TopoNode::getV() const { return _v; }

TopoNode::~TopoNode() {}

int TopoNode::getID() const { return _id; }

const gp_Pnt &TopoNode::getPosition() const { return _position; }

void TopoNode::setPosition(const gp_Pnt &position) { _position = position; }

TopoHalfEdge *TopoNode::getOut() const { return _out; }

void TopoNode::setOut(TopoHalfEdge *out) { _out = out; }

void TopoNode::setFreedom(NodeFreedom freedom) { _freedom = freedom; }

TopoNode::NodeFreedom TopoNode::getFreedom() const { return _freedom; }

void TopoNode::setConstraintTargetID(const std::string &targetID) {
  _constraintTargetID = targetID;
  // Auto-deduce freedom based on ID presence (basic heuristic)
  if (!targetID.empty()) {
    // This is a rough mapping, ideally the caller sets specific freedom
    // But for legacy partial support we assume logic elsewhere determines type
  }
}

std::string TopoNode::getConstraintTargetID() const {
  return _constraintTargetID;
}

void TopoNode::setMetadata(const std::string &key, const std::string &value) {
  _metadata[key] = value;
}

std::string TopoNode::getMetadata(const std::string &key) const {
  auto it = _metadata.find(key);
  if (it != _metadata.end()) {
    return it->second;
  }
  return "";
}

bool TopoNode::hasMetadata(const std::string &key) const {
  return _metadata.find(key) != _metadata.end();
}
