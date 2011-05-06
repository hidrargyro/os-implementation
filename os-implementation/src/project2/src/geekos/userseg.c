/*
 * Segmentation-based user mode implementation
 * Copyright (c) 2001,2003 David H. Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.23 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/ktypes.h>
#include <geekos/kassert.h>
#include <geekos/defs.h>
#include <geekos/mem.h>
#include <geekos/string.h>
#include <geekos/malloc.h>
#include <geekos/int.h>
#include <geekos/gdt.h>
#include <geekos/segment.h>
#include <geekos/tss.h>
#include <geekos/kthread.h>
#include <geekos/argblock.h>
#include <geekos/user.h>

/* ----------------------------------------------------------------------
 * Variables
 * ---------------------------------------------------------------------- */

#define DEFAULT_USER_STACK_SIZE 8192


/* ----------------------------------------------------------------------
 * Private functions
 * ---------------------------------------------------------------------- */


/*
 * Create a new user context of given size
 */

// TODO: Implement
static struct User_Context* Create_User_Context(ulong_t size)
{
	
    // Pido memoria para el proceso
    char *mem = (char *) Malloc(size);
    if(mem == NULL)
        goto error;

    //reset memory with zeros
    memset(mem, 0, size);

    // pido memoria para el User_Context
    struct User_Context *userContext = Malloc(sizeof(struct User_Context));
    if(userContext ==  NULL)
        goto error;    

    // Guardo el segment descriptor de la ldt en la gdt
    struct Segment_Descriptor *ldt_desc = Allocate_Segment_Descriptor(); //LDT-Descriptor for the process
    if(ldt_desc == NULL)
        goto error;    

    Init_LDT_Descriptor(ldt_desc, userContext->ldt, NUM_USER_LDT_ENTRIES);
    //creo un selector para el descriptor de ldt
    ushort_t ldt_selector = Selector(USER_PRIVILEGE, true, Get_Descriptor_Index(ldt_desc)); 

    //Inicio los segmentos
    Init_Code_Segment_Descriptor(&(userContext->ldt[0]), (ulong_t)mem, size/PAGE_SIZE, USER_PRIVILEGE);
    Init_Data_Segment_Descriptor(&(userContext->ldt[1]), (ulong_t)mem, size/PAGE_SIZE, USER_PRIVILEGE);

    //creo los selectores
    ushort_t cs_selector = Selector(USER_PRIVILEGE, false, 0);    
    ushort_t ds_selector = Selector(USER_PRIVILEGE, false, 1);

    //asigno todo al userContext
    userContext->ldtDescriptor = ldt_desc;
    userContext->ldtSelector = ldt_selector;
    userContext->csSelector = cs_selector;
    userContext->dsSelector = ds_selector;
    userContext->size = size;
    userContext->memory = mem;
	userContext->refCount = 0;
    goto success;

error:

    if(mem !=  NULL)
        Free(mem);
    if(userContext !=  NULL)
        Free(userContext);    
    return NULL;

success:

    return userContext;
}


static bool Validate_User_Memory(struct User_Context* userContext,
    ulong_t userAddr, ulong_t bufSize)
{
    ulong_t avail;

    if (userAddr >= userContext->size)
        return false;

    avail = userContext->size - userAddr;
    if (bufSize > avail)
        return false;

    return true;
}

/* ----------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */

/*
 * Destroy a User_Context object, including all memory
 * and other resources allocated within it.
 */
void Destroy_User_Context(struct User_Context* userContext)
{
    /*
     * Hints:
     * - you need to free the memory allocated for the user process
     * - don't forget to free the segment descriptor allocated
     *   for the process's LDT
     */

	Free_Segment_Descriptor(userContext->ldtDescriptor);
	Free(userContext->memory);
	Free(userContext);

}

/*
 * Load a user executable into memory by creating a User_Context
 * data structure.
 * Params:
 * exeFileData - a buffer containing the executable to load
 * exeFileLength - number of bytes in exeFileData
 * exeFormat - parsed ELF segment information describing how to
 *   load the executable's text and data segments, and the
 *   code entry point address
 * command - string containing the complete command to be executed:
 *   this should be used to create the argument block for the
 *   process
 * pUserContext - reference to the pointer where the User_Context
 *   should be stored
 *
 * Returns:
 *   0 if successful, or an error code (< 0) if unsuccessful
 */
int Load_User_Program(char *exeFileData, ulong_t exeFileLength,
    struct Exe_Format *exeFormat, const char *command,
    struct User_Context **pUserContext)
{
    /*
     * Hints:
     * - Determine where in memory each executable segment will be placed
     * - Determine size of argument block and where it memory it will
     *   be placed
     * - Copy each executable segment into memory
     * - Format argument block in memory
     * - In the created User_Context object, set code entry point
     *   address, argument block address, and initial kernel stack pointer
     *   address
     */
     
