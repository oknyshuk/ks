import ks.tier1.exprevaluator;
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk
//
// Forwarders of CMeshBase, the mesh the shader API draws, to the mesh implementation. Included by meshdx8.cpp.

void CMeshBase::BeginPass()
{
	switch ( Kind() )
	{
	case MeshKind::StaticMesh: static_cast<CMeshDX8 *>( this )->BeginPass(); return;
	case MeshKind::DynamicMesh: static_cast<CDynamicMeshDX8 *>( this )->BeginPass(); return;
	case MeshKind::TempMesh: static_cast<CTempMeshDX8 *>( this )->BeginPass(); return;
	default: break;
	}
	std::unreachable();
}
void CMeshBase::RenderPass( const unsigned char *pInstanceCommandBuffer )
{
	switch ( Kind() )
	{
	case MeshKind::StaticMesh: static_cast<CMeshDX8 *>( this )->RenderPass( pInstanceCommandBuffer ); return;
	case MeshKind::DynamicMesh: static_cast<CDynamicMeshDX8 *>( this )->RenderPass( pInstanceCommandBuffer ); return;
	case MeshKind::TempMesh: static_cast<CTempMeshDX8 *>( this )->RenderPass( pInstanceCommandBuffer ); return;
	default: break;
	}
	std::unreachable();
}
void CMeshBase::DrawPrims( const unsigned char *pInstanceCommandBuffer )
{
	switch ( Kind() )
	{
	case MeshKind::StaticMesh: static_cast<CMeshDX8 *>( this )->DrawPrims( pInstanceCommandBuffer ); return;
	case MeshKind::DynamicMesh: static_cast<CDynamicMeshDX8 *>( this )->DrawPrims( pInstanceCommandBuffer ); return;
	case MeshKind::TempMesh: static_cast<CTempMeshDX8 *>( this )->DrawPrims( pInstanceCommandBuffer ); return;
	default: break;
	}
	std::unreachable();
}
bool CMeshBase::HasColorMesh() const
{
	switch ( Kind() )
	{
	case MeshKind::StaticMesh: return static_cast<const CMeshDX8 *>( this )->HasColorMesh(); 
	case MeshKind::DynamicMesh: return static_cast<const CDynamicMeshDX8 *>( this )->HasColorMesh(); 
	case MeshKind::TempMesh: return static_cast<const CTempMeshDX8 *>( this )->HasColorMesh(); 
	default: break;
	}
	std::unreachable();
}
bool CMeshBase::HasFlexMesh() const
{
	switch ( Kind() )
	{
	case MeshKind::StaticMesh: return static_cast<const CMeshDX8 *>( this )->HasFlexMesh(); 
	case MeshKind::DynamicMesh: return static_cast<const CDynamicMeshDX8 *>( this )->HasFlexMesh(); 
	case MeshKind::TempMesh: return static_cast<const CTempMeshDX8 *>( this )->HasFlexMesh(); 
	default: break;
	}
	std::unreachable();
}
bool CMeshBase::IsUsingVertexID() const
{
	switch ( Kind() )
	{
	case MeshKind::StaticMesh: return static_cast<const CMeshDX8 *>( this )->IsUsingVertexID(); 
	case MeshKind::DynamicMesh: return static_cast<const CDynamicMeshDX8 *>( this )->IsUsingVertexID(); 
	case MeshKind::TempMesh: return static_cast<const CTempMeshDX8 *>( this )->IsUsingVertexID(); 
	default: break;
	}
	std::unreachable();
}
VertexStreamSpec_t * CMeshBase::GetVertexStreamSpec() const
{
	switch ( Kind() )
	{
	case MeshKind::StaticMesh: return static_cast<const CMeshDX8 *>( this )->GetVertexStreamSpec(); 
	case MeshKind::DynamicMesh: return static_cast<const CDynamicMeshDX8 *>( this )->GetVertexStreamSpec(); 
	case MeshKind::TempMesh: return static_cast<const CTempMeshDX8 *>( this )->GetVertexStreamSpec(); 
	default: break;
	}
	std::unreachable();
}
