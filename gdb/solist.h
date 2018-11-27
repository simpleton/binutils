/* Shared library declarations for GDB, the GNU Debugger.
   Copyright (C) 1990-2016 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef SOLIST_H
#define SOLIST_H

#define SO_NAME_MAX_PATH_SIZE 512	/* FIXME: Should be dynamic */
/* For domain_enum domain.  */
#include "symtab.h"

/* Forward declaration for target specific link map information.  This
   struct is opaque to all but the target specific file.  */
struct lm_info;

struct so_list
  {
    /* The following fields of the structure come directly from the
       dynamic linker's tables in the inferior, and are initialized by
       current_sos.  */

    struct so_list *next;	/* next structure in linked list */

    /* A pointer to target specific link map information.  Often this
       will be a copy of struct link_map from the user process, but
       it need not be; it can be any collection of data needed to
       traverse the dynamic linker's data structures.  */
    struct lm_info *lm_info;

    /* Shared object file name, exactly as it appears in the
       inferior's link map.  This may be a relative path, or something
       which needs to be looked up in LD_LIBRARY_PATH, etc.  We use it
       to tell which entries in the inferior's dynamic linker's link
       map we've already loaded.  */
    char so_original_name[SO_NAME_MAX_PATH_SIZE];

    /* Shared object file name, expanded to something GDB can open.  */
    char so_name[SO_NAME_MAX_PATH_SIZE];

    /* Program space this shared library belongs to.  */
    struct program_space *pspace;

    /* The following fields of the structure are built from
       information gathered from the shared object file itself, and
       are set when we actually add it to our symbol tables.

       current_sos must initialize these fields to 0.  */

    bfd *abfd;
    char symbols_loaded;	/* flag: symbols read in yet?  */

    /* objfile with symbols for a loaded library.  Target memory is read from
       ABFD.  OBJFILE may be NULL either before symbols have been loaded, if
       the file cannot be found or after the command "nosharedlibrary".  */
    struct objfile *objfile;

    struct target_section *sections;
    struct target_section *sections_end;

    /* Record the range of addresses belonging to this shared library.
       There may not be just one (e.g. if two segments are relocated
       differently); but this is only used for "info sharedlibrary".  */
    CORE_ADDR addr_low, addr_high;
  };

struct so_search_hints {
  /* Which of the fields below are valid.  When adding new fields, do
     not forget to update extension interfaces that consume them.  */
  unsigned base_addr_valid : 1;
  unsigned l_ld_valid : 1;
  unsigned minidump_id_valid : 1;

  /* Our best guess as to the true base address of the module.  */
  CORE_ADDR base_addr;

  /* On SVR4-ish systems, the l_ld value from link_map.  Does not
     point to the start of the mapped module, but may provide a hint
     for looking up the base address via target mappings.  */
  CORE_ADDR l_ld;

  /* Caller-owned memory providing any explicit ID provided via a
     loaded minidump module list.  */
  struct {
    const void *bytes;
    size_t length;
  } minidump_id;

  /* Optional sequence information that extensions can use to
     display a user-friendly progress indication.  */
  size_t nr_so;
  size_t nr_so_total;
};

