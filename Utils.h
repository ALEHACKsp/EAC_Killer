//DBG Help
#define wsp(a) DbgPrint("FACE WSTR: %ws\n\n", (a))
#define hp(a) DbgPrint("FACE HEX: 0x%p\n\n", (a))
#define sp(a) DbgPrint("FACE STR: %s\n\n", (a))
#define dp(a) DbgPrint("FACE DEC: %d\n\n", (a))

void Sleep(LONG64 MSec) {
	LARGE_INTEGER Delay; Delay.QuadPart = -MSec * 10000;
	KeDelayExecutionThread(KernelMode, false, &Delay);
}

PVOID ZwQuerySystemInfo(SYSTEM_INFORMATION_CLASS InfoClass)
{
	Loop: ULONG InfoSize = 0;
	ZwQuerySystemInformation(InfoClass, nullptr, InfoSize, &InfoSize);
	if (!InfoSize) goto Loop; PVOID Buff = ExAllocatePool(NonPagedPool, InfoSize);
	if (!NT_SUCCESS(ZwQuerySystemInformation(InfoClass, Buff, InfoSize, &InfoSize))) {
		if (Buff) { ExFreePool(Buff); goto Loop; }
	} return Buff;
}

ULONG64 GetDriverBase(const char* DriverName)
{
	UNREFERENCED_PARAMETER(DriverName);
	PSYSTEM_MODULE_INFORMATION ModuleList = 
		(PSYSTEM_MODULE_INFORMATION)ZwQuerySystemInfo(SystemModuleInformation);

	ULONG64 ModuleBase = 0;
	for (ULONG64 i = 0; i < ModuleList->ulModuleCount; i++)
	{
		hp(ModuleList);
	}

	//ExFreePool(ModuleList);
	return ModuleBase;
}

HANDLE GetPID(const wchar_t* ProcessName)
{
	HANDLE PID = 0;
	PSYSTEM_PROCESS_INFO ProcInfo = (PSYSTEM_PROCESS_INFO)ZwQuerySystemInfo(SystemProcessInformation);
	for (PSYSTEM_PROCESS_INFO Cur = ProcInfo;;)
	{
		PWCH ImageName = Cur->ImageName.Buffer;
		if (ImageName && MmIsAddressValid(ImageName)) {
			if (!wcscmp(ProcessName, ImageName)) {
				PID = Cur->UniqueProcessId; break;
			}
		}

		if (!Cur->NextEntryOffset) break;
		else Cur = (PSYSTEM_PROCESS_INFO)((PUCHAR)Cur + Cur->NextEntryOffset);
	}

	ExFreePool(ProcInfo);
	return PID;
}