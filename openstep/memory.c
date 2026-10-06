/* Page-aligned executable code cache for the i386 dynamic CPU core. */
#include "host.h"
#include <mach/mach.h>
#include <limits.h>

void *OSHostExecutableAlloc(unsigned bytes)
{
    vm_address_t address = 0;
    vm_size_t size;
    kern_return_t result;
    if (!bytes || bytes > UINT_MAX - vm_page_size) return NULL;
    size = (bytes + vm_page_size - 1) & ~(vm_page_size - 1);
    result = vm_allocate(task_self(), &address, size, TRUE);
    if (result != KERN_SUCCESS) return NULL;
    result = vm_protect(task_self(), address, size, FALSE,
                        VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
    if (result != KERN_SUCCESS) {
        vm_deallocate(task_self(), address, size);
        return NULL;
    }
    return (void *)address;
}

void OSHostExecutableFree(void *memory, unsigned bytes)
{
    if (memory && bytes && bytes <= UINT_MAX - vm_page_size) {
        vm_size_t size = (bytes + vm_page_size - 1) & ~(vm_page_size - 1);
        vm_deallocate(task_self(), (vm_address_t)memory, size);
    }
}
