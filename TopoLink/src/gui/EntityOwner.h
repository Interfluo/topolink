#pragma once

#include <Standard_Transient.hxx>

// A simple class that can be used as an "owner" for AIS objects.
// It allows associating a simple integer ID with a selectable object.
class EntityOwner : public Standard_Transient {
public:
  EntityOwner(int id) : m_id(id) {}
  int id() const { return m_id; }

  // OCCT RTTI
  DEFINE_STANDARD_RTTI_INLINE(EntityOwner, Standard_Transient)

private:
  int m_id;
};
