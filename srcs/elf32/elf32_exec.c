/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   elf32_exec.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: besellem <besellem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/04/26 20:22:06 by besellem          #+#    #+#             */
/*   Updated: 2022/05/04 22:26:20 by besellem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_nm.h"

static char		elf32_get_sym_type(const Elf32_Shdr *shdr, const Elf32_Sym sym)
{
	const unsigned char	_info = sym.st_info;
	Elf32_Word			_type = 0;
	Elf32_Word			_flags = 0;
	char				type;

	// avoid special section indexes (avoiding segfault by the way)
	// TODO: to check
	if (sym.st_shndx != SHN_UNDEF &&
		sym.st_shndx != SHN_LORESERVE &&
		sym.st_shndx != SHN_LOPROC &&
		sym.st_shndx != SHN_HIPROC &&
		sym.st_shndx != SHN_ABS &&
		sym.st_shndx != SHN_COMMON &&
		sym.st_shndx != SHN_HIRESERVE)
	{
		_type = shdr[sym.st_shndx].sh_type;
		_flags = shdr[sym.st_shndx].sh_flags;
	}

	if (ELF32_ST_BIND(_info) == STB_GNU_UNIQUE)
		type = 'u';
	else if (ELF32_ST_BIND(_info) == STB_WEAK)
		type = (SHN_UNDEF == sym.st_shndx) ? 'w' : 'W';
	else if (ELF32_ST_BIND(_info) == STB_WEAK && ELF32_ST_TYPE(_info) == STT_OBJECT)
		type = (SHN_UNDEF == sym.st_shndx) ? 'v' : 'V';
	else if (SHN_UNDEF == sym.st_shndx)
		type = 'U';
	else if (SHN_ABS == sym.st_shndx)
		type = 'A';
	else if (SHN_COMMON == sym.st_shndx)
		type = 'C';
	else if (SHT_NOBITS == _type && _flags == (SHF_ALLOC | SHF_WRITE))
		type = 'B';
	else if (SHT_PROGBITS == _type && (_flags == SHF_ALLOC || _flags == (SHF_ALLOC | SHF_MERGE)))
		type = 'R';
	else if ((SHT_PROGBITS == _type && _flags == (SHF_ALLOC | SHF_WRITE)) ||
			 SHT_DYNAMIC == _type || SHT_INIT_ARRAY == _type || SHT_FINI_ARRAY == _type ||
			 (ELF32_ST_BIND(_info) == STB_GLOBAL && STT_OBJECT == _type && SHN_UNDEF == sym.st_shndx))
		type = 'D';
	else if (SHT_PROGBITS == _type || SHT_INIT_ARRAY == _type || SHT_FINI_ARRAY == _type)
		type = 'T';
	else
		type = '?';
	
	if (ELF32_ST_BIND(_info) == STB_LOCAL)
		type = ft_tolower(type);
	
	return type;
}

static list_t	*elf32_get_symtab(const t_file *file, const Elf32_Shdr *shdr, size_t idx)
{
	const Elf32_Sym		*symtab = file->p + shdr[idx].sh_offset;
	const char			*strtab = file->p + shdr[shdr[idx].sh_link].sh_offset;
	list_t				*tab = NULL;

	// file truncated
	if ((size_t)file->st.st_size < shdr[idx].sh_offset ||
		(size_t)file->st.st_size < shdr[shdr[idx].sh_link].sh_offset)
	{
		return NULL;
	}

	for (size_t i = 0; i < (shdr[idx].sh_size / sizeof(Elf32_Sym)); ++i)
	{
		t_symbol	sym = {0};

		// -a option
		if (SHN_ABS == symtab[i].st_shndx && !option_set(g_opts.opts, OPT_A_MIN))
			continue ;

		sym.type = elf32_get_sym_type(shdr, symtab[i]);
		sym.name = (char *)strtab + symtab[i].st_name;
		sym.offset = symtab[i].st_value;

		// special error case
		if (STT_NOTYPE == ELF32_ST_TYPE(symtab[i].st_info) && '$' == *sym.name) // TODO: check
			continue ;

		// empty name
		if (!sym.name || !*sym.name)
			continue ;

		// -g option
		if (ft_islower(sym.type) && sym.type != 'w' && option_set(g_opts.opts, OPT_G_MIN))
			continue ;
		
		// -u option
		if (symtab[i].st_shndx != SHN_UNDEF && option_set(g_opts.opts, OPT_U_MIN))
			continue ;

		// -p option
		if (option_set(g_opts.opts, OPT_P_MIN))
		{
			lst_push_back(&tab, sym);
		}
		else
		{
			lst_push_sorted(&tab, sym, get_sorting_cmp());
		}
	}
	return tab;
}

void	elf32_exec(const t_file *file)
{
	const Elf32_Ehdr	*hdr = file->p;
	Elf32_Shdr			*shdr = file->p + hdr->e_shoff;
	size_t				symbols_nb = 0;
	list_t				*symtab;

	for (size_t i = 0; i < hdr->e_shnum; ++i)
	{
		if (SHT_SYMTAB == shdr[i].sh_type)
		{
			symtab = elf32_get_symtab(file, shdr, i);
			if (!symtab)
			{
				ft_dprintf(STDERR_FILENO, "%s: %s: file format not recognized\n",
					g_prog_name, file->filename);
				return ;
			}
			elf_display_list(file, symtab);
			lst_clear(&symtab, LST_CLEAR_IGN);
			++symbols_nb;
		}
	}

	if (0 == symbols_nb && hdr->e_shnum > 0)
	{
		ft_dprintf(STDERR_FILENO, "%s: %s: no symbols\n",
			g_prog_name, file->filename);
	}
}
