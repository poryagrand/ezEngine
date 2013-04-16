#pragma once

#include <Foundation/Math/Vec3.h>

/// \brief Describes on which side of a plane a point or an object is located.
struct ezPositionOnPlane
{
  enum Enum
  {
    Back,       ///< Something is completely on the back side of a plane
    Front,      ///< Something is completely in front of a plane
    OnPlane,    ///< Something is lying completely on a plane (all points)
    Spanning,   ///< Something is spanning a plane, i.e. some points are on the front and some on the back
  };
};

/// \brief A class that represents a mathematical plane.
class EZ_FOUNDATION_DLL ezPlane
{
public:
  // Means this object can be copied using memcpy instead of copy construction.
  EZ_DECLARE_POD_TYPE();

// *** Data ***
public:

  ezVec3 m_vNormal;
  float m_fNegDistance;


// *** Constructors ***
public:

  /// \brief Default constructor. Does not initialize the plane.
  ezPlane(); // [tested]

  /// \brief Creates the plane-equation from a normal and a point on the plane.
  ezPlane(const ezVec3 &vNormal, const ezVec3& vPointOnPlane); // [tested]

  /// \brief Creates the plane-equation from three points on the plane.
  ezPlane(const ezVec3& v1, const ezVec3& v2, const ezVec3& v3); // [tested]

  /// \brief Creates the plane-equation from three points on the plane, given as an array.
  ezPlane(const ezVec3* const pVertices); // [tested]

  /// \brief Creates the plane-equation from a set of unreliable points lying on the same plane. Some points might be equal or too close to each other for the typical algorithm.
  ezPlane(const ezVec3* const pVertices, ezUInt32 iMaxVertices); // [tested]

  /// \brief Creates the plane-equation from a normal and a point on the plane.
  void SetFromNormalAndPoint(const ezVec3 &vNormal, const ezVec3& vPointOnPlane); // [tested]

  /// \brief Creates the plane-equation from three points on the plane.
  ezResult SetFromPoints(const ezVec3& v1, const ezVec3& v2, const ezVec3& v3); // [tested]

  /// \brief Creates the plane-equation from three points on the plane, given as an array.
  ezResult SetFromPoints(const ezVec3* const pVertices); // [tested]

  /// \brief Creates the plane-equation from a set of unreliable points lying on the same plane. Some points might be equal or too close to each other for the typical algorithm. Returns false, if no reliable set of points could be found. Does try to create a plane anyway.
  ezResult SetFromPoints(const ezVec3* const pVertices, ezUInt32 iMaxVertices); // [tested]

  /// \brief Creates a plane from two direction vectors that span the plane, and one point on it.
  ezResult SetFromDirections(const ezVec3& vTangent1, const ezVec3& vTangent2, const ezVec3& vPointOnPlane); // [tested]

  /// \brief Sets the plane to an invalid state (all zero).
  void SetInvalid(); // [tested]

// *** Distance and Position ***
public:

  /// \brief Returns the distance of the point to the plane.
  float GetDistanceTo(const ezVec3& vPoint) const; // [tested]

  /// \brief Returns the minimum distance that any of the given points had to the plane.
  ///
  /// 'Minimum' means the (non-absolute) distance of a point to the plane. So a point behind the plane will always have a 'lower distance'
  /// than a point in front of the plane, even if that is closer to the plane's surface.
  float GetMinimumDistanceTo(const ezVec3* pPoints, ezUInt32 uiNumPoints, ezUInt32 uiStride = sizeof (ezVec3)) const; // [tested]

  /// \brief Returns the minimum and maximum distance that any of the given points had to the plane.
  ///
  /// 'Minimum' (and 'maximum') means the (non-absolute) distance of a point to the plane. So a point behind the plane will always have a 'lower distance'
  /// than a point in front of the plane, even if that is closer to the plane's surface.
  void GetMinMaxDistanceTo(float &out_fMin, float &out_fMax, const ezVec3* pPoints, ezUInt32 uiNumPoints, ezUInt32 uiStride = sizeof (ezVec3)) const; // [tested]

  /// \brief Returns on which side of the plane the point lies.
  ezPositionOnPlane::Enum GetPointPosition(const ezVec3& vPoint) const; // [tested]

  /// \brief Returns on which side of the plane the point lies.
  ezPositionOnPlane::Enum GetPointPosition(const ezVec3& vPoint, float fPlaneHalfWidth) const; // [tested]

