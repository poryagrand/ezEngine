#pragma once

#include <Foundation/Basics/Types/Bitflags.h>
#include <Foundation/Utilities/EnumerableClass.h>

#include <Image.h>

EZ_DECLARE_FLAGS(ezUInt8, ezImageConversionFlags, None, InPlace, Lossy);

/// \brief Helper class containing utilities to convert between different image formats and layouts.
class ezIImageConversion : public ezEnumerable<ezIImageConversion>
{
public:
  /// \brief Finds the image format from a given list of formats which is the cheapest to convert to.
  static ezImageFormat::Enum
    FindClosestCompatibleFormat(ezImageFormat::Enum format, const ezImageFormat::Enum* pCompatibleFormats, ezUInt32 uiNumCompatible);

  /// \brief Finds the image format from a given list of formats which is the cheapest to convert to.
  template<int N> static ezImageFormat::Enum
    FindClosestCompatibleFormat(ezImageFormat::Enum format, const ezImageFormat::Enum(&compatibleFormats)[N])
  {
    return FindClosestCompatibleFormat(format, compatibleFormats, N);
  }

  static ezResult Convert(const ezImage& source, ezImage& target, ezImageFormat::Enum targetFormat);

  EZ_DECLARE_ENUMERABLE_CLASS(ezIImageConversion);

protected:
  ezIImageConversion();
  virtual ~ezIImageConversion();

  virtual ezResult DoConvert(const ezImage& source, ezImage& target, ezImageFormat::Enum targetFormat) const = 0;

  struct SubConversion
  {
    ezImageFormat::Enum m_sourceFormat;
    ezImageFormat::Enum m_targetFormat;
    ezBitflags<ezImageConversionFlags> m_flags;
  };

  ezStaticArray<SubConversion, 16> m_subConversions;

private:
  static void RebuildConversionTable();
};