struct target_so_ops
  {
    /* Adjust the section binding addresses by the base address at
       which the object was actually mapped.  */
    void (*relocate_section_addresses) (struct so_list *so,
                                        struct target_section *);

    /* Free the link map info and any other private data structures
       associated with a so_list entry.  */
    void (*free_so) (struct so_list *so);

    /* Reset private data structures associated with SO.
       This is called when SO is about to be reloaded.
       It is also called before free_so when SO is about to be freed.  */
    void (*clear_so) (struct so_list *so);

    /* Reset or free private data structures not associated with
       so_list entries.  */
    void (*clear_solib) (void);

    /* Target dependent code to run after child process fork.  */
    void (*solib_create_inferior_hook) (int from_tty);

    /* Do additional symbol handling, lookup, etc. after symbols for a
       shared object have been loaded in the usual way.  This is
       called to do any system specific symbol handling that might be
       needed.  */
    void (*special_symbol_handling) (void);

    /* Construct a list of the currently loaded shared objects.  This
       list does not include an entry for the main executable file.

       Note that we only gather information directly available from the
       inferior --- we don't examine any of the shared library files
       themselves.  The declaration of `struct so_list' says which fields
       we provide values for.  */
    struct so_list *(*current_sos) (void);

    /* Find, open, and read the symbols for the main executable.  If
       FROM_TTYP dereferences to a non-zero integer, allow messages to
       be printed.  This parameter is a pointer rather than an int
       because open_symbol_file_object is called via catch_errors and
       catch_errors requires a pointer argument.  */
    int (*open_symbol_file_object) (void *from_ttyp);

    /* Determine if PC lies in the dynamic symbol resolution code of
       the run time loader.  */
    int (*in_dynsym_resolve_code) (CORE_ADDR pc);

    /* Find and open shared library binary file.
       Optional variant that accepts an so_list
       describing the file to load. */
    bfd *(*bfd_open2) (char *pathname,
                       const struct so_search_hints *hints);

    /* Optional extra hook for finding and opening a solib.
       If TEMP_PATHNAME is non-NULL: If the file is successfully opened a
       pointer to a malloc'd and realpath'd copy of SONAME is stored there,
       otherwise NULL is stored there.  */
    int (*find_and_open_solib) (char *soname,
        unsigned o_flags, char **temp_pathname);

    /* Hook for looking up global symbols in a library-specific way.  */
    struct block_symbol (*lookup_lib_global_symbol)
      (struct objfile *objfile,
       const char *name,
       const domain_enum domain);

    /* Given two so_list objects, one from the GDB thread list
       and another from the list returned by current_sos, return 1
       if they represent the same library.
       Falls back to using strcmp on so_original_name field when set
       to NULL.  */
    int (*same) (struct so_list *gdb, struct so_list *inferior);

    /* Return whether a region of memory must be kept in a core file
       for shared libraries loaded before "gcore" is used to be
       handled correctly when the core file is loaded.  This only
       applies when the section would otherwise not be kept in the
       core file (in particular, for readonly sections).  */
    int (*keep_data_in_core) (CORE_ADDR vaddr,
			      unsigned long size);

    /* Enable or disable optional solib event breakpoints as
       appropriate.  This should be called whenever
       stop_on_solib_events is changed.  This pointer can be
       NULL, in which case no enabling or disabling is necessary
       for this target.  */
    void (*update_breakpoints) (void);

    /* Target-specific processing of solib events that will be
       performed before solib_add is called.  This pointer can be
       NULL, in which case no specific preprocessing is necessary
       for this target.  */
    void (*handle_event) (void);

    /* Describe an otherwise-opaque lm_info structure
       for the benefit of extension solib-loading hooks.  */
    void (*describe_lm_info) (struct so_search_hints *hints,
                              struct lm_info *lm_info);

    /* Target-specific hook for exec_file_find.  */
    char *(*exec_file_find) (char *in_pathname, int *fd);
  };

/* Free the memory associated with a (so_list *).  */
void free_so (struct so_list *so);

/* Return address of first so_list entry in master shared object list.  */
struct so_list *master_so_list (void);

/* Find main executable binary file.  */
extern char *exec_file_find (char *in_pathname, int *fd);
/* Find main executable binary file, providing a location hint to the
   DSO-searching machinery.  */
extern char *exec_file_find2 (char *in_pathname,
                              int *fd,
                              const struct so_search_hints *hints);

/* Find shared library binary file.  */
extern char *solib_find (char *in_pathname,
                         const struct so_search_hints *hints,
                         int *fd);

/* Open BFD for shared library file.  */
extern bfd *solib_bfd_fopen (char *pathname, int fd);

/* Find solib binary file and open it, with
   additional context.  */
extern bfd *solib_bfd_open2 (char *in_pathname,
                             const struct so_search_hints *hints);

/* Handler for library-specific global symbol lookup in solib.c.  */
struct block_symbol solib_global_lookup (struct objfile *objfile,
					    const char *name,
					    const domain_enum domain);

#endif