  /// \brief Returns on which side of the plane the set of points lies. Might be on boths sides.
  ezPositionOnPlane::Enum GetObjectPosition(const ezVec3* const vPoints, int iVertices) const; // [tested]

  /// \brief Returns on which side of the plane the set of points lies. Might be on boths sides.
  ezPositionOnPlane::Enum GetObjectPosition(const ezVec3* const vPoints, int iVertices, float fPlaneHalfWidth) const; // [tested]

  /// \todo GetObjectPosition Boundingbox
  /// \todo GetObjectPosition BoundingSphere

  /// \brief Projects a point onto a plane (along the planes normal).
  const ezVec3 ProjectOntoPlane(const ezVec3& vPoint) const; // [tested]

  /// \brief Returns the mirrored point. E.g. on the other side of the plane, at the same distance.
  const ezVec3 Mirror(const ezVec3& vPoint) const; // [tested]

  /// \brief Take the given direction vector and returns a modified one that is coplanar to the plane.
  const ezVec3 GetCoplanarDirection(const ezVec3& vDirection) const; // [tested]

// *** Comparisons ***
public:

  /// \brief Checks whether this plane and the other are identical.
  bool IsIdentical(const ezPlane& rhs) const; // [tested]

  /// \brief Checks whether this plane and the other are equal within some threshold.
  bool IsEqual(const ezPlane& rhs, float fEpsilon = ezMath_DefaultEpsilon) const; // [tested]

  /// \brief Checks whether the plane has valid values (not NaN, or infinite, normalized normal).
  bool IsValid() const; // [tested]

// *** Modifications ***
public:

  /// \brief Transforms the plane with the given matrix.
  void Transform(const ezMat3& m); // [tested]

  /// \brief Transforms the plane with the given matrix.
  void Transform(const ezMat4& m); // [tested]

  /// \brief Negates Normal/Distance to switch which side of the plane is front and back.
  void Flip(); // [tested]

  /// \brief Negates Normal/Distance to switch which side of the plane is front and back. Returns true, if the plane had to be flipped.
  bool FlipIfNecessary(const ezVec3& vPoint, bool bPlaneShouldFacePoint = true); // [tested]

// *** Intersection Tests ***
public:

  /// \brief Returns true, if the ray hit the plane. The intersection time describes at which multiple of the ray direction the ray hit the plane.
  bool GetRayIntersection(const ezVec3& vRayStartPos, const ezVec3& vRayDir, float* out_fIntersection = NULL, ezVec3* out_vIntersection = NULL) const; // [tested]

  /// \brief Returns true, if the ray intersects the plane. Intersection time and point are stored in the out-parameters. Allows for intersections at negative times (shooting into the opposite direction).
  bool GetRayIntersectionBiDirectional(const ezVec3& vRayStartPos, const ezVec3& vRayDir, float* out_fIntersection = NULL, ezVec3* out_vIntersection = NULL) const; // [tested]

  /// \brief Returns true, if there is any intersection with the plane between the line's start and end position. Returns the fraction along the line and the actual intersection point.
  bool GetLineSegmentIntersection(const ezVec3& vLineStartPos, const ezVec3& vLineEndPos, float* out_fHitFraction = NULL, ezVec3* out_vIntersection = NULL) const; // [tested]

  /// \brief Computes the one point where all three planes intersect. Returns EZ_FAILURE if no such point exists.
  static ezResult GetPlanesIntersectionPoint(const ezPlane& p0, const ezPlane& p1, const ezPlane& p2, ezVec3& out_Result); // [tested]

// *** Helper Functions ***
public:

  /// \brief Returns three points from an unreliable set of points, that reliably form a plane. Returns false, if there are none.
  static ezResult FindSupportPoints(const ezVec3* const pVertices, ezInt32 iMaxVertices, ezInt32& out_v1, ezInt32& out_v2, ezInt32& out_v3); // [tested]

};

/// \brief Checks whether this plane and the other are identical.
bool operator== (const ezPlane& lhs, const ezPlane& rhs); // [tested]

/// \brief Checks whether this plane and the other are not identical.
bool operator!= (const ezPlane& lhs, const ezPlane& rhs); // [tested]

#include <Foundation/Math/Implementation/Plane_inl.h>