   int i;
  ulong_t maxva = 0;

	 /* Find maximum virtual address */
  for (i = 0; i < exeFormat->numSegments; ++i) {
    struct Exe_Segment *segment = &exeFormat->segmentList[i];
    ulong_t topva = segment->startAddress +segment->sizeInMemory; 
    if (topva > maxva)
      maxva = topva;
  }

 
	//obtener bloque de argumentos
	unsigned int argNum;
	ulong_t block_size;
	Get_Argument_Block_Size(command, &argNum, &block_size);

	//obtener el tamano de memoria necesaria, lo que usan los segmentos,
	//los argumentos y  2 paginas para el stack
	unsigned long Virtsize = Round_Up_To_Page(maxva) + Round_Up_To_Page(block_size + DEFAULT_USER_STACK_SIZE);
	// Crear el User_Context para el proceso
	struct User_Context *userContext = Create_User_Context(Virtsize);
	if (userContext == NULL) 
	{
		return 1;
	}
	



	// Copiar los segmentos de codigo y datos a memoria desde el archivo elf
	struct Exe_Segment *codeSegment = &(exeFormat->segmentList[0]);
	if(memcpy(userContext->memory + codeSegment->startAddress, exeFileData + codeSegment->offsetInFile, codeSegment->lengthInFile) == NULL)
	{
		Destroy_User_Context(userContext);
		return 1;
	}

	struct Exe_Segment *dataSegment = &(exeFormat->segmentList[1]);
	if(memcpy(userContext->memory + dataSegment->startAddress, exeFileData + dataSegment->offsetInFile, dataSegment->lengthInFile) == NULL)
	{
		Destroy_User_Context(userContext);
		return 1;
	}
	
	
	//formatear el bloque de argumentos, este va a estar ubicado justo a l final del ultimo segmento (datos)	
	userContext->argBlockAddr = maxva;
//
	Format_Argument_Block((char*)(userContext->memory + maxva), argNum, userContext->argBlockAddr, command);
	

	//Ubicar el puntero al stack al final del bloque de memoria solicitada
	userContext->stackPointerAddr = (ulong_t)(Virtsize-1);

	userContext->entryAddr = exeFormat->entryAddr;

	*pUserContext = userContext;
    return 0;
}

/*
 * Copy data from user memory into a kernel buffer.
 * Params:
 * destInKernel - address of kernel buffer
 * srcInUser - address of user buffer
 * bufSize - number of bytes to copy
 *
 * Returns:
 *   true if successful, false if user buffer is invalid (i.e.,
 *   doesn't correspond to memory the process has a right to
 *   access)
 */
bool Copy_From_User(void* destInKernel, ulong_t srcInUser, ulong_t bufSize)
{
    /*
     * Hints:
     * - the User_Context of the current process can be found
     *   from g_currentThread->userContext
     * - the user address is an index relative to the chunk
     *   of memory you allocated for it
     * - make sure the user buffer lies entirely in memory belonging
     *   to the process
     */
	struct Kernel_Thread* currentThread= Get_Current();	
	struct User_Context* threadUserContext = currentThread->userContext;
	if(Validate_User_Memory(threadUserContext,srcInUser,bufSize))
	{
		memcpy(destInKernel,threadUserContext->memory + srcInUser, bufSize); 
		return true;
	}
	return false;
}

/*
 * Copy data from kernel memory into a user buffer.
 * Params:
 * destInUser - address of user buffer
 * srcInKernel - address of kernel buffer
 * bufSize - number of bytes to copy
 *
 * Returns:
 *   true if successful, false if user buffer is invalid (i.e.,
 *   doesn't correspond to memory the process has a right to
 *   access)
 */
bool Copy_To_User(ulong_t destInUser, void* srcInKernel, ulong_t bufSize)
{
    /*
     * Hints: same as for Copy_From_User()
     */
	struct Kernel_Thread* currentThread= Get_Current();	
	struct User_Context* threadUserContext = currentThread->userContext;
	if(Validate_User_Memory(threadUserContext,destInUser,bufSize))
	{
		memcpy((void*)((ulong_t)threadUserContext->memory + destInUser), srcInKernel, bufSize); 
		return true;
	}
	return false;
}

/*
 * Switch to user address space belonging to given
 * User_Context object.
 * Params:
 * userContext - the User_Context
 */
void Switch_To_Address_Space(struct User_Context *userContext)
{
    /*
     * Hint: you will need to use the lldt assembly language instruction
     * to load the process's LDT by specifying its LDT selector.
     */
    
	ushort_t ldt_s = userContext->ldtSelector;
	KASSERT(ldt_s != 0);	
	__asm__ __volatile__ ("lldt %0" : : "r" (ldt_s));



}

