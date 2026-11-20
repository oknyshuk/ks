#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2009 Valve Corporation

//
// Describes possible states of an async operation
//
import ks.core.types;
import ks.platform;
import ks.simd;
enum AsyncOperationState_t
{
	AOS_RUNNING,
	AOS_ABORTING,
	AOS_ABORTED,
	AOS_FAILED,
	AOS_SUCCEEDED,
};

//
// Interface of an async operation
//
class IMatchAsyncOperation
{
public:
	// Poll if operation has completed
	virtual bool IsFinished() = 0;

	// Operation state
	virtual AsyncOperationState_t GetState() = 0;

	// Retrieve a generic completion result for simple operations
	// that return simple results upon success,
	// results are operation-specific, may result in undefined behavior
	// if operation is still in progress.
	virtual uint64 GetResult() = 0;
	virtual uint64 GetResultExtraInfo() { return 0; }

	// Request operation to be aborted
	virtual void Abort() = 0;

	// Release the operation interface and all resources
	// associated with the operation. Operation callbacks
	// will not be called after Release. Operation object
	// cannot be accessed after Release.
	virtual void Release() = 0;
};

class IMatchAsyncOperationCallback
{
public:
	// Signals when operation has finished
	virtual void OnOperationFinished( IMatchAsyncOperation *pOperation ) = 0;
};
