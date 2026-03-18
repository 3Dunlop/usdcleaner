// Copyright (C) Remedy Entertainment Plc.
// PATCHED by USDCleaner: Sibling deduplication for collectFbxNodes.
// This header is included by UsdFbxDataReader.cpp via forced-include or
// direct edit to fix the duplicate sibling name bug.
//
// The problem: Navisworks FBX exports create sibling FBX nodes with
// identical names (e.g., multiple "Floor" Xforms, multiple "Site___Earth"
// meshes under the same parent). The original code only checked the
// current node's name against its CHILDREN's names, not against
// already-processed SIBLINGS. This caused duplicate USD prim paths,
// silently overwriting geometry data (last-wins in the PrimMap).
//
// The fix: After computing the cleaned name, check if the prim path
// already exists in the context. If it does, append _1, _2, etc.

#pragma once

#include <string>
#include <pxr/usd/sdf/path.h>
#include <pxr/base/tf/stringUtils.h>

namespace remedy
{
	/// Ensure a prim name is unique among its siblings by checking the context.
	/// If parentPath/name already exists, appends _1, _2, etc.
	template< typename Context >
	inline std::string ensureUniqueSiblingName(
		const Context& context,
		const SdfPath& parentPath,
		const std::string& name )
	{
		std::string uniqueName = name;
		SdfPath candidatePath = parentPath.AppendChild( TfToken( uniqueName ) );

		if( context.GetPrim( candidatePath ).has_value() )
		{
			int suffix = 1;
			do
			{
				uniqueName = TfStringPrintf( "%s_%d", name.c_str(), suffix++ );
				candidatePath = parentPath.AppendChild( TfToken( uniqueName ) );
			} while( context.GetPrim( candidatePath ).has_value() );
		}

		return uniqueName;
	}
} // namespace remedy
