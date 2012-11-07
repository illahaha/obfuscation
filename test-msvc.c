///////////////////////////////////////////////////////////////
// compile:
// cl test-msvc.c /link /INCREMENTAL:NO
///////////////////////////////////////////////////////////////
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <windows.h>

#define INIT_VIA_TLS 0

#if INIT_VIA_TLS
///////////////////////////////////////////////////////////////
// TLS init, section: .CRT$XLx
///////////////////////////////////////////////////////////////
#pragma comment(linker, "/INCLUDE:__tls_used")
#pragma comment(linker, "/INCLUDE:__init_func")
#pragma section(".CRT$XLB",read)
#define INITIALIZER(f) \
    static void __stdcall f(PVOID, DWORD, PVOID); \
    __declspec(allocate(".CRT$XLB")) EXTERN_C PIMAGE_TLS_CALLBACK _init_func = f; \
    static void __stdcall f(PVOID _u1, DWORD _u2, PVOID _u3)
#else
///////////////////////////////////////////////////////////////
// CRT-init, section: .CRT$XCx
///////////////////////////////////////////////////////////////
#pragma section(".CRT$XCB",read)
#define INITIALIZER(f) \
    static void __cdecl f(void);\
    __declspec(allocate(".CRT$XCB")) void (__cdecl*f##_)(void) = f; \
    static void __cdecl f(void)
#endif

///////////////////////////////////////////////////////////////
// Defines for generating unique labels inside inline assembly
///////////////////////////////////////////////////////////////
#define APPEND_COUNTER2(x,y) x##y
#define APPEND_COUNTER1(x,y) APPEND_COUNTER2(x,y)
#define APPEND_COUNTER(x) APPEND_COUNTER1(x, __COUNTER__)
#define LABEL(x) APPEND_COUNTER(x)

#define my_obf(x, y) \
    __asm call $+6    \
    __asm __emit 0xC7 \
    __asm x: \
    __asm mov eax, offset x \
    __asm mov ebx, offset y \
    __asm sub ebx, eax \
    __asm pop eax \
    __asm inc eax \
    __asm add eax, 1 \
    __asm add ebx, eax \
    __asm push ebx \
    __asm retn \
    __asm y: \
    __asm __emit 0x8D

#define obf() my_obf(LABEL(beg_obf), LABEL(end_obf))

///////////////////////////////////////////////////////////////
// Global variables
///////////////////////////////////////////////////////////////
unsigned int addr;
unsigned int printer;
unsigned int _mask;

void __declspec(naked) __cdecl
call2(void* func, void* param1, void* param2, unsigned int mask) {
    __asm {
        mov  [esp-4], ebx     // Store EBX register
        mov  ebx, offset off1 // Some obfuscation junk which simply brings us to the next instruction
        
        add  ebx, 2
        cmp  ebx, ebx
        jnz  off2
        add  ebx, 3
        jmp  ebx              // Indirect jump to the next instruction
        
        _asm _emit 0xC7 _asm _emit 0xE8
        
        off1:
        _asm _emit 0xC7 _asm _emit 0x84 _asm _emit 0xAB
        
        off2:
        _asm _emit 0xC3 _asm _emit 0x8D
        mov  ebx, [esp+12]   // Start "pushing" parameters in reverse order: param2
        jmp  off3
        _asm _emit 0xC7 _asm _emit 0x02
        
        off3:
        mov [esp-8], ebx
        jmp  off4
        _asm _emit 0xC7
        
        off4:
        mov  ebx, [esp+8]  
        mov  [esp-12], ebx // The first parameter has been "pushed": param1
        mov  ebx, [esp+4]  // Load the address of the function into EBX register: func
        xor  ebx, [esp+16] // Apply XOR mask: mask
        sub  esp, 12       // Adjust ESP
        call ebx           // Call the function. It is strongly recommended to use indirect calls
        add  esp, 8        // Restore ESP
        pop  ebx           // Restore EBX
        retn
    }
}

/*	Prints the string specified by p
 * 	c times
 */
void
my_func(int c, char* p) {
	obf();
	while(c!=0)
	{
		call2((void*)printer, (void*)"%s\n", (void*)p, _mask);
		obf();
		c--;
	}
	obf();
}

/*	This constructor is responsible 
 * 	for calculation of random mask
 * 	and initial encryption of addresses.
 */
INITIALIZER(tls_init) {
    #if INIT_VIA_TLS
        // can't use srand() and rand(), coz TLS callback initialize
        // before CRT initialize    
        // rdtsc
        _asm _emit 0x0F _asm _emit 0x31
        _asm xor eax, edx
        _asm mov _mask, eax
    #else
        srand(time(NULL));
        _mask = rand();        
    #endif
    addr    = (unsigned int)my_func ^ _mask;
    printer = (unsigned int)printf  ^ _mask;
}

int
main() {
    char *str = "Hello!!!";
	int	 i    = 8;

    #if INIT_VIA_TLS
        // dummy call
        // if remove this call TLS-init doesn't work
        #pragma comment(lib, "user32.lib")
        CharLower("1");
    #endif

	printf("Start\n");
	obf();
	call2((void*)addr, (void*)i, (void*)str, _mask);
	obf();
	call2((void*)printer, (void*)"%s\n", (void*)"Test calling library function", _mask);
	obf();
	printf("Stop\n");
	
	return (0);
}
