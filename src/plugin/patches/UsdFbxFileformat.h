// Patched version of UsdFbxFileformat.h
// Avoids #include "VERSION" which conflicts with C++17's <version> on Windows
// (case-insensitive filesystem). Original: Copyright (C) Remedy Entertainment Plc.
#pragma once

#include "pxr/base/tf/staticTokens.h"
#include "pxr/pxr.h"
#include "pxr/usd/sdf/fileFormat.h"

#include <iosfwd>
#include <string>

constexpr const char* USDFBX_VERSION = "1.1.0";

#define USDFBX_FILE_FORMAT_TOKENS ( ( Id, "fbx" ) )( ( Version, USDFBX_VERSION ) )( ( Target, "usd" ) )
PXR_NAMESPACE_USING_DIRECTIVE

TF_DECLARE_PUBLIC_TOKENS( UsdFbxFileFormatTokens, USDFBX_FILE_FORMAT_TOKENS );

namespace remedy
{
	TF_DECLARE_WEAK_AND_REF_PTRS( UsdFbxFileFormat );

	class UsdFbxFileFormat : public SdfFileFormat
	{
	public:
		SdfAbstractDataRefPtr InitData( const FileFormatArguments& ) const override;
		bool CanRead( const std::string& file ) const override;
		bool Read( SdfLayer* layer, const std::string& resolvedPath, bool metadataOnly ) const override;
		bool ReadFromString( SdfLayer* layer, const std::string& str ) const override;

		bool WriteToString( const SdfLayer& layer, std::string* str, const std::string& comment = std::string() ) const override;
		bool WriteToStream( const SdfSpecHandle& spec, std::ostream& out, size_t indent ) const override;

		bool WriteToFile(
			const SdfLayer& layer,
			const std::string& filePath,
			const std::string& comment,
			const FileFormatArguments& args ) const override;

	protected:
		template< typename T >
		friend class PXR_INTERNAL_NS::Sdf_FileFormatFactory;

		UsdFbxFileFormat();

	private:
		SdfFileFormatConstPtr m_usda;
	};
} // namespace remedy
