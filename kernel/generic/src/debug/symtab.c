/*
 * Copyright (c) 2005 Ondrej Palkovsky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup genericdebug
 * @{
 */

/**
 * @file
 * @brief Kernel symbol resolver.
 */

#include <symtab.h>
#include <byteorder.h>
#include <string.h>
#include <print.h>
#include <arch/types.h>
#include <typedefs.h>
#include <errno.h>

/** Get name of a symbol that seems most likely to correspond to address.
 *
 * @param addr Address.
 * @param name Place to store pointer to the symbol name.
 *
 * @return Zero on success or negative error code, ENOENT if not found,
 *         ENOTSUP if symbol table not available.
 *
 */
int symtab_name_lookup(unative_t addr, char **name)
{
#ifdef CONFIG_SYMTAB
	count_t i;
	
	for (i = 1; symbol_table[i].address_le; i++) {
		if (addr < uint64_t_le2host(symbol_table[i].address_le))
			break;
	}
	
	if (addr >= uint64_t_le2host(symbol_table[i - 1].address_le)) {
		*name = symbol_table[i - 1].symbol_name;
		return EOK;
	}
	
	*name = NULL;
	return ENOENT;
	
#else
	*name = NULL;
	return ENOTSUP;
#endif
}

/** Lookup symbol by address and format for display.
 *
 * Returns name of closest corresponding symbol, "Not found" if none exists
 * or "N/A" if no symbol information is available.
 *
 * @param addr Address.
 * @param name Place to store pointer to the symbol name.
 *
 * @return Pointer to a human-readable string.
 *
 */
char *symtab_fmt_name_lookup(unative_t addr)
{
	char *name;
	int rc = symtab_name_lookup(addr, &name);
	
	switch (rc) {
	case EOK:
		return name;
	case ENOENT:
		return "Not found";
	default:
		return "N/A";
	}
}

#ifdef CONFIG_SYMTAB

/** Find symbols that match the parameter forward and print them.
 *
 * @param name     Search string
 * @param startpos Starting position, changes to found position
 *
 * @return Pointer to the part of string that should be completed or NULL.
 *
 */
static const char *symtab_search_one(const char *name, count_t *startpos)
{
	count_t namelen = str_length(name);
	
	count_t pos;
	for (pos = *startpos; symbol_table[pos].address_le; pos++) {
		const char *curname = symbol_table[pos].symbol_name;
		
		/* Find a ':' in name */
		const char *colon = str_chr(curname, ':');
		if (colon == NULL)
			continue;
		
		if (str_length(curname) < namelen)
			continue;
		
		if (str_lcmp(name, curname, namelen) == 0) {
			*startpos = pos;
			return (curname + str_lsize(curname, namelen));
		}
	}
	
	return NULL;
}

#endif

/** Return address that corresponds to the entry.
 *
 * Search symbol table, and if there is one match, return it
 *
 * @param name Name of the symbol
 * @param addr Place to store symbol address
 *
 * @return Zero on success, ENOENT - not found, EOVERFLOW - duplicate
 *         symbol, ENOTSUP - no symbol information available.
 *
 */
int symtab_addr_lookup(const char *name, uintptr_t *addr)
{
#ifdef CONFIG_SYMTAB
	count_t found = 0;
	count_t pos = 0;
	const char *hint;
	
	while ((hint = symtab_search_one(name, &pos))) {
		if (str_length(hint) == 0) {
			*addr = uint64_t_le2host(symbol_table[pos].address_le);
			found++;
		}
		pos++;
	}
	
	if (found > 1)
		return EOVERFLOW;
	
	if (found < 1)
		return ENOENT;
	
	return EOK;
	
#else
	return ENOTSUP;
#endif
}

/** Find symbols that match parameter and print them */
void symtab_print_search(const char *name)
{
#ifdef CONFIG_SYMTAB
	count_t pos = 0;
	while (symtab_search_one(name, &pos)) {
		uintptr_t addr = uint64_t_le2host(symbol_table[pos].address_le);
		char *realname = symbol_table[pos].symbol_name;
		printf("%p: %s\n", addr, realname);
		pos++;
	}
	
#else
	printf("No symbol information available.\n");
#endif
}

/** Symtab completion
 *
 * @param input Search string, completes to symbol name
 * @param size  Input buffer size
 *
 * @return 0 - nothing found, 1 - success, >1 print duplicates
 *
 */
int symtab_compl(char *input, count_t size)
{
#ifdef CONFIG_SYMTAB
	const char *name = input;
	
	/* Allow completion of pointers */
	if ((name[0] == '*') || (name[0] == '&'))
		name++;
	
	/* Do not print all symbols */
	if (str_length(name) == 0)
		return 0;
	
	count_t found = 0;
	count_t pos = 0;
	const char *hint;
	char output[MAX_SYMBOL_NAME];
	
	output[0] = 0;
	
	while ((hint = symtab_search_one(name, &pos))) {
		if ((found == 0) || (str_length(output) > str_length(hint)))
			str_ncpy(output, hint, MAX_SYMBOL_NAME);
		
		pos++;
		found++;
	}
	
	if ((found > 1) && (str_length(output) != 0)) {
		printf("\n");
		pos = 0;
		while ((hint = symtab_search_one(name, &pos))) {
			printf("%s\n", symbol_table[pos].symbol_name);
			pos++;
		}
	}
	
	if (found > 0)
		str_ncpy(input, output, size);
	
	return found;
	
#else
	return 0;
#endif
}

/** @}
 */
