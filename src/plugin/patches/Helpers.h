// Copyright (C) Remedy Entertainment Plc.
// PATCHED by USDCleaner: Fix duplicate sibling name handling.
// Original cleanName() returns immediately for valid USD identifiers WITHOUT
// checking usedNames, causing FBX files with duplicate sibling node names
// (common in Navisworks exports) to silently overwrite each other.

#pragma once

#include <pxr/pxr.h>
#include <pxr/usd/usd/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace remedy
{
	inline std::string cleanName( const std::string& inName, const char* trimLeading, const std::set< std::string >& usedNames )
	{
		std::string name;

		if( SdfPath::IsValidIdentifier( inName ) )
		{
			name = inName;
		}
		else if( inName.empty() )
		{
			name = "_";
		}
		else
		{
			name = TfStringTrimLeft( inName, trimLeading );
			name = TfMakeValidIdentifier( name );
		}

		// PATCH: Always check for duplicates when usedNames is provided,
		// even if the name was already a valid identifier.
		// Original code only checked when the name needed sanitization.
		if( !usedNames.empty() && usedNames.find( name ) != usedNames.end() )
		{
			int i = 0;
			std::string attempt = TfStringPrintf( "%s_%d", name.c_str(), ++i );
			while( usedNames.find( attempt ) != usedNames.end() )
			{
				attempt = TfStringPrintf( "%s_%d", name.c_str(), ++i );
			}
			name = attempt;
		}
		return name;
	}

	inline std::string cleanName( const std::string& inName )
	{
		return cleanName( inName, " _", {} );
	}

	inline std::string cleanName( const std::string& inName, const char* trimLeading )
	{
		return cleanName( inName, trimLeading, {} );
	}

	inline std::string cleanName( const std::string& inName, const std::set< std::string >& usedNames )
	{
		return cleanName( inName, " _", usedNames );
	}
} // namespace remedy
