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

//Dummy func's
PDRIVER_DISPATCH OrgDisp;
NTSTATUS DummyDispatch(PDEVICE_OBJECT a1, PIRP Irp) 
{
	auto irpStack = IoGetCurrentIrpStackLocation(Irp);
	if (irpStack->Parameters.DeviceIoControl.IoControlCode == 0x22E023)
		return OrgDisp(a1, Irp);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

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

#define ProcessName L"r5apex.exe"

//Worker Thread
void WorkItem_Mgr(PDEVICE_OBJECT, PVOID)
{
	while (true)
	{
		if (GetPID(ProcessName))
		{
			sp("FLEX");
			Sleep(5000);

			//suspend threads
			ThreadsMgr("EasyAntiCheat.sys", true);

			//get device
			PFILE_OBJECT FObj = nullptr; PDEVICE_OBJECT DObj = nullptr;
			UNICODE_STRING NDIS_Name = RTL_CONSTANT_STRING(L"\\Device\\EasyAntiCheat");
			IoGetDeviceObjectPointer(&NDIS_Name, FILE_ALL_ACCESS, &FObj, &DObj);
			ObDereferenceObject(FObj);

			//backup device io ctrl
			DRIVER_OBJECT* DriverObject = DObj->DriverObject;
			PDRIVER_DISPATCH OrgClose = DriverObject->MajorFunction[IRP_MJ_CLOSE];
			PDRIVER_DISPATCH OrgCreate = DriverObject->MajorFunction[IRP_MJ_CREATE]; 
			OrgDisp = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];

			//setup dummy dispatcher
			DriverObject->MajorFunction[IRP_MJ_CLOSE] =
			DriverObject->MajorFunction[IRP_MJ_CREATE] =
			DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DummyDispatch;

			while (GetPID(ProcessName))
				Sleep(200);

			Sleep(800);

			//restore original dispatcher
			DriverObject->MajorFunction[IRP_MJ_CLOSE] = OrgClose;
			DriverObject->MajorFunction[IRP_MJ_CREATE] = OrgCreate; 
			DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OrgDisp;

			//resume threads
			ThreadsMgr("EasyAntiCheat.sys", false);
			break;
		}

		Sleep(2000);
	}
}

NTSTATUS DriverEntry()
{
	UNICODE_STRING ResumeStr = RTL_CONSTANT_STRING(L"PsResumeProcess"); PUCHAR ResumeStart = (PUCHAR)MmGetSystemRoutineAddress(&ResumeStr);
	UNICODE_STRING SuspendStr = RTL_CONSTANT_STRING(L"PsSuspendProcess");PUCHAR SuspendStart = (PUCHAR)MmGetSystemRoutineAddress(&SuspendStr);
	PsSuspendThread = RVA(KPattern(SuspendStart, 0xFF, (PUCHAR)"\xE8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xE8", "x??????????x"), 5);
	PsResumeThread = RVA(KPattern(ResumeStart, 0xFF, (PUCHAR)"\xE8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xE8", "x??????????x"), 5);

	//get device
	PFILE_OBJECT FObj = nullptr; PDEVICE_OBJECT DObj = nullptr;
	UNICODE_STRING NDIS_Name = RTL_CONSTANT_STRING(L"\\Device\\Ndis");
	IoGetDeviceObjectPointer(&NDIS_Name, FILE_ALL_ACCESS, &FObj, &DObj);

	//create workitem & cleanup
	PIO_WORKITEM WorkItem = IoAllocateWorkItem(DObj);
	IoQueueWorkItem(WorkItem, WorkItem_Mgr, NormalWorkQueue, nullptr);
	ObDereferenceObject(FObj); return STATUS_SUCCESS;
}