// SPDX-License-Identifier: MIT
// Copyright (c) 2026 oknyshuk

// The switch dispatch of one group of particle operators (public/particles/particle_operator_list.h).
// Included at the end of the source file that defines the group's classes, after defining
// KS_PARTICLE_OP_GROUP to the group's name (Constraints, Initializers, ...). For every entry point of
// an operator it defines ParticleOps::<entry>_<group>, which switches on the kind and calls the
// member of that class (ParticleOps::<entry>_Call): the class' own, or the default of CParticleOperatorInstance.

#define KS_PARTICLE_OP_CAT_( a, b ) a##b
#define KS_PARTICLE_OP_CAT( a, b ) KS_PARTICLE_OP_CAT_( a, b )
#define KS_PARTICLE_OP_GROUP_LIST KS_PARTICLE_OP_CAT( KS_PARTICLE_OPS_, KS_PARTICLE_OP_GROUP )

namespace
{
	template < class Op, class P >
	auto *ParticleOpCast( P *pOp )
	{
		return static_cast< std::conditional_t< std::is_const_v< P >, const Op, Op > * >( pOp );
	}
}

#define KS_PARTICLE_OP_CASE( cls, Name, Args ) \
	case ParticleOperatorKind::cls: return Name##_Call< cls >( KS_PARTICLE_OP_ARGS( KS_PARTICLE_OP_UNPAREN Args ) );

#define KS_PARTICLE_OP_DEFINE_GROUP_ENTRY( Ret, Name, Const, Params, Args ) \
	Ret ParticleOps::KS_PARTICLE_OP_CAT( Name##_, KS_PARTICLE_OP_GROUP )( KS_PARTICLE_OP_SIGNATURE( Const, KS_PARTICLE_OP_UNPAREN Params ) ) \
	{ \
		switch ( pOp->m_Kind ) \
		{ \
		KS_PARTICLE_OP_GROUP_LIST( KS_PARTICLE_OP_CASE, Name, Args ) \
		default: std::unreachable(); \
		} \
	}
KS_PARTICLE_OP_ENTRIES( KS_PARTICLE_OP_DEFINE_GROUP_ENTRY )
#undef KS_PARTICLE_OP_DEFINE_GROUP_ENTRY
#undef KS_PARTICLE_OP_CASE

#define KS_PARTICLE_OP_CREATE_CASE( cls, ... ) \
	case ParticleOperatorKind::cls: return new cls;

CParticleOperatorInstance *ParticleOps::KS_PARTICLE_OP_CAT( Create_, KS_PARTICLE_OP_GROUP )( ParticleOperatorKind kind )
{
	switch ( kind )
	{
	KS_PARTICLE_OP_GROUP_LIST( KS_PARTICLE_OP_CREATE_CASE, )
	default: std::unreachable();
	}
}
#undef KS_PARTICLE_OP_CREATE_CASE

#define KS_PARTICLE_OP_DESTROY_CASE( cls, ... ) \
	case ParticleOperatorKind::cls: delete ParticleOpCast< cls >( pOp ); return;

void ParticleOps::KS_PARTICLE_OP_CAT( Destroy_, KS_PARTICLE_OP_GROUP )( CParticleOperatorInstance *pOp )
{
	switch ( pOp->m_Kind )
	{
	KS_PARTICLE_OP_GROUP_LIST( KS_PARTICLE_OP_DESTROY_CASE, )
	default: std::unreachable();
	}
}
#undef KS_PARTICLE_OP_DESTROY_CASE

#undef KS_PARTICLE_OP_GROUP_LIST
#undef KS_PARTICLE_OP_CAT
#undef KS_PARTICLE_OP_CAT_
#undef KS_PARTICLE_OP_GROUP
