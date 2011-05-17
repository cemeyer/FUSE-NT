#ifndef __BASICTYPES
#define __BASICTYPES

#include <wdm.h>

//
//  Some basic types that really should be included in the WDK
//

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed int int16_t;
typedef unsigned int uint16_t;
typedef signed long int int32_t;
typedef unsigned long int uint32_t;
typedef signed long long int int64_t;
typedef unsigned long long int uint64_t;

//
//  Types specific to the FUSE driver
//

typedef struct _IRP_LIST {
    PIRP Irp;
    struct _IRP_LIST* Next;
} IRP_LIST, *PIRP_LIST;

typedef struct _MODULE_STRUCT {
    //
    //  Maintain a list of available IRPs from the module
    //  that can be used to satisfy requests from userspace
    //  and a list of IRPs from userspace waiting to be
    //  fulfilled by the module
    //
    PIRP_LIST ModuleIrpList, UserspaceIrpList, OutstandingIrpList;
    PIRP_LIST ModuleIrpListEnd, UserspaceIrpListEnd, OutstandingIrpListEnd;
    FAST_MUTEX ModuleLock;

    //
    //  Space for the module name should be allocated separate
    //  from a file object so that the memory in which it resides
    //  is not reclaimed
    //

    WCHAR* ModuleName;

    //
    //  Store the module's file object pointer. When the
    //  module makes requests, we can verify that we are
    //  communicating with the module by comparing its
    //  file object pointer to the one in the incoming IRP
    //
    PFILE_OBJECT ModuleFileObject;
} MODULE_STRUCT, *PMODULE_STRUCT;

#endif // __BASICTYPES
