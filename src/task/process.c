#include "process.h"
#include "config.h"
#include "memory/memory.h"
#include "status.h"
#include "task/task.h"
#include "memory/heap/kheap.h"
#include "fs/file.h"
#include "string/string.h"
#include "memory/paging/paging.h"
#include "loader/formats/elfloader.h"
#include "kernel.h"
#include <stdbool.h>

struct process *current_process = 0;
static struct process *processes[PEACHOS_MAX_PROCESSES] = {0};

static void process_init(struct process *process)
{
    memset(process, 0, sizeof(struct process));
}

struct process *process_current()
{
    return current_process;
}

int process_get(int process_id, struct process **process)
{
    int res = 0;

    CHECK_ARG(process_id >= 0 && process_id < PEACHOS_MAX_PROCESSES);

    *process = processes[process_id];
out:
    return res;
}

int process_get_free_slot()
{
    for (int i = 0; i < PEACHOS_MAX_PROCESSES; i++)
    {
        if (processes[i] == 0)
        {
            return i;
        }
    }
    return -EISTKN;
}

static bool process_exists(int process_id)
{
    struct process *process;
    int res = process_get(process_id, &process);
    return res == 0 && process != 0;
}

int process_switch(struct process *process)
{
    current_process = process;
    return 0;
}

static int process_load_binary(const char *filename, struct process *process)
{
    int res = 0;
    int fd = fopen(filename, "r");

    CHECK(fd, -EIO);

    struct file_stat stat;

    CHECK_ERR(fstat(fd, &stat));

    void *program_data_ptr = kzalloc(stat.filesize);

    CHECK(program_data_ptr, -ENOMEM);
    CHECK_ERR(fread(program_data_ptr, stat.filesize, 1, fd));

    process->file_type = FILE_TYPE_BINARY;
    process->ptr = program_data_ptr;
    process->size = stat.filesize;

out:
    fclose(fd);

    return res;
}

static int process_load_elf(const char *filename, struct process *process)
{
    int res = 0;
    struct elf_file *elf_file = 0;

    CHECK_ERR(elf_load(filename, &elf_file));

    process->file_type = FILE_TYPE_ELF;
    process->elf_file = elf_file;

out:
    return res;
}

static int process_load_data(const char *filename, struct process *process)
{
    int res = 0;

    res = process_load_elf(filename, process);

    if (res == -EBADFORMAT)
    {
        res = process_load_binary(filename, process);
    }

    return res;
}

static int process_map_binary(struct process *process)
{
    void *phys_end = paging_align_address(process->ptr + process->size);

    return paging_map_to(
        process->task->page_directory,
        (void *)PEACHOS_PROGRAM_VIRTUAL_ADDRESS,
        process->ptr,
        phys_end,
        PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL | PAGING_IS_WRITEABLE);
}

static int process_map_elf(struct process *process)
{
    struct elf_file *elf_file = process->elf_file;
    struct elf_header *header = elf_header(elf_file);
    struct elf32_phdr *phdrs = elf_pheader(header);

    int res = 0;

    for (int i = 0; i < header->e_phnum && res == 0; i++)
    {
        struct elf32_phdr *phdr = &phdrs[i];
        void *phdr_phys_addr = elf_phdr_phys_addr(elf_file, phdr);
        int flags = PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL;

        if (phdr->p_flags & PF_W)
        {
            flags |= PAGING_IS_WRITEABLE;
        }

        res = paging_map_to(
            process->task->page_directory,
            paging_align_to_lower_page((void *)phdr->p_vaddr),
            phdr_phys_addr,
            paging_align_address(phdr_phys_addr + phdr->p_filesz),
            flags);
    }

    return res;
}

static int process_map_memory(struct process *process)
{
    int res = 0;

    switch (process->file_type)
    {
    case FILE_TYPE_ELF:
        CHECK_ERR(process_map_elf(process));
        break;
    case FILE_TYPE_BINARY:
        CHECK_ERR(process_map_binary(process));
        break;
    default:
        panic("process_map_memory unexpected file type");
        break;
    }

    void *phys_end = paging_align_address((void *)((int)process->stack + PEACHOS_USER_PROGRAM_STACK_SIZE));
    int flags = PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL | PAGING_IS_WRITEABLE;

    CHECK_ERR(paging_map_to(process->task->page_directory, (void *)PEACHOS_PROGRAM_VIRTUAL_STACK_ADDRESS_END, process->stack, phys_end, flags));

out:
    return res;
}

void process_free(struct process *process)
{
    if (process)
    {
        if (process->task)
        {
            task_free(process->task);
        }
        if (process->stack)
        {
            kfree(process->stack);
        }
        if (process->ptr)
        {
            kfree(process->ptr);
        }
        for (int i = 0, len = sizeof(process->allocations) / sizeof(process->allocations[0]); i < len; i++)
        {
            if (process->allocations[i])
            {
                kfree(process->allocations[i]);
            }
        }
        kfree(process);
    }
}

int process_load_switch(const char *filename, struct process **process)
{
    int res = process_load(filename, process);
    if (res == 0)
    {
        process_switch(*process);
    }
    return res;
}

int process_load_for_slot(const char *filename, struct process **process_out, int process_slot)
{
    int res = 0;
    struct process *process;

    CHECK(!process_exists(process_slot), -EISTKN);

    process = kzalloc(sizeof(struct process));

    CHECK(process, -ENOMEM);

    process_init(process);

    CHECK_ERR(process_load_data(filename, process));

    process->stack = kzalloc(PEACHOS_USER_PROGRAM_STACK_SIZE);

    CHECK(process->stack, -ENOMEM);

    NS(strncpy(process->filename, filename, sizeof(process->filename) - 1));
    process->id = process_slot;

    process->task = task_new(process);

    CHECK(process->task, -EIO);
    CHECK_ERR(process_map_memory(process));

    processes[process_slot] = process;
    *process_out = process;

out:
    if (res < 0)
    {
        process_free(process);
    }

    return res;
}

int process_load(const char *filename, struct process **process)
{
    int res = 0;
    int process_slot = process_get_free_slot();
    CHECK(process_slot >= 0, -ENOMEM);
    CHECK_ERR(process_load_for_slot(filename, process, process_slot));

out:
    return res;
}

static int process_find_free_allocation_index(struct process *process)
{
    for (int i = 0; i < PEACHOS_MAX_PROGRAM_ALLOCATIONS; i++)
    {
        if (process->allocations[i] == 0)
        {
            return i;
        }
    }
    return -ENOMEM;
}

void *process_malloc(struct process *process, size_t size)
{
    int i = process_find_free_allocation_index(process);

    if (i < 0)
    {
        return 0;
    }

    void *ptr = kzalloc(size);

    if (!ptr)
    {
        return 0;
    }

    process->allocations[i] = ptr;

    return ptr;
}

static int process_find_allocation_index(struct process *process, void *ptr)
{
    for (int i = 0; i < PEACHOS_MAX_PROGRAM_ALLOCATIONS; i++)
    {
        if (process->allocations[i] == ptr)
        {
            return i;
        }
    }
    return -1;
}

void process_free_allocation(struct process *process, void *ptr)
{
    if (!ptr)
    {
        return;
    }

    int i = process_find_allocation_index(process, ptr);

    if (i < 0)
    {
        return;
    }

    kfree(ptr);
    process->allocations[i] = 0;
}
