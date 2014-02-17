#ifndef FUSEUTIL_H_
#define FUSEUTIL_H_

#define M_FUSE \
    (((ULONG)'F' << 24) | ((ULONG)'u' << 16) | ((ULONG)'s' << 8) | (ULONG)'e')

LPWSTR
FuseExtractModuleName (
    IN PIRP Irp,
    OUT PULONG Length
    );

LPWSTR
FuseAllocateModuleName (
    IN PIRP Irp
    );

VOID
FuseCopyModuleName (
    IN PIRP Irp,
    OUT WCHAR* Destination,
    OUT PULONG Length
    );

PVOID
FuseMapUserBuffer (
    IN OUT PIRP Irp
    );

VOID
FusePrePostIrp (
    IN PIRP Irp
    );

VOID
FuseLockUserBuffer (
    IN OUT PIRP Irp,
    IN LOCK_OPERATION Operation,
    IN ULONG BufferLength
    );

/*
 * Simple wrapper for Ex*FastMutex since GCC doesn't support SEH __finally
 * extensions.
 */
class ScopedExLock {
	PFAST_MUTEX m_;
public:
	ScopedExLock(PFAST_MUTEX m): m_(m) {
		ExAcquireFastMutex(m_);
	}
	~ScopedExLock() {
		ExReleaseFastMutex(m_);
	}
};

#endif
