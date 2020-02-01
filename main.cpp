#include <ntifs.h>
#include "SDK.h"
#include "Utils.h"

//EAC Killer
struct CALLBACK_ENTRY {
	USHORT Version;
	USHORT OperationRegistrationCount;
	ULONG unk1;
	PVOID RegistrationContext;
	UNICODE_STRING Altitude;
};

struct CALLBACK_ENTRY_ITEM
{
	LIST_ENTRY CallbackList;
	OB_OPERATION Operations;
	ULONG Active;
	CALLBACK_ENTRY* CallbackEntry;
	PVOID ObjectType;
	POB_PRE_OPERATION_CALLBACK PreOperation;
	POB_POST_OPERATION_CALLBACK PostOperation;
};

//Dummy callbacks func's
OB_PREOP_CALLBACK_STATUS DummyPreCallback() {
	return OB_PREOP_SUCCESS;
}

void DummyPostCallback() {
	return;
}

//Main Disabler - Enabler
ULONG GetCallbackListOffset(ULONG64& ProcType)
{
	ProcType = (ULONG64)*PsProcessType;
	for (int i = 0xF8; i > 0; i -= 8) {
		if (ProcType && MmIsAddressValid(PVOID(ProcType + i))) {
			ULONG64 First = *(ULONG64*)(ProcType + i);
			if (First && MmIsAddressValid(PVOID(First + 0x20))) {
				ULONG64 Test = *(ULONG64*)(First + 0x20);
				if (Test && (Test == ProcType)) return i;
			}
		}
	} 
	
	return 0;
}

void DisableBEObjectCallbacks() 
{
	ULONG64 ProcType = 0;
	ULONG Off = GetCallbackListOffset(ProcType);
	if (!ProcType || !Off) return;

	LIST_ENTRY* CallBackList = (LIST_ENTRY*)(ProcType + Off);
	CALLBACK_ENTRY_ITEM* CurCallback = (CALLBACK_ENTRY_ITEM*)CallBackList->Flink;
	CALLBACK_ENTRY_ITEM* FirstCallBack = (CALLBACK_ENTRY_ITEM*)CallBackList->Flink;

	do
	{
		if (CurCallback && MmIsAddressValid(CurCallback) && 
			MmIsAddressValid(CurCallback->CallbackEntry) &&
			MmIsAddressValid(CurCallback->CallbackEntry->Altitude.Buffer))
		{
			DbgPrint("FACE: %ws\n\n", CurCallback->CallbackEntry->Altitude.Buffer);
		}

		CurCallback = (CALLBACK_ENTRY_ITEM*)CurCallback->CallbackList.Flink;
	} while (CurCallback != FirstCallBack);
}

//Worker Thread
void WorkItem_Mgr(PDEVICE_OBJECT, PVOID)
{
	
}



NTSTATUS DriverEntry()
{
	//get device
	PFILE_OBJECT FObj = nullptr; PDEVICE_OBJECT DObj = nullptr;
	UNICODE_STRING NDIS_Name = RTL_CONSTANT_STRING(L"\\Device\\Ndis");
	IoGetDeviceObjectPointer(&NDIS_Name, FILE_ALL_ACCESS, &FObj, &DObj);

	//create workitem & cleanup
	PIO_WORKITEM WorkItem = IoAllocateWorkItem(DObj);
	IoQueueWorkItem(WorkItem, WorkItem_Mgr, NormalWorkQueue, nullptr);
	ObfDereferenceObject(FObj); return STATUS_SUCCESS;
}