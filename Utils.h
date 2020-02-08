//DBG Help
#define wsp(a) DbgPrint("FACE WSTR: %ws\n\n", (a))
#define hp(a) DbgPrint("FACE HEX: 0x%p\n\n", (a))
#define sp(a) DbgPrint("FACE STR: %s\n\n", (a))
#define dp(a) DbgPrint("FACE DEC: %d\n\n", (a))

//Thread Funcs
DWORD64 PsResumeThread;
DWORD64 PsSuspendThread;

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

ULONG64 GetDriverBase(const char* DriverName, PULONG DriverVASize)
{
	PSYSTEM_MODULE_INFORMATION ModuleList = (PSYSTEM_MODULE_INFORMATION)
		ZwQuerySystemInfo(SystemModuleInformation);

	ULONG64 ModuleBase = 0;
	for (ULONG i = 0; i < ModuleList->ulModuleCount; i++){
		SYSTEM_MODULE Module = ModuleList->Modules[i];
		if (!strcmp((char*)&Module.FullPathName[Module.OffsetToFileName], DriverName)) {
			if (DriverVASize) *DriverVASize = Module.ImageSize;
			ModuleBase = (ULONG64)Module.ImageBase; break;
		}
	}

	ExFreePool(ModuleList);
	return ModuleBase;
}

void ThreadsMgr(const char* DriverName, bool Suspend)
{
	ULONG DriverSize; 
	ULONG64 DriverBase = GetDriverBase(DriverName, &DriverSize);
	if (!DriverBase || !DriverSize) return;

	PSYSTEM_PROCESS_INFO ProcInfo = (PSYSTEM_PROCESS_INFO)ZwQuerySystemInfo(SystemProcessInformation);
	for (PSYSTEM_PROCESS_INFO Cur = ProcInfo;;)
	{
		if (Cur->UniqueProcessId == (HANDLE)4)
		{
			for (ULONG i = 0; i < Cur->NumberOfThreads; i++)
			{
				SYSTEM_THREAD_INFORMATION Thread = Cur->Threads[i];
				if (((ULONG64)Thread.StartAddress > DriverBase) &&
					((ULONG64)Thread.StartAddress < (DriverBase + DriverSize))) 
				{
					PETHREAD ProcThread = nullptr;
					if (!PsLookupThreadByThreadId(Thread.ClientId.UniqueThread, &ProcThread) && ProcThread)
					{
						if (Suspend) {
							typedef void(__fastcall* KeSuspendThreadFn)(PETHREAD, PVOID);
							((KeSuspendThreadFn)PsSuspendThread)(ProcThread, nullptr);
						}

						else {
							typedef void(__fastcall* KeResumeThreadFn)(PETHREAD);
							((KeResumeThreadFn)PsResumeThread)(ProcThread);
						}

						ObDereferenceObject(ProcThread);
					}
				}
			}
		}

		if (!Cur->NextEntryOffset) break;
		else Cur = (PSYSTEM_PROCESS_INFO)((PUCHAR)Cur + Cur->NextEntryOffset);
	}

	ExFreePool(ProcInfo);
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

ULONG64 KPattern(PUCHAR Start, ULONG ImageLen, const UCHAR* Pattern, const char* Mask)
{
	//find pattern
	for (PUCHAR region_it = Start; region_it < (Start + ImageLen); ++region_it)
	{
		if (MmIsAddressValid(region_it) && *region_it == *Pattern)
		{
			bool found = true;
			const unsigned char* pattern_it = Pattern, * mask_it = (const UCHAR*)Mask, * memory_it = region_it;
			for (; *mask_it && (memory_it < (Start + ImageLen)); ++mask_it, ++pattern_it, ++memory_it)
			{
				if (*mask_it != 'x') continue;
				if (!MmIsAddressValid((void*)memory_it) || *memory_it != *pattern_it) {
					found = false; break;
				}
			}

			if (found)
				return (ULONG64)region_it;
		}
	}

	return 0;
}